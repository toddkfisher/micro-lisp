#pragma once

enum LISP_TYPE {
  V_INT         = 0x01,
  V_SYMBOL      = 0x02,
  V_CONS_CELL   = 0x04,
  V_CLOSURE     = 0x08,
  V_NIL         = 0x10,
  V_BUILTIN     = 0x20,
  V_UNALLOCATED = 0x40
};

#define V_ANY (V_INT | V_SYMBOL | V_CONS_CELL | V_CLOSURE | V_NIL)

typedef struct _BUILTIN_INFO BUILTIN_INFO;
typedef struct _LISP_VALUE LISP_VALUE;

#include "builtin-macros.h"

struct _LISP_VALUE {
  unsigned gc_mark;
  unsigned value_type;
  union {
    // V_INT
    int intnum;
    // V_SYMBOL
    // Symbol names occupy the same size as to pointers.
#   define SYM_SIZE (2*sizeof(void *))
    char symbol[SYM_SIZE];
    // V_CONS_CELL
    struct { struct _LISP_VALUE *car, *cdr; };
    // V_CLOSURE
    struct { struct _LISP_VALUE *env, *code; };
    // V_BUILTIN
    struct {
      unsigned builtin_id;
      BUILTIN_INFO *func_info;
    };
    // V_UNALLOCATED
    struct _LISP_VALUE *next_free;
  };
};


#define IS_TYPE(val, type) ((val)->value_type & type)

#define IS_ATOM(val) (IS_TYPE(val, V_INT) || IS_TYPE(val, V_SYMBOL) ||  \
                      IS_TYPE(val, V_NIL))

#define IS_MARKED(val) ((val)->value_bits & ~TYPE_BITMASK)

#define IS_WHITESPACE(c) (' ' == (c) || '\t' == (c) ||'\n' == (c))

#define IS_DIGIT(c) IN_RANGE((c), '0', '9')

#define IN_RANGE(x, a, b) (((a) <= (x)) && ((x) <= (b)))

#define STREQ(x, y) (!strcmp((x), (y)))

#define IS_ALPHANUM(c)                          \
  (IN_RANGE((c), 'a', 'z') ||                   \
   IN_RANGE((c), 'A', 'Z') ||                   \
   IS_DIGIT((c)))

#define IS_ATOM_CHAR(c)                         \
  (IS_ALPHANUM(c) ||                            \
   IS_DIGIT(c)    ||                            \
   IN_RANGE((c), '!', '/') ||                   \
   IN_RANGE((c), ':', '@') ||                   \
   IN_RANGE((c), '[', '`') ||                   \
   IN_RANGE((c), '{', '~'))

#define KW_EQ(x, strconst)                                      \
  (IS_TYPE((x), V_SYMBOL) && STREQ((x)->symbol, (strconst)))

#define IS_SELF_EVAUATING(x) (IS_TYPE((x), V_INT) || IS_TYPE((x), V_NIL))

#define ARG_EVALED 0
#define ARG_UNEVALED 1

// size of mem[] array.
#define MAX_VALUES 100000

// size of protect_stack[]
#define MAX_PROTECTED 1024

#define MAX_ARGS 16
#define BUILTIN_SYNTAX 0
#define BUILTIN_FUNCTION 1
#define BUILTIN_NOTUSED 2

struct _BUILTIN_INFO {
  char *name;
  int type;
  int n_args;
  union {
    builtin_fn_0  builtin_0;
    builtin_fn_1  builtin_1;
    builtin_fn_2  builtin_2;
    builtin_fn_3  builtin_3;
    builtin_fn_4  builtin_4;
    builtin_fn_5  builtin_5;
    builtin_fn_6  builtin_6;
    builtin_fn_7  builtin_7;
    builtin_fn_8  builtin_8;
    builtin_fn_9  builtin_9;
    builtin_fn_10 builtin_10;
    builtin_fn_11 builtin_11;
    builtin_fn_12 builtin_12;
    builtin_fn_13 builtin_13;
    builtin_fn_14 builtin_14;
    builtin_fn_15 builtin_15;
    builtin_fn_16 builtin_16;
  };
  int arg_types[MAX_ARGS*2];
};

void print_lisp_value(
  LISP_VALUE *val,
  int print_newline
);

void next_char(void);

void skip_blanks(void);

LISP_VALUE *read_symbol(void);

LISP_VALUE *read_intnum(void);

LISP_VALUE *read_list(void);

LISP_VALUE *read_lisp_value(void);

LISP_VALUE *new_value(
  int value_type
);

void protect_from_gc(
  LISP_VALUE *v
);

void unprotect_from_gc(void);

void mark(void);

void sweep(void);

void collect(void);

void gc_walk(
  LISP_VALUE *v,
  int depth
);

void dump_protect_stack(void);

LISP_VALUE *env_extend(
  LISP_VALUE *var_name,
  LISP_VALUE *var_value,
  LISP_VALUE *outer_env
);

LISP_VALUE *env_search(
  LISP_VALUE *name,
  LISP_VALUE *env
);

LISP_VALUE *env_fetch(
  LISP_VALUE *name,
  LISP_VALUE *env
);

int env_set(
  LISP_VALUE *name,
  LISP_VALUE *val,
  LISP_VALUE *env
);

LISP_VALUE *eval_builtin(
  BUILTIN_INFO *pinfo,
  LISP_VALUE *arglist,
  LISP_VALUE *env
);

LISP_VALUE *eval(
  LISP_VALUE *expr,
  LISP_VALUE *env
);

LISP_VALUE *check_arg(
  BUILTIN_INFO *pinfo,
  int i_arg,
  LISP_VALUE *unevaled_arg,
  LISP_VALUE *env
);

LISP_VALUE *call_builtin(
  BUILTIN_INFO *pinfo,
  LISP_VALUE *args[],
  LISP_VALUE *env
);
