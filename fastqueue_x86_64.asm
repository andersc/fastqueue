section .text
bits 64

BUFFER_MASK         equ 15
L1_CACHE            equ 64
SHIFT_NO            equ ((L1_CACHE) / ((L1_CACHE) % 255 + 1) / 255 % 255 * 8 + 7 - 86 / ((L1_CACHE) % 255 + 12))

global push_item
global pop_item
global verify_mask
global verify_cache_size

verify_mask:
    mov rax,rdi
    sub rax, BUFFER_MASK
    ret

verify_cache_size:
    mov rax,rdi
    sub rax, L1_CACHE
    ret

push_item:
    mov r11, [rdi + (L1_CACHE * 1)] ;mWritePosition
push_loop:
    cmp [rdi + (L1_CACHE * 4)], byte 0 ;mExitThreadSemaphore
    jnz exit_loop
    mov rcx, r11
    sub rcx, [rdi + (L1_CACHE * 2)] ;mReadPosition
    cmp rcx, BUFFER_MASK
    jge push_loop
    mov rax, r11
    inc r11
    and rax, BUFFER_MASK
    shl rax, SHIFT_NO
    add rax, (L1_CACHE * 5) ;mRingBuffer
    mov [rdi + rax], rsi
    sfence
    mov [rdi + (L1_CACHE * 1)], r11 ;mWritePosition
exit_loop:
	ret

pop_item:
    mov rcx, [rdi + (L1_CACHE * 2)] ;mReadPosition
    cmp rcx, [rdi + (L1_CACHE * 1)] ;mWritePosition
    jne entry_found
    sub rcx, [rdi + (L1_CACHE * 3)] ;mExitThread (0 = true)
    jnz pop_item
    cmp [rdi + (L1_CACHE * 4)], byte 0  ;mExitThreadSemaphore (1 = true)
    jz  pop_item
    xor rax, rax
    ret
entry_found:
    mov r11, rcx
    inc r11
    and rcx, BUFFER_MASK
    shl rcx, SHIFT_NO
    add rcx, (L1_CACHE * 5) ;mRingBuffer
    mov rax, [rdi + rcx]
    lfence
    mov [rdi + (L1_CACHE * 2)], r11 ;mReadPosition
	ret

