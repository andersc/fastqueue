![Logo](fastqueuesmall.png)


FastQueue is a single producer single consumer (SPSC) 'process to process queue' similar to *boost::lockfree::spsc_queue*

FastQueue is sligtely faster than the boost implementation (tested on a handfull systems and architectures) and is not as strict as boost is when it comes to data-types it can carry, it's possible to transfer smart pointers for example.

FastQueue is what's called a lock-free queue. However there must always be some sort of locks to prevent race conditions when two asychronous workers communicate. Many SPSC solutions use atomics to guard the data. FastQueue uses a memory barrier teqnique and limit it's usage to 64-bit platforms for cross thread variable data consistency.    

FastQueue can be pictured as illustrated below:

```
                                 FastQueue                                 
                   ◀───────────────────────────────────▶                   
                                .─────────.                                
                             ┌────┐        '─.                             
                           ,'│Data│       ┌────┐                           
 CPU 1 / Thread 1         ╱  └────┴──────.│Data│╲          CPU 2 / Thread 2      
◀───────────────▶        ╱     ,'         └────┘ ╲        ◀───────────────▶
┌───────────────┐  Push ┌────┐╱             ╲     : Pop   ┌───────────────┐
│ Data producer │ ─────▶│Data│Circular buffer:    │─────▶ │ Data consumer │
└───────────────┘       ├────┘               ┌────┤       └───────────────┘
                        :     ╲             ╱│Data│                        
                         ╲     ╲           ╱ └────┘                        
                          ╲ ┌────┐       ,'     ╱                          
                           ╲│Data│`─────┌────┐ ╱                           
                            └────┘      │Data│'                            
                              '─.       └────┘                             
                                 `───────'                                 
```

FastQueue is aiming to be fast. When it comes to measuring performance using templated code the compiler may optimize the final solution in ways where FastQueue might be slower than alternative solutions and/or change it's performance depending on other factors such as queue depth and so on. If you're aiming for speed it might be wise to benchmark FastQueue against other SPSC queues in your implementation, match L1_CACHE size to the executing CPU and tune the queue depth to match the data flow.

For example in my tests [Rigtorps SPSC](https://github.com/rigtorp/SPSCQueue) queue is really fast on ARM64.

**Apple M1 Pro**

```
boost lock free pointer test started.
boost lock free pointer test ended.
BoostLockFree Transactions -> 8437017/s
FastQueue pointer test started.
FastQueue pointer test ended.
FastQueue Transactions -> 9886604/s
Rigtorp pointer test started.
Rigtorp pointer test ended.
Rigtorp Transactions -> 10974382/s
FastQueueASM pointer test started.
FastQueueASM pointer test ended.
FastQueueASM Transactions -> 9471164/s
```

However on X64 platforms I don't see the same gain in my benchmarks. With that said Rigtorps queue is really the one to beat ;-) .


The queue is a header only template class and is implemented in a few lines of C++.

The code compiles on arm64 or x86_64 CPU's running Windows, MacOS or Linux OS.

There is also an pure Assembly version *FastQueueASM.h* that I've been playing around with. FastQueueASM is a bit more difficult to build compared to just dropping in the FastQueue.h into your project. Just look in the CMake file for guidance if you want to test it.

## Build status



## Usage

Copy the *FastQueue.h* file to your project and ->

```cpp

#include "FastQueue.h"

//Create the queue
//In this example a unique pointer to a vector of uint8_t
auto fastQueue = FastQueue<std::unique_ptr<std::vector<uint8_t>>, QUEUE_MASK>();

//Then the producer of the data pushes the data from one thread
auto dataProduced = std::make_unique<std::vector<uint8_t>>(1000);
fastQueue.push(dataProduced);

//And the consumer pops the data in another thread    
auto dataConsume = fastQueue.pop();

//When done signal that from anywhere (can be a third thread)
fastQueue.stopQueue();

//The consumer/producer may stop immediately or pop the queue
//until nullptr (in the above example) is received to drain all pushed items. 


```

For more examples see the included implementations and tests.


## License

*MIT*

Read *LICENCE* for details
