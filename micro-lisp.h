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

#define V_ANY (V_INT | V_SYMBOL | V_CONS_CELL | V_CLOSURE | V_NIL | V_BUILTIN)

TDS(LISP_VALUE);
TDS(BUILTIN_INFO);

#include "builtin-macros.h"

struct LISP_VALUE {
  unsigned gc_mark;
  unsigned value_type;
  union {
    // V_INT
    int intnum;
    // V_SYMBOL
    // Symbol names occupy the same size as two pointers.
#   define SYM_SIZE (2*sizeof(void *))
    char symbol[SYM_SIZE];
    // V_CONS_CELL
    struct {
      struct LISP_VALUE *car;
      struct LISP_VALUE *cdr;
    };
    // V_CLOSURE
    struct {
      struct LISP_VALUE *arg_list;
      struct LISP_VALUE *env;
      struct LISP_VALUE *code;
    };
    // V_BUILTIN
    BUILTIN_INFO *func_info;
    // V_UNALLOCATED
    struct LISP_VALUE *next_free;
  };
};

#define IS_TYPE(val, type) (NULL != (val) && ((val)->value_type & type))

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
   IN_RANGE((c), '!', '\'') ||                  \
   IN_RANGE((c), '*', '/') ||                   \
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

// Maximum number of built-in keywords(syntax) and functions.
#define MAX_BUILTINS 128

// Number of syntax keywords.  Non-syntax builtins go into builtin_info[]
// following these.
#define N_SYNTAX_KEYWORDS 4

// Maximum number of arguments a built-in keyword or function
// may have.
#define MAX_ARGS 16

enum {
  BUILTIN_SYNTAX,
  BUILTIN_FUNCTION,
  BUILTIN_NOTUSED
};

struct BUILTIN_INFO {
  char name[SYM_SIZE];
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
