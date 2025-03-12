In this Chapter, we will add the following features to the event framework solution in Chapter 9:
* Intergrate `boost::asio` to build a file system i/o extension to the BatchExecutor to test i/o workload
* Provide async file system read, write and batch read & write options.
* Benchmark `AsyncFSExecutor` and compare the performance of regular and batch executors.

```
*  Sample Output
          
➜  build git:(chapter-10) ✗ ./event_scheduler

=== CPU-Bound Task Benchmarks ===

Testing Regular Executor...
Regular Executor completed 1000000 out of 1000000 tasks in 5786ms

Testing Batch Executor (batch size: 8)...
Batch Executor (batch size: 8) completed 1000000 out of 1000000 tasks in 4756ms

Testing Batch Executor (batch size: 16)...
Batch Executor (batch size: 16) completed 1000000 out of 1000000 tasks in 5382ms

Testing Batch Executor (batch size: 32)...
Batch Executor (batch size: 32) completed 1000000 out of 1000000 tasks in 4567ms

Testing Batch Executor (batch size: 64)...
Batch Executor (batch size: 64) completed 1000000 out of 1000000 tasks in 4887ms

Testing Batch Executor (batch size: 128)...
Batch Executor (batch size: 128) completed 1000000 out of 1000000 tasks in 4687ms

Testing Batch Executor (batch size: 256)...
Batch Executor (batch size: 256) completed 1000000 out of 1000000 tasks in 4425ms

=== I/O-Bound Task Benchmarks ===
Async FS Benchmark Results:

Sync Write Performance:
Files written: 1000
Total bytes written: 1048576000
Write time: 1843ms
Write throughput: 542.594 MB/s

Async Write Performance:
Files written: 1000
Total bytes written: 1048576000
Write time: 1864ms
Write throughput: 536.481 MB/s

Async Read Performance:
Files processed: 1000
Total bytes read: 1048576000
Read time: 6535ms
Read throughput: 153.022 MB/s

Async Batch Read Performance:
Files processed: 1000
Total bytes read: 1048576000
Read time: 6542ms
Batch Read throughput: 152.858 MB/s

=== Event System Benchmarks ===
Starting executor...
Registering handlers...
Registering handler for: new_message
Registering handler for: system_status
Registering handler for: user_login

Starting benchmark with 1000 iterations...
User logged in: jack_smith (Latency: 105162 microseconds)
New message received: Hello, cpp20 coroutines world! (Latency: System status changed: 105203 microseconds)
1 (Latency: 105206 microseconds)

Emission Time Statistics (microseconds):
Average: 10.778 microseconds
Median: 7 microseconds
95th percentile: 25 microseconds
99th percentile: 65 microseconds
Sample size: 1000 events
Stopping executor...
```

