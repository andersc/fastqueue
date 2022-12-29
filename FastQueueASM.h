#pragma once

#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <memory>
#ifdef _MSC_VER
#include <malloc.h>
#endif

//Remember to set the parameters in the ASM file as well if changed
#define BUFFER_MASK 15
#define L1_CACHE 64

namespace FastQueueASM {

    //ASM function declarations
    extern "C" {
    //param1 = pointer to the struct FastQueueASM::DataBlock
    //param2 = the pointer
    void push_item(void *, void *);
    //param1 = pointer to the struct FastQueueASM::DataBlock, returns the pointer, or NULL if last item is popped
    void *pop_item(void *);
    //param1 = the mask used by the C header file, returns 0 if the ASM queue and C buffer size match
    uint64_t verify_mask(uint64_t);
    //param1 = the cache size used by the C header file, returns 0 if the ASM cache size and C cache size setting match
    uint64_t verify_cache_size(uint64_t);
    }

    //The data block 'template' used by the queue
    //Block is set to 0 after initialised
    struct DataBlock {
        struct alignas(L1_CACHE) mAlign {
            void *mObj;
            volatile uint8_t mStuff[L1_CACHE - sizeof(void *)];
        };

        alignas(L1_CACHE) volatile uint8_t mBorderUpp[L1_CACHE];
        alignas(L1_CACHE) volatile uint64_t mWritePosition; //L1CACHE * 1
        alignas(L1_CACHE) volatile uint64_t mReadPosition;  //L1CACHE * 2
        alignas(L1_CACHE) volatile uint64_t mExitThread; //L1CACHE * 3
        alignas(L1_CACHE) volatile uint64_t mExitThreadSemaphore; //L1CACHE * 4
        alignas(L1_CACHE) volatile mAlign mRingBuffer[BUFFER_MASK + 1]; //L1CACHE * 5
        alignas(L1_CACHE) volatile uint8_t mBorderDown[L1_CACHE];
    };

    //Allocate an new queue
    DataBlock *newQueue() {

        //Verify the compiler generated data block
        static_assert(sizeof(DataBlock) == ((4 * L1_CACHE) + ((BUFFER_MASK + 1) * L1_CACHE) + (L1_CACHE * 2)),
                      "FastQueueASM::DataBlock is not matching expected size");
#ifdef _MSC_VER
        auto pData = (DataBlock *)_aligned_malloc(sizeof(DataBlock), L1_CACHE);
#else
        auto pData = (DataBlock *)std::aligned_alloc(L1_CACHE, sizeof(DataBlock));
#endif
        if (pData) std::memset((void *) pData, 0, sizeof(DataBlock));

        uint64_t lSource = BUFFER_MASK;
        uint64_t lContiguousBits = 0;
        while (true) {
            if (!(lSource & 1)) break;
            lSource = lSource >> 1;
            lContiguousBits++;
        }
        uint64_t lBitsSetTotal = std::bitset<64>(BUFFER_MASK).count();
        if (lContiguousBits != lBitsSetTotal || !lContiguousBits)
            throw std::runtime_error(
                    "Buffer size must be a number of contiguous bits set from LSB. Example: 0b00001111 not 0b01001111");
        if (verify_mask(BUFFER_MASK))
            throw std::runtime_error("the buffer size in fast queue ASM and C-header missmatch.");
        if (std::bitset<64>(L1_CACHE).count() != 1) throw std::runtime_error("L1_CACHE must be a 2 complement number ( 2pow(6) = 64 )");
        if (verify_cache_size(L1_CACHE))
            throw std::runtime_error("the cache size in fast queue ASM and C-header missmatch.");
        return pData;
    }

    //Free the memory of an allocated queue
    void deleteQueue(DataBlock *pData) {
#ifdef _MSC_VER
        _aligned_free(pData);
#else
        std::free(pData);
#endif
    }

    //Stop queue (Maybe called from any thread)
    void stopQueue(DataBlock *pData) {
        pData->mExitThread = pData->mWritePosition;
        pData->mExitThreadSemaphore = true;
    }

}
