In this Chapter, we will add the following features to the event framework solution in Chapter 7:
* Add batch executor to enable task batching for better cache locality.
* Benchmark and compare the performance of regular and batch executors.

```
*  Sample Output

➜  build git:(chapter-9) ✗ ./event_scheduler          

Testing Regular Executor...
Regular Executor completed 1000000 out of 1000000 tasks in 3549ms

Testing Batch Executor...
Batch Executor completed 1000000 out of 1000000 tasks in 3108ms
Starting executor...
Registering handlers...
Registering handler for: user_login
Registering handler for: system_status
Registering handler for: new_message

Starting benchmark with 1000 iterations...
User logged in: jack_smith (Latency: 105146 microseconds)
System status changed: 1 (Latency: 105144 microseconds)
New message received: Hello, cpp20 coroutines world! (Latency: 105137 microseconds)

Emission Time Statistics (microseconds):
Average: 25.255 microseconds
Median: 9 microseconds
95th percentile: 93 microseconds
99th percentile: 416 microseconds
Sample size: 1000 events
Stopping executor...
```

