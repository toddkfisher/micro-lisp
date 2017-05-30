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

//------------------------------------------------------------------------------
// Read/write routines

void print_lisp_value(LISP_VALUE *val, int nest_level, int has_items_following)
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
  printf("symbol=%s\n", ret->symbol);
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
    printf("Error reading integer.\n");
    exit(0);
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
    printf("Read error.\n");
    exit(0);
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
    printf("Slot #%d, addr %p\n", protect_stack[i] - mem, protect_stack[i]);
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
  printf("Walking protect_stack[].  Size == %d.\n", protect_stack_ptr);
  for (i = 0; i < protect_stack_ptr; i++) {
    printf("Walking protect_stack[%d] ==%p.\n", i, protect_stack[i]);
    gc_walk(protect_stack[i]);
  }
}

void gc_walk(LISP_VALUE *v)
{
  if (NULL != v) {
    printf("gc_walk(slot == %d, ptr == %p) : ", v - mem, v);
    if (!v->gc_mark) {
      printf("not visited - to be saved.\n");
      v->gc_mark = 1;
      printf("Type == %s.", value_names[v->value_type]);
      switch (v->value_type) {
        case V_INT:
        case V_SYMBOL:
        case V_NIL:
          break;
        case V_CONS_CELL:
          printf("Walking car.\n");
          gc_walk(v->car);
          printf("Walking cdr.\n");
          gc_walk(v->cdr);
          break;
        case V_CLOSURE:
          printf("Walking env.\n");
          gc_walk(v->env);
          printf("Walking code.\n");
          gc_walk(v->code);
          break;
        case V_UNALLOCATED:
          printf("gc_walk() on V_UNALLOCATED.\n");
          exit(0);
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
  protect_stack[protect_stack_ptr++] = v;
}

void unprotect_from_gc(void)
{
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
      printf("Memory overflow.\n");
      exit(0);
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

int main(int argc, char **argv)
{
  LISP_VALUE *x;
  init_free_list();
  for (;;) {
    x = read_lisp_value();
    printf("Value=");
    print_lisp_value(x, 0, 0);
    printf("\n");
  }
  exit(0);
}
