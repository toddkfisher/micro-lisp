#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "micro-lisp.h"

//------------------------------------------------------------------------------
/// Globals

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
/// Utility functions

void error(
  char *msg
)
{
  fprintf(stderr, "ERROR: %s\n", msg);
}

void fatal(
  char *msg
)
{
  fprintf(stderr, "FATAL: %s\n", msg);
  exit(1);
}

int sym_eq(
  LISP_VALUE *x,
  LISP_VALUE *y
)
{
  return STREQ(x->symbol, y->symbol);
}

LISP_VALUE *cons(
  LISP_VALUE *x,
  LISP_VALUE *y
)
{
  LISP_VALUE *res = new_value(V_CONS_CELL);
  res->car = x;
  res->cdr = y;
  return res;
}

LISP_VALUE *car(
  LISP_VALUE *x
)
{
  return x->car;
}

LISP_VALUE *cdr(
  LISP_VALUE *x
)
{
  return x->cdr;
}

LISP_VALUE *cadr(
  LISP_VALUE *x
)
{
  return x->cdr->car;
}

LISP_VALUE *caddr(
  LISP_VALUE *x
)
{
  return x->cdr->cdr->car;
}

//------------------------------------------------------------------------------
/// Value creation routines.

LISP_VALUE *create_intnum(
  int n
)
{
  LISP_VALUE *ret = new_value(V_INTNUM);
  ret->intnum = n;
  return ret;
}

LISP_VALUE *create_nil(void)
{
  return new_value(V_NIL);
}

LISP_VALUE *create_symbol(
  char *name
)
{
}

//------------------------------------------------------------------------------
/// Read/write routines

void print_lisp_value_aux(
  LISP_VALUE *val,
  int nest_level,
  int has_items_following
)
{
  if (NULL == val) {
    printf("<NULL>");
  }
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

void print_lisp_value(
  LISP_VALUE *val,
  int print_newline
)
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

LISP_VALUE *read_atom(void)
{
  int n_char = 0, maybe_number = 1, sign = 1, intnum = 0, saw_digit = 0;
  LISP_VALUE *ret = new_value(V_ANY);
  while (IS_ATOM_CHAR(current_char)) {
    if (('+' == current_char) || '-' == current_char) {
      // Sign (+|-) can only occur as first char.
      maybe_number = maybe_number && (0 == n_char);
      if (maybe_number) {
        sign = '-' == current_char ? -1 : 1;
      }
    } else if (IS_DIGIT(current_char)) {
      saw_digit = 1;
      if (maybe_number) {
        // We have a digit and what we've seen so far looks like a number.
        intnum = intnum*10 + (current_char - '0');
      }
    } else {
      maybe_number = 0;
    }
    if (n_char < SYM_SIZE - 1) {
      ret->symbol[n_char++] = current_char;
      ret->symbol[n_char] = '\0';
    }
    next_char();
  }
  if (maybe_number && saw_digit) {
    ret->value_type = V_INT;
    ret->intnum = sign*intnum;
  } else {
    ret->value_type = V_SYMBOL;
  }
  return ret;
}

LISP_VALUE *read_lisp_value(void)
{
  skip_blanks();
  if (IS_ATOM_CHAR(current_char)) {
    return read_atom();
  } else if ('(' == current_char) {
    return read_list();
  } else if (')' == current_char) {
    error("Unbalanced parens.");
  } else {
    error("Read error.");
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
/// Memory/gc-related routines.

void dump_protect_stack(void)
{
  int i;
  printf("---Protect stack:\n");
  for (i = 0; i < protect_stack_ptr; ++i) {
    printf("%02d : slot #%ld, addr %p\n", i, protect_stack[i] - mem,
           protect_stack[i]);
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

void indent(
  int n
)
{
  while (n-- > 0) {
    printf("    ");
  }
}

void gc_walk(
  LISP_VALUE *v,
  int depth
)
{
  if (NULL != v) {
    indent(depth);
    printf("gc_walk(slot == %ld, ptr == %p) : ", v - mem, v);
    if (!v->gc_mark) {
      printf("not visited - to be saved.\n");
      v->gc_mark = 1;
      indent(depth);
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

void protect_from_gc(
  LISP_VALUE *v
)
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

LISP_VALUE *new_value(
  int value_type
)
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
/// Environment-related routines
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

LISP_VALUE *env_extend(
  LISP_VALUE *var_name,
  LISP_VALUE *var_value,
  LISP_VALUE *outer_env
)
{
  LISP_VALUE *part1, *part2;
  part1 = cons(var_value, outer_env);
  protect_from_gc(part1);
  part2 = cons(var_name, part1);
  unprotect_from_gc();
  return part2;
}

LISP_VALUE *env_search(
  LISP_VALUE *name,
  LISP_VALUE *env
)
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

LISP_VALUE *env_fetch(
  LISP_VALUE *name,
  LISP_VALUE *env
)
{
  LISP_VALUE *e = env_search(name, env);
  if (NULL != e) {
    return car(cdr(e));
  }
  return NULL;
}

int env_set(
  LISP_VALUE *name,
  LISP_VALUE *val,
  LISP_VALUE *env
)
{
  LISP_VALUE *e = env_search(name, env);
  if (NULL != e) {
    e->cdr->car = val;
    return 1;
  }
  return 0;
}

// This function may not be called after any closure creation since
// it modifies global_env and (an|the) outer environment of all closures
// is global.  Also: this function does not prevent duplicate names
// from being added to global_env.
void global_env_init(
  LISP_VALUE *name,
  LISP_VALUE *value
)
{
  LISP_VALUE *value_part;
  value_part = cons(value, global_env);
  protect_from_gc(value_part);
  global_env = cons(name, value_part);
  unprotect_from_gc();
}

// This function may be safely called after any closure creation.
// global_env must contain at least one name/value pair.
void global_env_extend(
  LISP_VALUE *name,
  LISP_VALUE *value
)
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
/// Built-in functions and syntax.

enum {
  STX_SETQ,
  STX_QUOTE,
  STX_DUMPENV,
  FUNC_ADD
};

LISP_VALUE *stx_dumpenv(
  LISP_VALUE *env
)
{
  print_lisp_value(env, 1);
  return env;
}

LISP_VALUE *stx_quote(
  LISP_VALUE *arg,
  LISP_VALUE *env
)
{
  return arg;
}

LISP_VALUE *stx_setq(
  LISP_VALUE *name,
  LISP_VALUE *val,
  LISP_VALUE *env
)
{
  LISP_VALUE *curr_val = env_fetch(name, env);
  if (NULL == curr_val) {
    if (global_env == env) {
      printf("extending global env.\n");
      global_env_extend(name, val);
      return val;
    } else {
      error("Cannot extend global env in closure.");
      return NULL;
    }
  } else {
    env_set(name, val, env);
    return val;
  }
}

LISP_VALUE *fn_add(
  LISP_VALUE *x,
  LISP_VALUE *y,
  LISP_VALUE *env
)
{
  LISP_VALUE *res = new_value(V_INT);
  res->intnum = x->intnum + y->intnum;
  return res;
}

BUILTIN_INFO builtin_list[] = {
  {
    .name      = "setq",
    .type      = BUILTIN_SYNTAX,
    .n_args    = 2,
    .builtin_2 = stx_setq,
    ARG_UNEVALED, V_SYMBOL,
    ARG_EVALED,   V_ANY
  },
  {
    .name      = "quote",
    .type      = BUILTIN_SYNTAX,
    .n_args    = 1,
    .builtin_1 = stx_quote,
    ARG_UNEVALED, V_ANY
  },
  {
    .name      = "dumpenv",
    .type      = BUILTIN_SYNTAX,
    .builtin_0 = stx_dumpenv,
    .n_args    = 0
  },
  {
    .name = "add",
    .type = BUILTIN_FUNCTION,
    .builtin_2 = fn_add,
    .n_args = 2,
    ARG_EVALED, V_INT,
    ARG_EVALED, V_INT
  },
  {"", BUILTIN_NOTUSED, -1, NULL}, // end of list marker
};

LISP_VALUE *check_arg(
  BUILTIN_INFO *pinfo,
  int i_arg,
  LISP_VALUE *unevaled_arg,
  LISP_VALUE *env
)
{
  LISP_VALUE *evaluated_arg;
  if (ARG_UNEVALED == pinfo->arg_types[2*i_arg]) {
    if (!IS_TYPE(unevaled_arg, pinfo->arg_types[2*i_arg + 1])) {
      error("Incorrect argument type to function.");
      return NULL;
    }
    return unevaled_arg;
  } else if (ARG_EVALED == pinfo->arg_types[2*i_arg]) {
    if (NULL == (evaluated_arg = eval(unevaled_arg, env))) {
      return NULL;
    }
    if (!IS_TYPE(evaluated_arg, pinfo->arg_types[2*i_arg + 1])) {
      error("Incorrect argument type to function.");
      return NULL;
    }
    return evaluated_arg;
  }
  return NULL;
}

LISP_VALUE *call_builtin(
  BUILTIN_INFO *pinfo,
  LISP_VALUE *args[],
  LISP_VALUE *env
)
{
  switch (pinfo->n_args) {
    CALL_BUILTIN_WITH_ARG_ARRAY(0)
    CALL_BUILTIN_WITH_ARG_ARRAY(1)
    CALL_BUILTIN_WITH_ARG_ARRAY(2)
    CALL_BUILTIN_WITH_ARG_ARRAY(3)
    CALL_BUILTIN_WITH_ARG_ARRAY(4)
    CALL_BUILTIN_WITH_ARG_ARRAY(5)
    CALL_BUILTIN_WITH_ARG_ARRAY(6)
    CALL_BUILTIN_WITH_ARG_ARRAY(7)
    CALL_BUILTIN_WITH_ARG_ARRAY(8)
    CALL_BUILTIN_WITH_ARG_ARRAY(9)
    CALL_BUILTIN_WITH_ARG_ARRAY(10)
    CALL_BUILTIN_WITH_ARG_ARRAY(11)
    CALL_BUILTIN_WITH_ARG_ARRAY(12)
    CALL_BUILTIN_WITH_ARG_ARRAY(13)
    CALL_BUILTIN_WITH_ARG_ARRAY(14)
    CALL_BUILTIN_WITH_ARG_ARRAY(15)
  }
  return NULL;
}

LISP_VALUE *eval_builtin(
  BUILTIN_INFO *pinfo,
  LISP_VALUE *arglist,
  LISP_VALUE *env
)
{
  LISP_VALUE *arg_array[16], *unevaled_arg, *passed_arg, *ret;
  int n_args, i_arg;
  n_args = pinfo->n_args;
  for (i_arg = 0; i_arg < n_args; ++i_arg) {
    if (!IS_TYPE(arglist, V_CONS_CELL)) {
      error("Insufficient number of arguments to function.");
      return NULL;
    }
    unevaled_arg = car(arglist);
    arglist = cdr(arglist);
    if (NULL == (passed_arg = check_arg(pinfo, i_arg, unevaled_arg, env))) {
      return NULL;
    }
    arg_array[i_arg] = passed_arg;
  }
  for (i_arg = 0; i_arg < n_args; ++i_arg) {
    protect_from_gc(arg_array[i_arg]);
  }
  ret = call_builtin(pinfo, arg_array, env);
  for (i_arg = 0; i_arg < n_args; ++i_arg) {
    unprotect_from_gc();
  }
  return ret;
}

BUILTIN_INFO *get_keyword_info(
  LISP_VALUE *kw_sym
)
{
  int i;
  for (i = 0; BUILTIN_NOTUSED != builtin_list[i].type; ++i) {
    if (KW_EQ(kw_sym, builtin_list[i].name)) {
      return &builtin_list[i];
    }
  }
  return NULL;
}

LISP_VALUE *eval(
  LISP_VALUE *expr,
  LISP_VALUE *env
)
{
  LISP_VALUE *ret = NULL;
  protect_from_gc(expr);
  if (IS_SELF_EVAUATING(expr)) {
    ret = expr;
  } else if (IS_TYPE(expr, V_SYMBOL)) {
    if (NULL == (ret = env_fetch(expr, env))) {
      error("Variable not found: ");
      print_lisp_value(expr, 1);
    }
  } else if (IS_TYPE(expr, V_CONS_CELL)) {
    // function call or syntax.
    LISP_VALUE *fn = car(expr);
    if (IS_TYPE(fn, V_SYMBOL)) {
      // keyword (syntax) or variable holding either:
      // (1) builtin (gets evaluated by name)
      // (2) user-defined function
      BUILTIN_INFO *pinfo;
      if (NULL != (pinfo = get_keyword_info(fn))) {
        ret = eval_builtin(pinfo, cdr(expr), env);
      } else {
        ret = NULL;  // placeholder
      }
    } else {
      ret = NULL;  // placeholder
    }
  }
  unprotect_from_gc();
  return ret;
}



int main(
  int argc,
  char **argv
)
{
  LISP_VALUE *expr, *value, *name;
  init_free_list();
  global_env = new_value(V_NIL);

  print_lisp_value(global_env, 1);
  for (;;) {
    expr = read_lisp_value();
    printf("unevaluated =>");
    print_lisp_value(expr, 1);
    if (NULL != (value = eval(expr, global_env))) {
      printf("  evaluated =>");
      print_lisp_value(value, 1);
    }
  }
}
