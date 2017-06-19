#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "micro-lisp.h"

// enum to string map
char *value_names[] = {
    "V_INT",
    "V_SYMBOL",
    "V_CONS_CELL",
    "V_CLOSURE",
    "V_NIL",
    "V_UNALLOCATED"
};

// Array of values.  Everything gets stored in one (or more) of these.
LISP_VALUE mem[MAX_VALUES];

// Free list of cells from mem[].
LISP_VALUE *free_list_head = NULL;

// Counter for reporting.
int n_free_values = 0;

// Stack of cells in mem[] for which may not be collected during GC.
// Ancestors to values on protect_stack[] are also saved from collection.
// Note that the global environment is also protected.
LISP_VALUE *protect_stack[MAX_PROTECTED];
int protect_stack_ptr = 0;

// Current input character not yet processed.
char current_char = '\n';

// Nesting level of lists.
int nest_level = 0;

// Global environment.  Gets special treatment since everything points to
// it.
LISP_VALUE *global_env;

//------------------------------------------------------------------------------
// Utility functions

void error(char *msg)
{
    fprintf(stderr, "ERROR: %s\n", msg);
}

void fatal(char *msg)
{
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

LISP_VALUE *set_car(LISP_VALUE *x,
                    LISP_VALUE *y)
{
    return x->car = y;
}

LISP_VALUE *set_cadr(LISP_VALUE *x,
                     LISP_VALUE *y)
{
    return x->car->cdr = y;
}

int sym_eq(LISP_VALUE *x,
           LISP_VALUE *y)
{
    return STREQ(x->symbol, y->symbol);
}

LISP_VALUE *cons(LISP_VALUE *x,
                 LISP_VALUE *y)
{
    LISP_VALUE *res = new_value(V_CONS_CELL);
    res->car = x;
    res->cdr = y;
    return res;
}

LISP_VALUE *car(LISP_VALUE *x)
{
    return x->car;
}

LISP_VALUE *cdr(LISP_VALUE *x)
{
    return x->cdr;
}

LISP_VALUE *cadr(LISP_VALUE *x)
{
    return x->cdr->car;
}

//------------------------------------------------------------------------------
// Read/write routines

void print_lisp_value_aux(LISP_VALUE *val,
                          int nest_level,
                          int has_items_following)
{
    switch (val->value_type)
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
                print_lisp_value_aux(val->car, nest_level + 1,
                                     !IS_TYPE(val->cdr, V_NIL));
                val = val->cdr;
            }
            if (!IS_TYPE(val, V_NIL)) {
                printf(". ");
                print_lisp_value_aux(val, nest_level + 1, 0);
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
            fatal("Unknown lisp type.\n");
            break;
    }
    if (has_items_following) {
        printf(" ");
    }
}

void print_lisp_value(LISP_VALUE *val,
                      int print_newline)
{
    print_lisp_value_aux(val, 0, 0);
    if (print_newline) {
        printf("\n");
    }
}

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
    LISP_VALUE *ret = new_value(V_SYMBOL);
    while (IS_ALPHANUM(current_char)) {
        if (n < SYM_SIZE - 1) {
            ret->symbol[n] = current_char;
            ret->symbol[++n] = '\0';
        }
        next_char();
    }
    if (STREQ(ret->symbol, "nil")) {
        ret->value_type = V_NIL;
    }
    return ret;
}

LISP_VALUE *read_intnum(void)
{
    int n = 0;
    LISP_VALUE *ret = new_value(V_INT);
    int sign = '-' == current_char ? -1 : 1;
    if ('+' == current_char || '-' == current_char) {
        next_char();
    }
    if (!IS_DIGIT(current_char)) {
        error("Error reading integer.\n");
    }
    while (IS_DIGIT(current_char)) {
        n = n*10 + (current_char - '0');
        next_char();
    }
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
        error("Read error.\n");
    }
    return NULL;
}

LISP_VALUE *read_list(void)
{
    LISP_VALUE *ret = new_value(V_CONS_CELL);
    LISP_VALUE *left, *right, *curr;
    protect_from_gc(ret);
    nest_level += 1;
    next_char();  // skip '('
    skip_blanks();
    curr = ret;
    curr->value_type = V_CONS_CELL;
    while (')' != current_char) {
        left = read_lisp_value();
        skip_blanks();
        curr->car = left;
        right = new_value(V_CONS_CELL);
        curr->cdr = right;
        curr = right;
    }
    next_char();   // skip ')'
    curr->value_type = V_NIL;
    nest_level -= 1;
    unprotect_from_gc();
    return ret;
}

//------------------------------------------------------------------------------
// Memory/gc-related routines.

void dump_protect_stack(void)
{
    int i;
    printf("---Protect stack:\n");
    for (i = 0; i < protect_stack_ptr; ++i) {
        printf("%02d : slot #%ld, addr %p type %s\n", i, protect_stack[i] - mem,
               protect_stack[i], value_names[protect_stack[i]->value_type]);
        if (IS_ATOM(protect_stack[i])) {
            printf("value = ");
            print_lisp_value(protect_stack[i], 1);
        }
    }
    printf("---\n");
}

void gc(void)
{
    printf("Garbage collecting...\n");
    mark();
    dump_protect_stack();
    sweep();
    collect();
    printf("Available values/cons cells = %d\n", n_free_values);
}

void mark(void)
{
    int i;
    printf("Marking gc_mark = 0 on all values...");
    for (i = 0; i < MAX_VALUES; ++i) {
        // Zero out marked bit.  Zero mark bit means "collect"
        mem[i].gc_mark = 0;
    }
    printf("marked %d values/cons cells.\n", i);
}

void sweep(void)
{
    int i;
    printf("Walking global_env.\n");
    gc_walk(global_env, 0);
    printf("Walking protect_stack[].  Size == %d.\n", protect_stack_ptr);
    for (i = 0; i < protect_stack_ptr; i++) {
        printf("Walking protect_stack[%d] ==%p.\n", i, protect_stack[i]);
        gc_walk(protect_stack[i], 0);
    }
}

void indent(int n)
{
    while (n-- > 0) {
        printf("    ");
    }
}

void gc_walk(LISP_VALUE *v,
             int depth)
{
    if (NULL != v) {
        indent(depth);
        printf("gc_walk(slot == %ld, ptr == %p) : ", v - mem, v);
        if (!v->gc_mark) {
            printf("not visited - to be saved.\n");
            v->gc_mark = 1;
            indent(depth);
            printf("type == %s", value_names[v->value_type]);
            switch (v->value_type) {
                case V_INT:
                case V_SYMBOL:
                case V_NIL:
                    break;
                case V_CONS_CELL:
                    indent(depth);
                    printf("walking car.\n");
                    gc_walk(v->car, depth + 1);
                    printf("\n");
                    indent(depth);
                    printf("walking cdr.\n");
                    gc_walk(v->cdr, depth + 1);
                    break;
                case V_CLOSURE:
                    indent(depth);
                    printf("walking env.\n");
                    gc_walk(v->env, depth + 1);
                    printf("\n");
                    indent(depth);
                    printf("walking code.\n");
                    gc_walk(v->code, depth + 1);
                    break;
                case V_UNALLOCATED:
                    fatal("gc_walk() on V_UNALLOCATED.\n");
                    break;
            }
        } else {
            printf("visited.\n");
        }
    }
}

void collect(void)
{
    int i;
    LISP_VALUE *last = NULL;
    free_list_head = NULL;
    n_free_values = 0;
    for (i = 0; i < MAX_VALUES; i++) {
        if (!mem[i].gc_mark) {
            n_free_values += 1;
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

void protect_from_gc(LISP_VALUE *v)
{
    printf("protect_from_gc %s\n", value_names[v->value_type]);
    protect_stack[protect_stack_ptr++] = v;
}

void unprotect_from_gc(void)
{
    printf("unprotect_from_gc %s\n",
           value_names[protect_stack[protect_stack_ptr - 1]->value_type]);
    protect_stack_ptr -= 1;
}

void init_free_list(void)
{
    int i;
    LISP_VALUE *v;
    free_list_head = &mem[0];
    for (i = 1; i < MAX_VALUES; ++i) {
        mem[i - 1].next_free = &mem[i];
        mem[i - 1].value_type = V_UNALLOCATED;
    }
    mem[MAX_VALUES - 1].next_free = NULL;
    mem[MAX_VALUES - 1].value_type = V_UNALLOCATED;
    n_free_values = 0;
    for (v = free_list_head; NULL != v; v = v->next_free) {
        n_free_values += 1;
    }
    printf("Available values/cons cells = %d\n", n_free_values);
}

LISP_VALUE *new_value(int value_type)
{
    LISP_VALUE *ret = free_list_head;
    if (NULL == free_list_head) {
        gc();
        if (NULL == free_list_head) {
            fatal("Memory overflow.\n");
        }
        ret = free_list_head;
    }
    free_list_head = free_list_head->next_free;
    ret->value_type = value_type;
    if (V_CONS_CELL == value_type) {
        ret->car = ret->cdr = NULL;
    }
    n_free_values -= 1;
    return ret;
}

//------------------------------------------------------------------------------
// Environment-related routines
//
// The format of an environment is: (var-name var-value . <outer environment>)
// The end of the environment chain is terminated with NIL.  Thus, environments
// are ordinary lists which can be read and printed.
// As an example, the expression:
//
//     (let ((x 1)
//           (y 2)
//           (z 3))
//       (+ x y z))
//
// Creates an environment:
//
//     (z 3 y 2 x 1 . <outer env>)
//
// Note that variable update involves the "impure" operation of setting an
// environment's cadr.
//

LISP_VALUE *env_extend(LISP_VALUE *var_name,
                       LISP_VALUE *var_value,
                       LISP_VALUE *outer_env)
{
    LISP_VALUE *part1, *part2;
    part1 = cons(var_value, outer_env);
    protect_from_gc(part1);
    part2 = cons(var_name, part1);
    unprotect_from_gc();
    return part2;
}

LISP_VALUE *env_search(LISP_VALUE *name,
                       LISP_VALUE *env)
{
    if (!IS_TYPE(env, V_NIL)) {
        if (sym_eq(car(env), name)) {
            return env;
        } else {
            return env_search(name, cdr(cdr(env)));
        }
    }
    return NULL;
}

LISP_VALUE *env_fetch(LISP_VALUE *name,
                      LISP_VALUE *env)
{
    LISP_VALUE *e = env_search(name, env);
    if (NULL != e) {
        return car(cdr(e));
    }
    return NULL;
}

LISP_VALUE *env_set(LISP_VALUE *name,
                    LISP_VALUE *val,
                    LISP_VALUE *env)
{
    LISP_VALUE *e = env_search(name, env);
    if (NULL != e) {
        e->cdr->car = val;
    }
    return val;
}

// This function may not be called after any closure creation since
// it modifies global_env and (an|the) outer environment of all closures
// is global.  Also: this function does not prevent duplicate names
// from being added to global_env.
void global_env_init(LISP_VALUE *name,
                     LISP_VALUE *value)
{
    LISP_VALUE *value_part;
    value_part = cons(value, global_env);
    protect_from_gc(value_part);
    global_env = cons(name, value_part);
    unprotect_from_gc();
}

// This function may be safely called after any closure creation.
// global_env must contain at least one name/value pair.
void global_env_extend(LISP_VALUE *name,
                       LISP_VALUE *value)
{
    LISP_VALUE *tmp0, *tmp1;
    protect_from_gc(name);  // name
    protect_from_gc(value); // value name
    tmp0 = cons(value, global_env->cdr->cdr);
    // value will be protected when tmp0 is protected, so it
    // may be safely unprotected now.
    unprotect_from_gc();  // name
    protect_from_gc(tmp0); // tmp0 name
    tmp1 = cons(name, tmp0);
    unprotect_from_gc();  // name
    unprotect_from_gc();  //
    global_env->cdr->cdr = tmp1;
}

//------------------------------------------------------------------------------
// Eval-related and built-in functions.

#define N_STX_BITS 2
#define STX_CAR 0x1
#define STX_CDR 0x2
#define STX_BITMASK 0x3

#define ADD_STX_BITS(n, b) ((n) = ((n) << N_STX_BITS) | b)
#define STX_CADR ((N_STX_BITS << STX_CAR) | STX_CAR)

int syntax_check(LISP_VALUE *expr,
                 unsigned syntax_bits,
                 unsigned type_expected)
{
    while (syntax_bits) {
        if ((NULL == expr) || !IS_TYPE(expr, V_CONS_CELL)) {
            return 0;
        }
        switch (syntax_bits & STX_BITMASK) {
            case STX_CAR:
                expr = expr->car;
                break;
            case STX_CDR:
                expr = expr->cdr;
                break;
            default:
                return 0;
                break;
        }
    }
    return (NULL != expr) && IS_TYPE(expr, type_expected);
}

LISP_VALUE *eval(LISP_VALUE *expr,
                 LISP_VALUE *env)
{
    LISP_VALUE *ret = NULL;
    if (IS_SELF_EVAUATING(expr)) {
        return expr;
    } else if (IS_TYPE(expr, V_SYMBOL)) {
        if(NULL == (ret = env_fetch(expr, env))) {
            error("Variable not found: ");
            print_lisp_value(expr, 1);
        }
    }
#if 0
    else if (IS_TYPE(expr, V_CONS_CELL)) {
        // Keyword or function.
        LISP_VALUE *car_expr = car(expr);
        if (KW_EQ(car_expr, "quote")) {
            syntax_check
        }
       else if (KW_EQ(car_expr, "set!")) {
            LISP_VALUE *name;
        }
    }
#endif
    return NULL;
}

int main(int argc, char **argv)
{
    LISP_VALUE *expr, *value;
    init_free_list();
    global_env = new_value(V_NIL);
    for (;;) {
        expr = read_lisp_value();
        if (NULL != (value = eval(expr, global_env))) {
            printf("=>");
            print_lisp_value(value, 1);
        }
    }
}
