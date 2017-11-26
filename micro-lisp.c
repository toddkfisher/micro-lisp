#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "micro-lisp.h"
#include "proto.h"

//------------------------------------------------------------------------------
/// Globals

// Array of values.  This is the "source" for all LISP_VALUEs
LISP_VALUE mem[MAX_VALUES];

// Free list of cells from mem[].
LISP_VALUE *free_list_head = NULL;

// Counter for reporting.
int n_free_values = N_SYNTAX_KEYWORDS;

// Stack of cells in mem[] for which may not be collected during GC.
// Ancestors to values on protect_stack[] are also saved from collection.
LISP_VALUE *protect_stack[MAX_PROTECTED];
int protect_stack_ptr = 0;

// Current input character not yet processed.
char current_char = '\n';

// Nesting level of lists.
int nest_level = 0;

// Global environment.  Gets special treatment since everything points to
// it.
LISP_VALUE *global_env;

// Index of next function to be placed into builtin_list[].
int builtin_index = N_SYNTAX_KEYWORDS;

//------------------------------------------------------------------------------
/// Utility

void error(char *msg)
{
  fprintf(stderr, "ERROR: %s\n", msg);
}

void fatal(char *msg)
{
  fprintf(stderr, "FATAL: %s\n", msg);
  exit(1);
}

int sym_eq(LISP_VALUE *x, LISP_VALUE *y)
{
  return STREQ(x->symbol, y->symbol);
}

LISP_VALUE *cons(LISP_VALUE *x, LISP_VALUE *y)
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

LISP_VALUE *caddr(LISP_VALUE *x)
{
  return x->cdr->cdr->car;
}

//------------------------------------------------------------------------------
/// Value creation

LISP_VALUE *create_intnum(int n)
{
  LISP_VALUE *ret = new_value(V_INT);
  ret->intnum = n;
  return ret;
}

LISP_VALUE *create_nil(void)
{
  return new_value(V_NIL);
}

// must be able to store at least 1 char (nul) in *dest
char *strncpy_with_nul(char *dest, char *src, int n)
{
  int n_chars_copied = 0;
  while (*src && n > 0 && n_chars_copied < n) {
    *dest++ = *src++;
  }
  *dest = '\0';
  return dest;
}

LISP_VALUE *create_symbol(char *name)
{
  LISP_VALUE *ret = new_value(V_SYMBOL);
  int n_chars_copied = 0;
  while (*name && n_chars_copied < SYM_SIZE - 1) {
    ret->symbol[n_chars_copied++] = *name++;
  }
  ret->symbol[n_chars_copied] = '\0';
  return ret;
}

LISP_VALUE *create_builtin(BUILTIN_INFO *func_info)
{
  LISP_VALUE *ret = new_value(V_BUILTIN);
  ret->func_info = func_info;
  return ret;
}

//------------------------------------------------------------------------------
/// Read/write

void print_lisp_value_aux(LISP_VALUE *val, int nest_level,
                          int has_items_following)
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
      printf("#<CLOSURE: %p, %p, %p>", val->arg_names, val->code, val->env);
      break;
    case V_NIL:
      if (nest_level > 0) {
        printf("()");
      } else {
        printf("nil");
      }
      break;
    case V_BUILTIN:
      printf("#<BUILTIN: %s>", val->func_info->name);
      break;
    default:
      fatal("Unknown lisp type.\n");
      break;
  }
  if (has_items_following) {
    printf(" ");
  }
}

void print_lisp_value(LISP_VALUE *val, int print_newline)
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
  int n_char = 0;
  int maybe_number = 1;
  int sign = 1;
  int intnum = 0;
  int saw_digit = 0;
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
  LISP_VALUE *left;
  LISP_VALUE *right;
  LISP_VALUE *curr;
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
/// Memory/GC

void dump_protect_stack(void)
{
#ifdef DEBUG
  int i;
  DBG_MSG("Protect stack:");
  for (i = 0; i < protect_stack_ptr; ++i) {
    printf("! %02d : slot #%ld, addr %p\n", i, protect_stack[i] - mem,
           protect_stack[i]);
    if (IS_ATOM(protect_stack[i])) {
      DBG_MSG("value = ");
      DBG_PRINT_LISP_VAR(protect_stack[i]);
    }
  }
  DBG_MSG("Bottom of protect stack");
#endif
}

void gc(void)
{
  DBG_MSG("Garbage collecting...");
  mark();
  dump_protect_stack();
  sweep();
  collect();
  DBG_FN_PRINT_VAR(n_free_values, "%d");
}

void mark(void)
{
  int i;
  DBG_MSG("Marking gc_mark = 0 on all values...");
  for (i = 0; i < MAX_VALUES; ++i) {
    // Zero out marked bit.  Zero mark bit means "collect"
    mem[i].gc_mark = 0;
  }
#ifdef DEBUG
  printf("! Marked %d values/cons cells.\n", i);
#endif
}

void sweep(void)
{
  int i;
  DBG_MSG("Walking global_env.");
  gc_walk(global_env, 0);
#ifdef DEBUG
  printf("! Walking protect_stack[].  Size == %d.\n", protect_stack_ptr);
#endif
  for (i = 0; i < protect_stack_ptr; i++) {
#ifdef DEBUG
    printf("! Walking protect_stack[%d] ==%p.\n", i, protect_stack[i]);
#endif
    gc_walk(protect_stack[i], 0);
  }
}

void indent(int n)
{
  while (n-- > 0) {
    printf("    ");
  }
}

void gc_walk(LISP_VALUE *v, int depth)
{
  if (NULL != v) {
    indent(depth);
#ifdef DEBUG
    printf("! gc_walk(slot == %ld, ptr == %p) : ", v - mem, v);
#endif
    if (!v->gc_mark) {
      DBG_MSG("not visited - to be saved.");
      v->gc_mark = 1;
      indent(depth);
      switch (v->value_type) {
        case V_INT:
        case V_SYMBOL:
        case V_NIL:
          break;
        case V_CONS_CELL:
          indent(depth);
          DBG_MSG("walking car.");
          gc_walk(v->car, depth + 1);
#ifdef DEBUG
          printf("\n");
          indent(depth);
          DBG_MSG("walking cdr.");
#endif
          gc_walk(v->cdr, depth + 1);
          break;
        case V_CLOSURE:
          indent(depth);
          DBG_MSG("walking env.");
          gc_walk(v->env, depth + 1);
#ifdef DEBUG
          printf("\n");
          indent(depth);
          DBG_MSG("walking code.");
#endif
          gc_walk(v->code, depth + 1);
          break;
        case V_UNALLOCATED:
          fatal("gc_walk() on V_UNALLOCATED.\n");
          break;
      }
    } else {
      DBG_MSG("Already visited.");
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
#ifdef DEBUG
  printf("Available values/cons cells = %d\n", n_free_values);
#endif
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
/// Environment
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

LISP_VALUE *env_extend(LISP_VALUE *var_name, LISP_VALUE *var_value,
                       LISP_VALUE *outer_env)
{
  LISP_VALUE *part1;
  LISP_VALUE *part2;
  part1 = cons(var_value, outer_env);
  protect_from_gc(part1);
  part2 = cons(var_name, part1);
  unprotect_from_gc();
  return part2;
}

LISP_VALUE *env_search(LISP_VALUE *name, LISP_VALUE *env)
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

LISP_VALUE *env_fetch(LISP_VALUE *name, LISP_VALUE *env)
{
  LISP_VALUE *e = env_search(name, env);
  if (NULL != e) {
    return car(cdr(e));
  }
  return NULL;
}

int env_set(LISP_VALUE *name, LISP_VALUE *val, LISP_VALUE *env)
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
void global_env_init(LISP_VALUE *name, LISP_VALUE *value)
{
  LISP_VALUE *value_part;
  value_part = cons(value, global_env);
  protect_from_gc(value_part);
  global_env = cons(name, value_part);
  unprotect_from_gc();
}

// This function may be safely called after any closure creation.
// global_env must contain at least one name/value pair.
void global_env_extend(LISP_VALUE *name, LISP_VALUE *value)
{
  LISP_VALUE *tmp0;
  LISP_VALUE *tmp1;
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
/// Built-ins

LISP_VALUE *stx_dumpenv(LISP_VALUE *env)
{
  print_lisp_value(env, 1);
  return env;
}

LISP_VALUE *stx_quote(LISP_VALUE *arg, LISP_VALUE *env)
{
  return arg;
}

LISP_VALUE *stx_closure(LISP_VALUE *clo_expr, LISP_VALUE *env)
{
  LISP_VALUE *arg_names;
  LISP_VALUE *ret;
  LISP_VALUE *args;
  arg_names = car(clo_expr);
  if (!IS_TYPE(arg_names, V_CONS_CELL)) {
    error("Argument list to closure should be () or (arg ...)");
    return NULL;
  }
  FOR_LIST(args, arg_names) {
    if (!IS_TYPE(car(args), V_SYMBOL)) {
      error("Argument names for closure should be symbols.");
      return NULL;
    }
  }
  ret = new_value(V_CLOSURE);
  ret->arg_names = arg_names;
  ret->env = env;
  ret->code = cdr(clo_expr);
  return ret;
}


LISP_VALUE *stx_setq(LISP_VALUE *name, LISP_VALUE *val, LISP_VALUE *env)
{
  LISP_VALUE *curr_val = env_fetch(name, env);
  if (NULL == curr_val) {
    if (global_env == env) {
      DBG_MSG("extending global env.");
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

LISP_VALUE *fn_add(LISP_VALUE *x, LISP_VALUE *y, LISP_VALUE *env)
{
  LISP_VALUE *res = new_value(V_INT);
  res->intnum = x->intnum + y->intnum;
  return res;
}

BUILTIN_INFO builtin_list[] = {
   [0] = {
    .name      = "setq",
    .type      = BUILTIN_SYNTAX,
    .n_args    = 2,
    .builtin_2 = stx_setq,
    ARG_UNEVALED, V_SYMBOL,
    ARG_EVALED,   V_ANY
  },
  [1] = {
    .name      = "quote",
    .type      = BUILTIN_SYNTAX,
    .n_args    = 1,
    .builtin_1 = stx_quote,
    ARG_UNEVALED, V_ANY
  },
  [2] = {
    .name      = "dumpenv",
    .type      = BUILTIN_SYNTAX,
    .builtin_0 = stx_dumpenv,
    .n_args    = 0
  },
  [3] = {
    .name      = "fn",
    .type      = BUILTIN_SYNTAX,
    .n_args    = -1,
    .builtin_1 = stx_closure,
    ARG_UNEVALED, V_CONS_CELL
  },
  [MAX_BUILTINS - 1] = {"", BUILTIN_NOTUSED, -1, NULL}, // end of list marker
};

#define SWITCH_16(v, m)                          \
  do {                                           \
    switch (v) {                                 \
      case 0: m(0); break;                       \
      case 1: m(1); break;                       \
      case 2: m(2); break;                       \
      case 3: m(3); break;                       \
      case 4: m(4); break;                       \
      case 5: m(5); break;                       \
      case 6: m(6); break;                       \
      case 7: m(7); break;                       \
      case 8: m(8); break;                       \
      case 9: m(9); break;                       \
      case 10: m(10); break;                     \
      case 11: m(11); break;                     \
      case 12: m(12); break;                     \
      case 13: m(13); break;                     \
      case 14: m(14); break;                     \
      case 15: m(15); break;                     \
      case 16: m(16); break;                     \
    }                                            \
  } while (0)

#define SET_BUILTIN_FN(n) builtin_list[builtin_index].builtin_##n = (builtin_fn_##n) fn;

// This function exists because the "installation" of builtin functions
// is slightly more complicated than simple array initialization which
// is all that is required for builtin syntax.  Builtin functions must
// be bound to a variable in the global environment (ex. "+" is bound to
// fn_add()).
int install_builtin_fn(char *var_name, char *descriptive_name, void *fn,
                       int n_args)
{
  if (builtin_index < MAX_BUILTINS - 1) {
    LISP_VALUE *var = create_symbol(var_name);
    LISP_VALUE *val;
    strncpy_with_nul(builtin_list[builtin_index].name, descriptive_name, SYM_SIZE - 1);
    builtin_list[builtin_index].type = BUILTIN_FUNCTION;
    builtin_list[builtin_index].n_args = n_args;
    SWITCH_16(n_args, SET_BUILTIN_FN);
    val = create_builtin(&builtin_list[builtin_index]);
    builtin_index += 1;
    global_env_init(var, val);
    return builtin_index - 1;
  }
  return -1;
}

char *type_name(int t)
{
  switch (t) {
    case V_INT:
      return "integer";
    case V_SYMBOL:
      return "symbol";
    case V_CONS_CELL:
      return "cons";
    case V_CLOSURE:
      return "closure";
    case V_NIL:
      return "nil";
    case V_BUILTIN:
      return "builtin";
    default:
      return "unknown";
  }
}

void arg_type_error(char *fn_name, int i_arg, LISP_VALUE *arg, int expected_type)
{
  printf("ERROR: incorrect type to argument %d of %s\n"
         "       expected: %s\n"
         "       recieved: %s\n"
         "       value is: ", i_arg, fn_name, type_name(expected_type),
         type_name(arg->value_type));
  print_lisp_value(arg, 1);
}

LISP_VALUE *check_arg(BUILTIN_INFO *pinfo, int i_arg, LISP_VALUE *unevaled_arg,
                      LISP_VALUE *env)
{
  LISP_VALUE *evaluated_arg;
  int expected_arg_type;
  expected_arg_type = pinfo->arg_types[2*i_arg + 1];
  if (ARG_UNEVALED == pinfo->arg_types[2*i_arg]) {
    if (!IS_TYPE(unevaled_arg, expected_arg_type)) {
      arg_type_error(pinfo->name, i_arg, unevaled_arg, expected_arg_type);
      return NULL;
    }
    return unevaled_arg;
  } else if (ARG_EVALED == pinfo->arg_types[2*i_arg]) {
    if (NULL == (evaluated_arg = eval(unevaled_arg, env))) {
      return NULL;
    }
    if (!IS_TYPE(evaluated_arg, expected_arg_type)) {
      arg_type_error(pinfo->name, i_arg, evaluated_arg, expected_arg_type);
      return NULL;
    }
    return evaluated_arg;
  }
  return NULL;
}

LISP_VALUE *call_builtin(BUILTIN_INFO *pinfo, LISP_VALUE *args[],
                         LISP_VALUE *env)
{
  int n_args = pinfo->n_args < 0 ? 1 : pinfo->n_args;
  SWITCH_16(n_args, CALL_BUILTIN_WITH_ARG_ARRAY);
  return NULL;
}

void set_builtin_arg_info(int builtin_idx, int arg_idx, int eval_type,
                          int arg_type)
{
  builtin_list[builtin_idx].arg_types[2*arg_idx] = eval_type;
  builtin_list[builtin_idx].arg_types[2*arg_idx + 1] = arg_type;
}

//------------------------------------------------------------------------------
/// Evaluation

LISP_VALUE *eval_builtin(BUILTIN_INFO *pinfo, LISP_VALUE *arglist,
                         LISP_VALUE *env)
{
  LISP_VALUE *arg_array[16];
  LISP_VALUE *unevaled_arg;
  LISP_VALUE *passed_arg;
  LISP_VALUE *ret;
  int n_args, i_arg;
  // pinfo->n_args < 0 means to treat entire argument list as a single argument.
  n_args = pinfo->n_args < 0 ? 1 : pinfo->n_args;
  if (pinfo->n_args < 0) {
    unevaled_arg = arglist;
    if (NULL == (passed_arg = check_arg(pinfo, 0, unevaled_arg, env))) {
      return NULL;
    }
    arg_array[0] = passed_arg;
  } else {
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

BUILTIN_INFO *get_builtin_info(LISP_VALUE *kw_sym)
{
  int i;
  for (i = 0; i < N_SYNTAX_KEYWORDS; ++i) {
    if (BUILTIN_SYNTAX == builtin_list[i].type &&
        KW_EQ(kw_sym, builtin_list[i].name)) {
      return &builtin_list[i];
    }
  }
  return NULL;
}

int is_syntax(LISP_VALUE *expr)
{
  BUILTIN_INFO *pinfo;
  LISP_VALUE *car_expr;
  // to be syntax, expr must be: (<symbol> ...) and <symbol> must exist in
  // the builtin_list[] with type == BUILTIN_SYNTAX
  if (!IS_TYPE(expr, V_CONS_CELL)) {
    return 0;
  }
  car_expr = car(expr);
  if (!IS_TYPE(car_expr, V_SYMBOL)) {
    return 0;
  }
  if (NULL == (pinfo = get_builtin_info(car_expr))) {
    return 0;
  }
  return BUILTIN_SYNTAX == pinfo->type;
}

// LISP_VALUE eval_seq(LISP_VALUE *seq, LISP_VALUE *env)
// {
//   LISP_VALUE ret;
//   if (IS_TYPE(seq, V_NIL)) {
//     ret = the_nil_value;
//   } else {
//
//
// LISP_VALUE eval_closure_application(LISP_VALUE *clo, LISP_VALUE *arg_list,
//                                     LISP_VALUE *env)
// {
//   if (IS_TYPE(arg_list, V_CONS_CELL)) {
//     FOR_LIST(args, clo->arg_names) {
//       evaled_arg =
//     }
//   } else if (IS_TYPE(arg_list, V_NIL)) {
//     ret = new_value(V_NIL);
//   } else {
//     error("Dotted pair used as argument.");
//     return NULL;
//   }
// }

LISP_VALUE *eval_application(LISP_VALUE *expr, LISP_VALUE *env)
{
  LISP_VALUE *fn;
  LISP_VALUE *ret = NULL;
  int n_args = 0;
  fn = eval(car(expr), env);
  protect_from_gc(fn);
  if (IS_TYPE(fn, V_BUILTIN)) {
    ret = eval_builtin(fn->func_info, cdr(expr), env);
  } else if (IS_TYPE(fn, V_CLOSURE)) {
    //ret = eval_closure_application(fn, cdr(expr), env);
  } else {
    error("Application of non-closure.\n");
  }
  unprotect_from_gc();
  return ret;
}

LISP_VALUE *eval(LISP_VALUE *expr, LISP_VALUE *env)
{
  LISP_VALUE *ret = NULL;
  if (NULL != expr) {
    protect_from_gc(expr);
    if (IS_SELF_EVAUATING(expr)) {
      ret = expr;
    } else if (IS_TYPE(expr, V_SYMBOL)) {
      ret = eval_var(expr, env);
    } else if (is_syntax(expr)) {
      ret = eval_syntax(expr, env);
      print_lisp_value(ret, 1);
    } else if (IS_TYPE(expr, V_CONS_CELL)) {
      ret = eval_application(expr, env);
    } else {
      fatal("Unknown form.");
    }
    unprotect_from_gc();
  }
  return ret;
}

LISP_VALUE *eval_var(LISP_VALUE *expr, LISP_VALUE *env)
{
  LISP_VALUE *ret = NULL;
  if (NULL == (ret = env_fetch(expr, env))) {
    error("Variable not found: ");
    print_lisp_value(expr, 1);
  }
  return ret;
}

LISP_VALUE *eval_syntax(LISP_VALUE *expr, LISP_VALUE *env)
{
  BUILTIN_INFO *pinfo = get_builtin_info(car(expr));
  // since expr passed is_syntax() we know NULL != pinfo
  return eval_builtin(pinfo, cdr(expr), env);
}

int main(int argc, char **argv)
{
  LISP_VALUE *expr;
  LISP_VALUE *value;
  LISP_VALUE *name;
  int idx;
  init_free_list();
  global_env = new_value(V_NIL);
  idx = install_builtin_fn("+", "add", fn_add, 2);
  DBG_FN_PRINT_VAR(idx, "%d");
  set_builtin_arg_info(idx, 0, ARG_EVALED, V_INT);
  set_builtin_arg_info(idx, 1, ARG_EVALED, V_INT);
  for (;;) {
    expr = read_lisp_value();
    DBG_MSG("unevaluated =>");
    DBG_PRINT_LISP_VAR(expr);
    if (NULL != (value = eval(expr, global_env))) {
      printf("result =>");
      print_lisp_value(value, 1);
    }
  }
}
