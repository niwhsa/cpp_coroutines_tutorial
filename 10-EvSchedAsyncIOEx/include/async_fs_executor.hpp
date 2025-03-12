#pragma once
#include "batch_executor.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <boost/asio.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class AsyncFSExecutor : public BatchExecutor {
  private:
    static constexpr size_t WRITE_BUFFER_SIZE = 1024 * 64;
    static constexpr size_t READ_BUFFER_SIZE = 1024 * 1024; // 1 MB for reads
    boost::asio::io_context _io_context;
    boost::asio::thread_pool _io_pool;

    struct FileOp {
        std::filesystem::path path;
        std::vector<char> buffer;
        std::promise<size_t> result;

        FileOp(const std::filesystem::path& p, size_t bufferSize) 
          : path(p), buffer(bufferSize) {} 
    };

    struct WriteBatch {
        static constexpr size_t BATCH_SIZE = 8;
        std::vector<std::shared_ptr<FileOp>> ops;
        
        void add(std::shared_ptr<FileOp> op) {
            ops.emplace_back(op);
        }

        bool full() const { return ops.size() >= BATCH_SIZE; }

        void execute() {
            for (auto& op : ops) {
                try {
                    std::ofstream file(op->path, std::ios::binary);
                    file.write(op->buffer.data(), op->buffer.size());
                    op->result.set_value(op->buffer.size());
                } catch (const std::exception& e) {
                    op->result.set_exception(std::current_exception());
                }
            }
        }
    };

    struct ReadBatch {
        static constexpr size_t BATCH_SIZE = 32;
        std::vector<std::shared_ptr<FileOp>> ops;
        
        void add(std::shared_ptr<FileOp> op) {
            ops.emplace_back(op);
        }

        bool full() const { return ops.size() >= BATCH_SIZE; }

        void execute(boost::asio::thread_pool& pool) {
            std::vector<std::shared_ptr<FileOp>> pending_ops = ops;
            std::atomic<size_t> completed{0};

            for (auto& op : pending_ops) {
                boost::asio::post(pool, [op, &completed]() {
                    try {
                        std::ifstream file(op->path, std::ios::binary);
                        size_t bytes = file.read(op->buffer.data(), READ_BUFFER_SIZE).gcount();
                        op->result.set_value(bytes);
                    } catch(const std::exception& e) {
                        op->result.set_exception(std::current_exception());
                    }
                    completed++;
                });
            }

            while (completed < pending_ops.size()) {
                std::this_thread::yield();
            }
        }

        void execute_mmap(boost::asio::thread_pool& pool) {
            std::vector<std::shared_ptr<FileOp>> pending_ops = ops;
            std::atomic<size_t> completed{0};

            for (auto& op : pending_ops) {
                boost::asio::post(pool, [op, &completed]() {
                    try {
                        //Open file for mem mapping
                        int fd = open(op->path.c_str(), O_RDONLY);
                        if (fd == -1) throw std::runtime_error("Failed to open file");

                        //Get file sz
                        struct stat sb;
                        if (fstat(fd, &sb) == -1) {
                            close(fd);
                            throw std::runtime_error("Failed to get file size");
                        }

                        //Map file into memory
                        void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                        if (mapped == MAP_FAILED) {
                            close(fd);
                            throw std::runtime_error("Failed to map file");
                        }

                        //Copy data to buffer
                        op->buffer.resize(sb.st_size);
                        std::memcpy(op->buffer.data(), mapped, sb.st_size);

                        //clean up
                        munmap(mapped, sb.st_size);
                        close(fd);

                        op->result.set_value(sb.st_size);
                    } catch (std::exception& e) {
                        op->result.set_exception(std::current_exception());
                    }
                    completed++;
                });
            }

            while (completed < pending_ops.size()) {
                std::this_thread::yield();
            }
        }            
    };

  public:
    AsyncFSExecutor(const Config& config) 
      : BatchExecutor(config),
        _io_pool(config.threadCount) {}
    
    //Async file read operation
    std::future<size_t> readFileAsync(const std::filesystem::path& path) {
        auto op = std::make_shared<FileOp>(path, READ_BUFFER_SIZE);
        auto future = op->result.get_future();

        auto task = [this, op]() {
            boost::asio::post(_io_pool, [op]() {
                try {
                    std::ifstream file(op->path, std::ios::binary);
                    size_t bytes = file.read(op->buffer.data(), READ_BUFFER_SIZE).gcount();
                    op->result.set_value(bytes);
                } catch (const std::exception& e) {
                    op->result.set_exception(std::current_exception());
                }
            });
        };

        schedule(Task(std::move(task)));
        return future;
    } 

    //Async file write operation
    std::future<size_t> writeFileAsync(
        const std::filesystem::path& path,
        const std::vector<char>& data) {
        
        auto op = std::make_shared<FileOp>(path, WRITE_BUFFER_SIZE);
        op->buffer = data;
        auto future = op->result.get_future();
        
        auto task = [this, op]() {
            boost::asio::post(_io_pool, [op]() {
                try {
                    std::ofstream file(op->path, std::ios::binary);
                    file.write(op->buffer.data(), op->buffer.size());
                    op->result.set_value(op->buffer.size());
                } catch (const std::exception& e) {
                    op->result.set_exception(std::current_exception());
                }
            });
        };

        schedule(Task(std::move(task)));
        return future;
    }
    
    // Async batch write
     std::future<size_t> writeFileAsyncBatch(
        const std::filesystem::path& path,
        const std::vector<char>& data) {
        
        static thread_local WriteBatch batch;
        
        auto op = std::make_shared<FileOp>(path, WRITE_BUFFER_SIZE);
        op->buffer = data;
        auto future = op->result.get_future();
        
        batch.add(op);
        
        if (batch.full() || op == batch.ops.back()) {
            auto currentBatch = std::move(batch);
            batch = WriteBatch();
            
            auto task = [this, b = std::move(currentBatch)]() mutable {
                boost::asio::post(_io_pool, [b = std::move(b)]() mutable {
                    b.execute();
                });
            };
            schedule(Task(std::move(task)));
        }
        return future;
    }

    //Async batch read
    std::future<size_t> readFileAsyncBatch(const std::filesystem::path& path) {
        static thread_local ReadBatch batch;

        auto op = std::make_shared<FileOp>(path, READ_BUFFER_SIZE);
        auto future = op->result.get_future();
        batch.add(op);

        if (batch.full() ||  op == batch.ops.back()) {
            auto currentBatch = std::move(batch);
            batch = ReadBatch();

            auto task = [this, b = std::move(currentBatch)]() mutable {
                     b.execute(_io_pool);
            };
            schedule(Task(std::move(task)));
        }
        return future;
    }

    //Directory traversal with async processing
    template<typename Func>
    void processDirAsync(const std::filesystem::path& dirPath, Func processor) {
        auto task  = [this, dirPath, processor] () {
            try {
                for(const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                    if (entry.is_regular_file()) {
                        auto process_task = [this, entry, processor]() {
                            boost::asio::post(_io_pool, [entry, processor]() {
                                processor(entry);    
                            });
                        };
                        schedule(Task(std::move(process_task)));
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Dir processing error: " << e.what() << std::endl;
            }
        };

        schedule(Task(std::move(task)));
    }

};
