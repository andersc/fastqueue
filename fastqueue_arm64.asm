; Created by Anders Cedronius

.text
.align 6
.global _push_item
.global _pop_item
.global _verify_mask
.global _verify_cache_size

.equ BUFFER_MASK, 15
.equ L1_CACHE, 64
.equ SHIFT_NO, ((L1_CACHE) / ((L1_CACHE) % 255 + 1) / 255 % 255 * 8 + 7 - 86 / ((L1_CACHE) % 255 + 12))

_pop_item:
    mov x3, x0
    ldr x1, [x0, #L1_CACHE * 4] ;mReadPositionPop
pop_loop:
    ldr x2, [x3, #L1_CACHE * 3] ;mWritePositionPop
    cmp x1,x2
    bne entry_found
    ldr x4, [x3, #L1_CACHE * 5] ;mExitThread
    cmp x4, x1
    bne pop_loop
    ldr x5, [x3, #L1_CACHE * 6] ;mExitThreadSemaphore (1 = true)
    cmp x5, #0
    beq pop_loop
    eor x0, x0, x0
    ret
entry_found:
    add x2, x1, #1
    and x1, x1, BUFFER_MASK
    lsl x1, x1, SHIFT_NO
    add x1, x1, #L1_CACHE * 7 ;mRingBuffer
    ldr x0, [x3, x1]
    dmb ishld
    str x2, [x3, #L1_CACHE * 4] ;mReadPositionPop
    str x2, [x3, #L1_CACHE * 2] ;mReadPositionPush
    ret

_push_item:
    ldr x2, [x0, #L1_CACHE * 1] ;mWritePositionPush
push_loop:
    ldr x3, [x0, #L1_CACHE * 6] ;mExitThreadSemaphore (1 = true)
    cmp x3, #0
    bne exit_loop
    ldr x4, [x0, #L1_CACHE * 2] ;mReadPositionPush
    sub x3, x2, x4
    cmp x3, BUFFER_MASK
    bge push_loop
    mov x3, x2
    add x2, x2, #1
    and x3, x3, BUFFER_MASK
    lsl x3, x3, SHIFT_NO
    add x3, x3, #L1_CACHE * 7 ;mRingBuffer
    str x1,[x0, x3]
    dmb ishst
    str x2,[x0, #L1_CACHE * 1] ;mWritePositionPush
    str x2,[x0, #L1_CACHE * 3] ;mWritePositionPop
exit_loop:
    ret

_verify_mask:
    sub x0, x0, BUFFER_MASK
    ret

_verify_cache_size:
    sub x0, x0, L1_CACHE
    ret