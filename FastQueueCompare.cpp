//
// Created by Anders Cedronius on 2022-10-10.
//

// speed-test.
// FastQueue, boost::lockfree, FastQueueASM and Rigtorps SPSC queue
// 1. Generate the data
// 2. Stamp something unique
// 3. Push the data through the queue
// 4. The receiver pops the data in another thread
// 5. Checks the data for the expected value
// 6. Garbage collects the data.

#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <iostream>
#include <thread>
#include "PinToCPU.h"
#include "FastQueue.h"
#include "SPSCQueue.h"
#include "FastQueueASM.h"

#define QUEUE_MASK 0b1111
#define L1_CACHE_LINE 64
#define TEST_TIME_DURATION_SEC 20
//Run the consumer on CPU
#define CONSUMER_CPU 0
//Run the producer on CPU
#define PRODUCER_CPU 2

std::atomic<uint64_t> gActiveConsumer = 0;
std::atomic<uint64_t> gCounter = 0;
bool gStartBench = false;
bool gActiveProducer = true;

class MyObject {
public:
    uint64_t mIndex;
};

/// -----------------------------------------------------------
///
/// Boost queue section Start
///
/// -----------------------------------------------------------

void boostLockFreeProducer(boost::lockfree::spsc_queue<MyObject *, boost::lockfree::capacity<QUEUE_MASK>> *pQueue,
                           int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        return;
    }
    while (!gStartBench) {
#ifdef _MSC_VER
        __nop();
#else
        asm volatile ("NOP");
#endif
    }
    uint64_t lCounter = 0;
    while (gActiveProducer) {
        auto lTheObject = new MyObject();
        lTheObject->mIndex = lCounter++;
        while (!pQueue->push(lTheObject) && gActiveProducer) {
        };
    }
}

void boostLockFreeConsumer(boost::lockfree::spsc_queue<MyObject *, boost::lockfree::capacity<QUEUE_MASK>> *pQueue,
                           int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        gActiveConsumer--;
        return;
    }
    uint64_t lCounter = 0;
    MyObject *lpMyObject;
    while (gActiveProducer) {
        while (pQueue->pop(lpMyObject)) {
            if (lpMyObject->mIndex != lCounter) {
                std::cout << "Queue item error" << std::endl;
            }
            delete lpMyObject;
            lCounter++;
        }
    }
    gCounter += lCounter;
    gActiveConsumer--;
}

/// -----------------------------------------------------------
///
/// Boost queue section End
///
/// -----------------------------------------------------------

/// -----------------------------------------------------------
///
/// FastQueue section Start
///
/// -----------------------------------------------------------

void fastQueueProducer(FastQueue<MyObject *, QUEUE_MASK, L1_CACHE_LINE> *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        return;
    }
    while (!gStartBench) {
#ifdef _MSC_VER
        __nop();
#else
        asm volatile ("NOP");
#endif
    }
    uint64_t lCounter = 0;
    while (gActiveProducer) {
        auto lTheObject = new MyObject();
        lTheObject->mIndex = lCounter++;
        pQueue->push(lTheObject);
    }
    pQueue->stopQueue();
}

void fastQueueConsumer(FastQueue<MyObject *, QUEUE_MASK, L1_CACHE_LINE> *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        gActiveConsumer--;
        return;
    }
    uint64_t lCounter = 0;
    while (true) {
        auto lResult = pQueue->pop();
        if (lResult == nullptr) {
            break;
        }
        if (lResult->mIndex != lCounter) {
            std::cout << "Queue item error" << std::endl;
        }
        lCounter++;
        delete lResult;
    }
    gCounter += lCounter;
    gActiveConsumer--;
}

/// -----------------------------------------------------------
///
/// FastQueue section End
///
/// -----------------------------------------------------------

/// -----------------------------------------------------------
///
/// Rigtorp section Start
///
/// -----------------------------------------------------------


void rigtorpQueueProducer(rigtorp::SPSCQueue<MyObject *> *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        return;
    }
    while (!gStartBench) {
#ifdef _MSC_VER
        __nop();
#else
        asm volatile ("NOP");
#endif
    }
    uint64_t lCounter = 0;
    while (gActiveProducer) {
        auto lTheObject = new MyObject();
        lTheObject->mIndex = lCounter++;
        pQueue->push(lTheObject);
    }
    pQueue->push(nullptr);
}

void rigtorpQueueConsumer(rigtorp::SPSCQueue<MyObject *> *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        gActiveConsumer--;
        return;
    }
    uint64_t lCounter = 0;
    while (true) {
        while (!pQueue->front());
        auto lResult = *pQueue->front();
        if (lResult == nullptr) {
            break;
        }
        pQueue->pop();
        if (lResult->mIndex != lCounter) {
            std::cout << "Queue item error" << std::endl;
        }
        lCounter++;
        delete lResult;
    }
    gCounter += lCounter;
    gActiveConsumer--;
}

/// -----------------------------------------------------------
///
/// Rigtorp section End
///
/// -----------------------------------------------------------

/// -----------------------------------------------------------
///
/// FastQueueASM section Start
///
/// -----------------------------------------------------------

void fastQueueASMProducer(FastQueueASM::DataBlock *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        return;
    }
    while (!gStartBench) {
#ifdef _MSC_VER
        __nop();
#else
        asm volatile ("NOP");
#endif
    }
    uint64_t lCounter = 0;
    while (gActiveProducer) {
        auto lTheObject = new MyObject();
        lTheObject->mIndex = lCounter++;
        FastQueueASM::push_item(pQueue, lTheObject);
    }
    stopQueue(pQueue);
}

void fastQueueASMConsumer(FastQueueASM::DataBlock *pQueue, int32_t aCPU) {
    if (!pinThread(aCPU)) {
        std::cout << "Pin CPU fail. " << std::endl;
        gActiveConsumer--;
        return;
    }
    uint64_t lCounter = 0;
    while (true) {
        auto lResult = (MyObject*)FastQueueASM::pop_item(pQueue);
        if (lResult == nullptr) {
            break;
        }
        if (lResult->mIndex != lCounter) {
            std::cout << "Queue item error " << lResult->mIndex << " " << lCounter  << std::endl;
        }
        delete lResult;
        lCounter++;
    }
    gCounter += lCounter;
    gActiveConsumer--;
}


/// -----------------------------------------------------------
///
/// FastQueueASM section End
///
/// -----------------------------------------------------------

int main() {

    ///
    /// BoostLockfree test ->
    ///

    // Create the queue
    auto lBoostLockFree = new boost::lockfree::spsc_queue<MyObject *, boost::lockfree::capacity<QUEUE_MASK>>;

    // Start the consumer(s) / Producer(s)
    gActiveConsumer++;
    std::thread([lBoostLockFree] { return boostLockFreeConsumer(lBoostLockFree, CONSUMER_CPU); }).detach();
    std::thread([lBoostLockFree] { return boostLockFreeProducer(lBoostLockFree, PRODUCER_CPU); }).detach();

    // Wait for the OS to actually get it done.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Start the test
    std::cout << "BoostLockFree pointer test started." << std::endl;
    gStartBench = true;
    std::this_thread::sleep_for(std::chrono::seconds(TEST_TIME_DURATION_SEC));

    // End the test
    gActiveProducer = false;
    std::cout << "BoostLockFree pointer test ended." << std::endl;

    // Wait for the consumers to 'join'
    // Why not the classic join? I prepared for a multi thread case I need this function for.
    while (gActiveConsumer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Garbage collect the queue
    delete lBoostLockFree;

    // Print the result.
    std::cout << "BoostLockFree Transactions -> " << gCounter / TEST_TIME_DURATION_SEC << "/s" << std::endl;

    // Zero the test parameters.
    gStartBench = false;
    gActiveProducer = true;
    gCounter = 0;
    gActiveConsumer = 0;


    ///
    /// FastQueue test ->
    ///

    // Create the queue
    auto lFastQueue = new FastQueue<MyObject *, QUEUE_MASK, L1_CACHE_LINE>();

    // Start the consumer(s) / Producer(s)
    gActiveConsumer++;
    std::thread([lFastQueue] { return fastQueueConsumer(lFastQueue, CONSUMER_CPU); }).detach();
    std::thread([lFastQueue] { return fastQueueProducer(lFastQueue, PRODUCER_CPU); }).detach();

    // Wait for the OS to actually get it done.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Start the test
    std::cout << "FastQueue pointer test started." << std::endl;
    gStartBench = true;
    std::this_thread::sleep_for(std::chrono::seconds(TEST_TIME_DURATION_SEC));

    // End the test
    gActiveProducer = false;
    std::cout << "FastQueue pointer test ended." << std::endl;

    // Wait for the consumers to 'join'
    // Why not the classic join? I prepared for a multi thread case I need this function for.
    while (gActiveConsumer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Garbage collect the queue
    delete lFastQueue;

    // Print the result.
    std::cout << "FastQueue Transactions -> " << gCounter / TEST_TIME_DURATION_SEC << "/s" << std::endl;

    // Zero the test parameters.
    gStartBench = false;
    gActiveProducer = true;
    gCounter = 0;
    gActiveConsumer = 0;

    ///
    /// Erik Rigtorp SPSC test ->
    ///

    // Create the queue
    auto lRigtorpSPSCQueue = new rigtorp::SPSCQueue<MyObject *>(QUEUE_MASK);

    // Start the consumer(s) / Producer(s)
    gActiveConsumer++;
    std::thread([lRigtorpSPSCQueue] { return rigtorpQueueConsumer(lRigtorpSPSCQueue, CONSUMER_CPU); }).detach();
    std::thread([lRigtorpSPSCQueue] { return rigtorpQueueProducer(lRigtorpSPSCQueue, PRODUCER_CPU); }).detach();

    // Wait for the OS to actually get it done.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Start the test
    std::cout << "Rigtorp pointer test started." << std::endl;
    gStartBench = true;
    std::this_thread::sleep_for(std::chrono::seconds(TEST_TIME_DURATION_SEC));

    // End the test
    gActiveProducer = false;
    std::cout << "Rigtorp pointer test ended." << std::endl;

    // Wait for the consumers to 'join'
    // Why not the classic join? I prepared for a multi thread case I need this function for.
    while (gActiveConsumer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Garbage collect the queue
    delete lRigtorpSPSCQueue;

    // Print the result.
    std::cout << "Rigtorp Transactions -> " << gCounter / TEST_TIME_DURATION_SEC << "/s" << std::endl;

    // Zero the test parameters.
    gStartBench = false;
    gActiveProducer = true;
    gCounter = 0;
    gActiveConsumer = 0;

    ///
    /// FastQueueASM test ->
    ///

    // Create the queue
    auto pQueue = FastQueueASM::newQueue();

    // Start the consumer(s) / Producer(s)
    gActiveConsumer++;

    std::thread([pQueue] {fastQueueASMConsumer(pQueue, CONSUMER_CPU);}).detach();
    std::thread([pQueue] {fastQueueASMProducer(pQueue, PRODUCER_CPU);}).detach();

    // Wait for the OS to actually get it done.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start the test
    std::cout << "FastQueueASM pointer test started." << std::endl;
    gStartBench = true;
    std::this_thread::sleep_for(std::chrono::seconds(TEST_TIME_DURATION_SEC));

    // End the test
    gActiveProducer = false;
    std::cout << "FastQueueASM pointer test ended." << std::endl;

    // Wait for the consumers to 'join'
    // Why not the classic join? I prepared for a multi thread case I need this function for.
    while (gActiveConsumer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Garbage collect the queue
    deleteQueue(pQueue);

    // Print the result.
    std::cout << "FastQueueASM Transactions -> " << gCounter / TEST_TIME_DURATION_SEC << "/s" << std::endl;

    return EXIT_SUCCESS;
}
