#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "micro-lisp.h"

void print_lisp_value(LISP_VALUE *val, int nest_level, int has_items_following) 
{
    switch (val->value_bits & TYPE_BITMASK)
    {
        case V_INT:
            printf("%d", val->intnum);
            break;
        case V_SYMBOL:
            printf("%s", val->symbol);
            break;
        case V_CONS_CELL:
            printf("(");
            while (IS_TYPE(val, V_CONS_CELL)) {
                print_lisp_value(val->car, nest_level + 1,
                                 !IS_TYPE(val->cdr, V_NIL));
                val = val->cdr;
            }
            if (!IS_TYPE(val, V_NIL)) {
                printf(". ");
                print_lisp_value(val, nest_level + 1, 0);
            }
            printf(")");
            break;
        case V_CLOSURE:
            printf("#<CLOSURE: %p, %p>", val->code, val->env);
            break;
        case V_NIL:
            if (nest_level > 0) {
                printf("()");
            } else {
                printf("nil");
            }
            break;
        default:
            printf("Unknown lisp type.\n");
            exit(0);
            break;
    }
    if (has_items_following) {
        printf(" ");
    }
}

char current_char = '\n';
int nest_level = 0;

void next_char(void)
{
    if ('\n' == current_char) {
        while ('\n' == current_char) {
            if (nest_level > 0) {
                printf("%d", nest_level);
            }
            printf(">");
            current_char = getchar();
        }
    } else {
        current_char = getchar();
    }
}

void skip_blanks(void)
{
    while (IS_WHITESPACE(current_char)) {
        next_char();
    }
}

LISP_VALUE *read_symbol(void)
{
    int n = 0;
    LISP_VALUE *ret = malloc(sizeof(LISP_VALUE));
    ret->value_bits = V_SYMBOL;
    while (IS_ALPHANUM(current_char)) {
        if (n < SYM_SIZE - 1) {
            ret->symbol[n++] = current_char;
            ret->symbol[n + 1] = '\0';
        }
        next_char();
    }
    if (STREQ(ret->symbol, "nil")) {
        ret->value_bits = V_NIL;
    }
    return ret;
}

LISP_VALUE *read_intnum(void)
{
    int n = 0;
    LISP_VALUE *ret = malloc(sizeof(LISP_VALUE));
    int sign = '-' == current_char ? -1 : 1;
    if ('+' == current_char || '-' == current_char) {
        next_char();
    }
    if (!IS_DIGIT(current_char)) {
        printf("Error reading integer.\n");
        exit(0);
    }
    while (IS_DIGIT(current_char)) {
        n = n*10 + (current_char - '0');
        next_char();
    }
    ret->value_bits = V_INT;
    ret->intnum = n*sign;
    return ret; 
}    

LISP_VALUE *read_lisp_value(void)
{
    skip_blanks();
    if (isalpha(current_char)) {
        return read_symbol();
    } else if ('(' == current_char) {
        return read_list();
    } else if ('+' == current_char || '-' == current_char || 
               IS_DIGIT(current_char)) {
        return read_intnum();
        
    } else {
        printf("Read error.\n");
        exit(0);
    }
    return NULL;
}

LISP_VALUE *read_list(void)
{
    LISP_VALUE *ret = malloc(sizeof(LISP_VALUE));
    LISP_VALUE *left, *right, *curr;
    nest_level += 1;
    next_char();  // skip '('
    skip_blanks();
    curr = ret;
    curr->value_bits = V_CONS_CELL;
    while (')' != current_char) {
        left = read_lisp_value();
        skip_blanks();
        curr->car = left;
        right = malloc(sizeof(LISP_VALUE));
        right->value_bits = V_CONS_CELL;
        curr->cdr = right;
        curr = right;
    }
    next_char();   // skip ')'
    curr->value_bits = V_NIL;
    nest_level -= 1;
    return ret;
}

LISP_VALUE mem[MAX_VALUES];
LISP_VALUE *free_list_head = NULL;
LISP_VALUE *protect_stack[MAX_PROTECTED];
int protect_stack_ptr = 0;

void mark(void) 
{
    int i;
    for (i = 0; i < MAX_VALUES; ++i) {
        // Zero out marked bit.  Zero mark bit means "collect"
        mem[i].value_bits &= TYPE_BITMASK;
    }
}  

void gc_walk(LISP_VALUE *v)
{
    
        
void sweep(void) 
{
    int i;
    for (i = 0; i < MAX_PROTECTED; i++) {
        gc_walk(protect_stack[i]);
    }
}

void collect(void) 
{
    int i;
    LISP_VALUE *last = NULL;
    free_list_head = NULL;
    for (i = 0; i < MAX_VALUES; i++) {
        if (mem[i].value_bits & ~TYPE_BITMASK) {
            if (NULL == free_list_head) {
                free_list_head = &mem[i];
                free_list_head->next_free = NULL;
                last = free_list_head;
            } else {
                last->next_free = &mem[i];
                last->next_free->next_free = NULL;
                last = last->next_free;
            }
        }
    }
}

void init_free_list(void)
{
    int i;
    free_list_head = &mem[0];
    for (i = 1; i < MAX_VALUES - 1; ++i) {
        mem[i - 1].next_free = &mem[i];
    }
    mem[MAX_VALUES - 1].next_free = NULL;
}

LISP_VALUE *new_value(int value_type)
{
    LISP_VALUE *ret = free_list_head;
    if (NULL == free_list_head) {
        gc();
        if (NULL == free_list_head) {
            printf("Memory overflow.\n");
            exit(0);
        }
        ret = free_list_head;
    }
    free_list_head = free_list_head->next_free;
    ret->value_bits = value_type;
    return ret;
}

void protect_from_gc(LISP_VALUE *v)
{
    protect_stack[protect_stack_ptr++] = v;
}

void unprotect_from_gc(void) 
{
    protect_stack_ptr -= 1;
}

int main(int argc, char **argv)
{
    LISP_VALUE *x;
    for (;;) {
        x = read_lisp_value();
        printf("Value=");
        print_lisp_value(x, 0, 0);
        printf("\n");
    }
    exit(0);
}




