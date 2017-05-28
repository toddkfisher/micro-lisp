; Tiny lisp interpreter as an experiment in assembly coding.

UNALLOC_CELL_FLAG equ 1

NUM_CELL_FLAG equ 2
    struc NUM_CELL ; 8 bytes
.flags  resd 1
.fixnum resd 1
    endstruc

CONS_CELL_FLAG equ 4
    struc CONS_CELL ; 24 bytes
.flags   resd 1
         align 8
.p_car   resq 1
.p_cdr   resq 1
    endstruc

SYM_CELL_FLAG equ 8
    struc SYM_CELL ; 16 bytes
.flags  resd 1
        align 8
.p_sym  resq 1
    endstruc

CLO_CELL_FLAG equ 16
    struc CLO_CELL ; 24 bytes
.flags  resd 1
        align 8
.p_env  resq 1
.p_code resq 1
    endstruc

N_CELLS equ 5 ; available memory size for lisp
CELL_SIZE equ 24 ;CONS_CELL_size ; CONS_CELL is largest

;;------------------------------------------------------------------------------
    section .data
msg db "Micro Lisp. %d available cells.", 0x0a, 0
nil dq 0
raw_unalloc_fmt db "Cell @ %p, Unallocated, next =%p", 0x0a, 0
raw_num_fmt db "Cell @ %p, Number = %d", 0x0a, 0
raw_cons_fmt db "Cell @ %p, Cons, car = %p, cdr = %p", 0x0a, 0
raw_sym_fmt db  "Cell @ %p, Symbol, ptr = %p", 0x0a, 0
raw_clo_fmt db  "Cell @ %p, Closure, env = %p, code = %p", 0x0a, 0
freelist_head dq 0
n_available_cells dd 0

    section .bss
cell_array resb N_CELLS*CELL_SIZE

;;------------------------------------------------------------------------------
    section .text
    global main
    extern printf

;;------------------------------------------------------------------------------
print_raw_cell:  ; (int cell_index)
    ; rbx = cell_array[di].flags
    ;xor rax, rax
    mov eax, edi
    imul eax, edi, CELL_SIZE
    lea rax, [cell_array + eax + CONS_CELL.flags]
    mov rsi, rax ; arg2 to printf() (pointer to current cell)
    mov rbx, [rax]
    ; setup for printf()
    push rbp
    mov rbp, rsp
    ;----------
    and rbx, UNALLOC_CELL_FLAG
    jz .print_raw_cell0
    lea rdi, [raw_unalloc_fmt]
    lea rdx, [cell_array + rax + CONS_CELL.p_cdr]
    jmp .print_raw_cell5
.print_raw_cell0:
    ;----------
    and rbx, NUM_CELL_FLAG
    jz .print_raw_cell1
    lea rdi, [raw_num_fmt]
    mov rdx, [cell_array + rax + NUM_CELL.fixnum]
    jmp .print_raw_cell5
.print_raw_cell1:
    ;----------
    and rbx, CONS_CELL_FLAG
    jz .print_raw_cell2
.print_raw_cell2:
    ;----------
    and rbx, CLO_CELL_FLAG
    jz .print_raw_cell3
.print_raw_cell3:
    ;----------
    and rbx, SYM_CELL_FLAG
    jz .print_raw_cell4
.print_raw_cell4:
    ;----------
.print_raw_cell5:
    pop rbp
    ret

;;------------------------------------------------------------------------------
print_freelist:
    mov ecx, 0
.print_freelist0:
    cmp ecx, N_CELLS - 1
    jz .print_freelist1
    mov edi, ecx
    push rbp
    mov rbp, rsp
    call print_raw_cell
    pop rbp
    inc ecx
    jmp .print_freelist0
.print_freelist1:
    ret

;;------------------------------------------------------------------------------
init_freelist:
    mov rax, 1
    mov [n_available_cells], rax
    mov rsi, 0 ; rsi is index into cell array
.init_freelist0:
    cmp rsi, N_CELLS - 1
    jz .init_freelist1
    ; cell_array[rsi].cdr = &cell_array[rsi + 1]
    mov rax, rsi
    imul rax, CELL_SIZE
    mov rcx, rax
    lea rax, [cell_array + rax + CONS_CELL.p_cdr]
    mov rbx, rsi
    inc rbx
    imul rbx, CELL_SIZE
    lea rbx, [cell_array + rbx]
    mov [rax], rbx
    ; cell_array[rsi].flags = UNALLOC_CELL_FLAG
    mov ebx, UNALLOC_CELL_FLAG
    mov dword [cell_array + rcx + CONS_CELL.flags], ebx
    ; next cell
    inc rsi
    ; n_available_cells += 1
    mov rax, [n_available_cells]
    inc rax
    mov [n_available_cells], rax
    jmp .init_freelist0
.init_freelist1:
    ; cell_array[N_CELLS - 1].cdr = &nil
    mov rax, rsi
    imul rax, CELL_SIZE
    lea rax, [cell_array + rax + CONS_CELL.p_cdr]
    lea rbx, [nil]
    mov [rax], rbx
    ret

;;------------------------------------------------------------------------------
;;get_cell:

;;------------------------------------------------------------------------------
main:
    call init_freelist
    push rbp
    mov rbp, rsp
    lea rdi, [msg]
    mov rsi, [n_available_cells]
    xor eax, eax ; 0 floating point params
    call printf
    xor eax, eax ; return 0
    pop rbp
    push rbp
    mov rbp, rsp
    call print_freelist
    pop rbp
    ret
