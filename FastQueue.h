//
// Created by Anders Cedronius
//

// Usage

// Create the queue
// auto queue = FastQueue<Type, Size, L1-Cache size>
// Type of data
// Size of queue as a contiguous bitmask from LSB example 0b1111
// The ring buffer is acting as a rubber band between the
// producer/consumer to avoid unnecessary stalls when pushing new data.
// L1-Cache size typically 64 bytes

// queue.push is blocking if queue is full
// queue.stopQueue() or a popped entry will release the spinlock only.
// queue.push(object/pointer)

// queue.pop is blocking if the queue is empty
// queue.stopQueue() or a pushed entry will release the spinlock only.
// auto result = queue.pop();
// if result is {} this signals all objects are popped and the consumer should
// not pop any more data

// use tryPush and/or tryPop if you want to avoid spinlock CPU hogging for low
// frequency data transfer tryPush should be followed by pushAfterTry if used
// and tryPop should be followed by popAfterTry.

// Call queue.stopQueue() from any thread to signal end of transaction
// the user may drop the queue or pop the queue until {} is returned.

// Call queue.isQueueStopped() to see the status of the queue.
// May be used to manage the life cycle of the thread pushing data for example.


#pragma once

#include <iostream>
#include <cstdint>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <bitset>

#if __x86_64__ || _M_X64
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#elif __aarch64__ || _M_ARM64
#ifdef _MSC_VER
#include <arm64_neon.h>
#endif
#else
#error Arhitecture not supported
#endif

template<typename T, uint64_t RING_BUFFER_SIZE, uint64_t L1_CACHE_LNE>
class FastQueue {
public:

    enum class FastQueueMessages : uint64_t {
        END_OF_SERVICE,
        READY_TO_POP,
        NOT_READY_TO_POP,
        READY_TO_PUSH,
        NOT_READY_TO_PUSH,
    };

    explicit FastQueue() {
        uint64_t lSource = RING_BUFFER_SIZE;
        uint64_t lContiguousBits = 0;
        while (true) {
            if (!(lSource & 1)) break;
            lSource = lSource >> 1;
            lContiguousBits++;
        }

        uint64_t lBitsSetTotal = std::bitset<64>(RING_BUFFER_SIZE).count();
        if (lContiguousBits != lBitsSetTotal || !lContiguousBits) {
            throw std::runtime_error(
                    "Buffer size must be a number of contiguous bits set from LSB. Example: 0b00001111 not 0b01001111");
        }
        if ((uint64_t) &mWritePosition % 8 || (uint64_t) &mReadPosition % 8) {
            throw std::runtime_error("Queue-pointers are misaligned in memory.");
        }
    }

    ///////////////////////
    /// Push part
    ///////////////////////

    FastQueueMessages tryPush() {
        if (mWritePosition - mReadPosition >= RING_BUFFER_SIZE || mExitThreadSemaphore) {
            return FastQueueMessages::NOT_READY_TO_PUSH;
        }
        return FastQueueMessages::READY_TO_PUSH;
    }

    void pushAfterTry(T &rItem) {
        mRingBuffer[mWritePosition & RING_BUFFER_SIZE].mObj = std::move(rItem);
#if __x86_64__ || _M_X64
        _mm_sfence();
#elif __aarch64__ || _M_ARM64
#ifdef _MSC_VER
        __dmb(_ARM64_BARRIER_ISHST);
#else
        asm volatile("dmb ishst" : : : "memory");
#endif
#else
#error Architecture not supported
#endif
        mWritePosition++;
    }

     void push(T &rItem) noexcept {
        while (mWritePosition - mReadPosition >= RING_BUFFER_SIZE) {
            if (mExitThreadSemaphore) {
                return;
            }
        }
        mRingBuffer[mWritePosition & RING_BUFFER_SIZE].mObj = std::move(rItem);
#if __x86_64__ || _M_X64
        _mm_sfence();
#elif __aarch64__ || _M_ARM64
#ifdef _MSC_VER
        __dmb(_ARM64_BARRIER_ISHST);
#else
        asm volatile("dmb ishst" : : : "memory");
#endif
#else
#error Architecture not supported
#endif
         mWritePosition++;
    }

    ///////////////////////
    /// Pop part
    ///////////////////////

    FastQueueMessages tryPop() {
        if (mWritePosition == mReadPosition) {
            if ((mExitThread == mReadPosition) && mExitThreadSemaphore) {
                return FastQueueMessages::END_OF_SERVICE;
            }
            return FastQueueMessages::NOT_READY_TO_POP;
        }
        return FastQueueMessages::READY_TO_POP;
    }

    T popAfterTry() {
        T lData = std::move(mRingBuffer[mReadPosition & RING_BUFFER_SIZE].mObj);
#if __x86_64__ || _M_X64
        _mm_lfence();
#elif __aarch64__ || _M_ARM64
#ifdef _MSC_VER
        __dmb(_ARM64_BARRIER_ISHLD);
#else
        asm volatile("dmb ishld" : : : "memory");
#endif
#else
#error Architecture not supported
#endif
        mReadPosition++;
        return lData;
    }

     T pop() noexcept {
        while (mWritePosition == mReadPosition) {
            if ((mExitThread == mReadPosition) && mExitThreadSemaphore) {
                return {};
            }
        }
        T lData = std::move(mRingBuffer[mReadPosition & RING_BUFFER_SIZE].mObj);
#if __x86_64__ || _M_X64
         _mm_lfence();
#elif __aarch64__ || _M_ARM64
#ifdef _MSC_VER
         __dmb(_ARM64_BARRIER_ISHLD);
#else
         asm volatile("dmb ishld" : : : "memory");
#endif
#else
#error Architecture not supported
#endif
        mReadPosition++;
        return lData;
    }

    //Stop queue (Maybe called from any thread)
    void stopQueue() {
        mExitThread = mWritePosition;
        mExitThreadSemaphore = true;
    }

    //Is the queue stopped?
    bool isQueueStopped() {
        return mExitThreadSemaphore;
    }

    ///Delete copy and move constructors and assign operators
    FastQueue(FastQueue const &) = delete;              // Copy construct
    FastQueue(FastQueue &&) = delete;                   // Move construct
    FastQueue &operator=(FastQueue const &) = delete;   // Copy assign
    FastQueue &operator=(FastQueue &&) = delete;        // Move assign
private:
    struct alignas(L1_CACHE_LNE) mAlign {
        T mObj;
        volatile uint8_t mStuff[L1_CACHE_LNE - sizeof(T)];
    };

    alignas(L1_CACHE_LNE) volatile uint8_t mBorderUpp[L1_CACHE_LNE];
    alignas(L1_CACHE_LNE) volatile uint64_t mWritePosition = 0;
    alignas(L1_CACHE_LNE) volatile uint64_t mReadPosition = 0;
    alignas(L1_CACHE_LNE) volatile uint64_t mExitThread = 0;
    alignas(L1_CACHE_LNE) volatile bool mExitThreadSemaphore = false;
    alignas(L1_CACHE_LNE) mAlign mRingBuffer[RING_BUFFER_SIZE + 1];
    alignas(L1_CACHE_LNE) volatile uint8_t mBorderDown[L1_CACHE_LNE];
};

