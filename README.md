![Logo](fastqueuesmall.png)


FastQueue is a single producer single consumer (SPSC) 'process to process queue' similar to *boost::lockfree::spsc_queue*

FastQueue is slightly faster than the boost implementation (tested on a handful systems and architectures) and is not as strict as boost is when it comes to data-types it can carry, it's possible to transfer smart pointers for example.

FastQueue is what's called a lock-free queue. However, there must always be some sort of lock to prevent race conditions when two asynchronous workers communicate. Many SPSC solutions use atomics to guard the data. FastQueue uses a memory barrier technique and limit it's usage to 64-bit platforms for cross thread variable data consistency.    

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

FastQueue is aiming to be fast. When it comes to measuring performance using templated code the compiler may optimize the final solution in ways where FastQueue might change its performance depending on code changes not related to the queue. If you're aiming for speed, it might be wise to benchmark FastQueue against other SPSC queues in your live implementation, match L1_CACHE size to the executing CPU and tune the queue depth to match the data flow avoiding for example hitting the limit too often.

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

However, on X64 platforms I don't see the same gain in my benchmarks. With that said Rigtorps queue is really the one to beat ;-) .


The queue is a header only template class and is implemented in a few lines of C++.

The code compiles on arm64 or x86_64 CPU's running Windows, MacOS or Linux OS.

There is also a pure Assembly version *FastQueueASM.h* that I've been playing around with (not 100% tested). FastQueueASM is a bit more difficult to build compared to just dropping in the FastQueue.h into your project. Just look in the CMake file for guidance if you want to test it.

## Build

Build the integrity test by:

```
	cmake -DCMAKE_BUILD_TYPE=Release .
	cmake --build .
```
The integrity test is sending and consuming data at an irregular rate. The data is verified for consistency when consumed. The test executes for 200 seconds and will create race conditions where the queue is full, drained and when data is consumed at the same time data is put on the queue. 

Build the integrity and queue benchmark by:

```
	cmake -DCMAKE_BUILD_TYPE=Release -DUSE_BOOST=ON .
	cmake --build .
```

This requires BOOST and MASM. See the github actions for how to install deps. on the respective platforms.

The benchmark spins up one consumer thread on a CPU and a consumer thread on another CPU. Then sends as many objects between the two as possible. The data is displayed when done for each test.

For accurate benchmarks there is C-States, nice factors or running as 'rt' and so on and so on. People write about how to test stuff all the time. My benchmark is maybe indicative, or maybe not.
 

Current build status ->

**ARM64**

[![mac_arm64](https://github.com/andersc/fastqueue/actions/workflows/mac_arm64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/mac_arm64.yml)

[![ubuntu_arm64](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_arm64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_arm64.yml)

[![win_arm64](https://github.com/andersc/fastqueue/actions/workflows/win_arm64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/win_arm64.yml)

**X86_64**

[![mac_x64](https://github.com/andersc/fastqueue/actions/workflows/mac_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/mac_x64.yml)

[![ubuntu_x64](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_x64.yml)

[![win_x64](https://github.com/andersc/fastqueue/actions/workflows/win_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/win_x64.yml)

**Builds with -DUSE_BOOST=ON**

Github do not provide ARM64 runners. The above ARM64 compiles are cross compiled. Cross compiling BOOST is about as booring as looking at drying paint. So I have not created actions for all systems and architectures. But here is the one I made.

**BOOST_ASM_ARM64**

[![mac_boost_arm64](https://github.com/andersc/fastqueue/actions/workflows/mac_boost_arm64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/mac_boost_arm64.yml)

**BOOST_ASM_X86_64**

[![mac_boost_x64](https://github.com/andersc/fastqueue/actions/workflows/mac_boost_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/mac_boost_x64.yml)

[![ubuntu_boost_x64](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_boost_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/ubuntu_boost_x64.yml)

[![win_boost_x64](https://github.com/andersc/fastqueue/actions/workflows/win_boost_x64.yml/badge.svg)](https://github.com/andersc/fastqueue/actions/workflows/win_boost_x64.yml)
  

## Usage

Copy the *FastQueue.h* file to your project and ->

```cpp

#include "FastQueue.h"

//Create the queue
//In this example a unique pointer to a vector of uint8_t
//QUEUE_MASK == size of queue as a contiguous bitmask from LSB example 0b1111
//L1_CACHE_LINE == the size of the L1 cache, usually 64
auto fastQueue = FastQueue<std::unique_ptr<std::vector<uint8_t>>, QUEUE_MASK, L1_CACHE_LINE>();

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

If the producer and / or consumer irregularly consumes or produces data it might be wise to use the **tryPush** / **pushAfterTry** and **tryPop** / **popAfterTry**. This to avoid spending excessive amount of CPU time in spinlocks. Using the tryPush/Pop you may sleep or do other things while waiting for data to consume or free queue slots to put data in.  


For more examples see the included implementations and tests.

## Final words

Please steal the PinToCPU.h header it's a cross platform CPU affinity tool. And while you're at it, add support for more than 64 CPU's on Windows platforms (I'm obviously not a Windows person ;-) ).

Have fun and please let me know if you find any quirks bugs or 'ooopses'.

## License

*MIT*

Read *LICENCE* for details
