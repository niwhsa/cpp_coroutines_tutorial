#include "async_fs_executor.hpp"
#include <chrono>
#include <random>

//Usage demo
void demoAsyncFS() {
    Executor::Config config;
    config.threadCount = 4;
    config.batchExecutorTaskBatchSize = 32;

    AsyncFSExecutor asyncFSExecutor(config);
    asyncFSExecutor.start();
    
    //Ex 1: Async file read
    auto future1 = asyncFSExecutor.readFileAsync("large_file.txt");
    auto size1 = future1.get();
    std::cout << "Read" << size1 << " bytes\n";
    
    //Ex 2: Async file write
    std::vector<char> data = { 'H', 'e', 'l', 'l', 'o'};
    auto future2 = asyncFSExecutor.writeFileAsync("output.txt", data);
    auto size2 = future2.get();
    std::cout << "Wrote " << size2 << " bytes\n";

    //Ex 3: Process dir async
    std::atomic<size_t> fileCount = 0;
    asyncFSExecutor.processDirAsync("./data", [&fileCount] (const auto& entry) {
        fileCount++;
        //Process file content
        std::cout << "Processing: " << entry.path() <<std::endl;
    });
    
    //Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Processed: " << fileCount << " files\n";

    asyncFSExecutor.stop();
}

void runFSExecutorBenchmark() {
    const size_t FILE_COUNT = 1000;
    const size_t FILE_SIZE = 1024 * 1024;

    // Create test files
    std::vector<std::filesystem::path> testFiles;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    // Prepare test data
    std::vector<std::vector<char>> testData;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        std::vector<char> data(FILE_SIZE);
        std::generate(data.begin(), data.end(), [&]() { return dis(gen); });
        testData.emplace_back(std::move(data));
        testFiles.emplace_back(std::filesystem::temp_directory_path() / ("test_" + std::to_string(i) + ".dat"));
    }
    
    // 1. Measure sync write performance
    auto syncWriteStart = std::chrono::high_resolution_clock::now();
    size_t totalBytesSyncWritten = 0;

    for (size_t i = 0; i < FILE_COUNT; ++i) {
        std::ofstream file(testFiles[i], std::ios::binary);
        file.write(testData[i].data(), testData[i].size());
        totalBytesSyncWritten += testData[i].size();
    }

    auto syncWriteEnd = std::chrono::high_resolution_clock::now();
    auto syncWriteDuration = std::chrono::duration_cast<std::chrono::milliseconds>(syncWriteEnd - syncWriteStart);

    // Setup async executor
    Executor::Config config;
    config.threadCount = std::thread::hardware_concurrency();
    AsyncFSExecutor executor(config);
    executor.start();
    
    // 2. Measure regular async read performance
    auto readStart = std::chrono::high_resolution_clock::now();
    std::vector<std::future<size_t>> readFutures;

    for (const auto& path : testFiles) {
        readFutures.emplace_back(executor.readFileAsync(path));
    }

    size_t totalBytesAsyncRead = 0;
    for (auto& future : readFutures) {
        totalBytesAsyncRead += future.get();
    }

    auto readEnd = std::chrono::high_resolution_clock::now();
    auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - readStart);

    // 3. Measure batch async read performance
    auto batchReadStart = std::chrono::high_resolution_clock::now();
    std::vector<std::future<size_t>> batchReadFutures;

    for (const auto& path : testFiles) {
        batchReadFutures.emplace_back(executor.readFileAsyncBatch(path));
    }

    size_t totalBytesBatchAsyncRead = 0;
    for (auto& future : batchReadFutures) {
        totalBytesBatchAsyncRead += future.get();
    }

    auto batchReadEnd = std::chrono::high_resolution_clock::now();
    auto batchReadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(batchReadEnd - batchReadStart);

    // 4. Clean up test files
    for (auto& path : testFiles) {
        std::filesystem::remove(path);
    }

    // 5. Measure async write performance
    auto asyncWriteStart = std::chrono::high_resolution_clock::now();
    std::vector<std::future<size_t>> writeFutures;

    for (size_t i = 0; i < FILE_COUNT; ++i) {
        writeFutures.emplace_back(executor.writeFileAsyncBatch(testFiles[i], testData[i]));
    }

    size_t totalBytesAsyncWritten = 0;
    for (auto& future : writeFutures) {
        totalBytesAsyncWritten += future.get();
    }

    auto asyncWriteEnd = std::chrono::high_resolution_clock::now();
    auto asyncWriteDuration = std::chrono::duration_cast<std::chrono::milliseconds>(asyncWriteEnd - asyncWriteStart);

    //Clean up test files
    for (const auto& path: testFiles) {
        std::filesystem::remove(path);
    }

    // Print results
    std::cout << "Async FS Benchmark Results:\n"
              << "\nSync Write Performance:\n"
              << "Files written: " << FILE_COUNT << "\n"
              << "Total bytes written: " << totalBytesSyncWritten << "\n"
              << "Write time: " << syncWriteDuration.count() << "ms\n"
              << "Write throughput: " << (totalBytesSyncWritten / 1024.0 / 1024.0) / (syncWriteDuration.count() / 1000.0) 
              << " MB/s\n"
              << "\nAsync Write Performance:\n"
              << "Files written: " << FILE_COUNT << "\n"
              << "Total bytes written: " << totalBytesAsyncWritten << "\n"
              << "Write time: " << asyncWriteDuration.count() << "ms\n"
              << "Write throughput: " << (totalBytesAsyncWritten / 1024.0 / 1024.0) / (asyncWriteDuration.count() / 1000.0) 
              << " MB/s\n"
              << "\nAsync Read Performance:\n"
              << "Files processed: " << FILE_COUNT << "\n"
              << "Total bytes read: " << totalBytesAsyncRead << "\n"
              << "Read time: " << readDuration.count() << "ms\n"
              << "Read throughput: " << (totalBytesAsyncRead / 1024.0 / 1024.0) / (readDuration.count() / 1000.0) 
              << " MB/s\n"
              << "\nAsync Batch Read Performance:\n"
              << "Files processed: " << FILE_COUNT << "\n"
              << "Total bytes read: " << totalBytesBatchAsyncRead << "\n"
              << "Read time: " << batchReadDuration.count() << "ms\n"
              << "Batch Read throughput: " << (totalBytesBatchAsyncRead / 1024.0 / 1024.0) / (batchReadDuration.count() / 1000.0) 
              << " MB/s\n";     

    executor.stop();
}
