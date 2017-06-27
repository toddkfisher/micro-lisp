enum LISP_TYPE {
    V_INT         = 0x01,
    V_SYMBOL      = 0x02,
    V_CONS_CELL   = 0x04,
    V_CLOSURE     = 0x08,
    V_NIL         = 0x09,
    V_UNALLOCATED = 0x0a
};

#define V_ANY (V_INT | V_SYMBOL | V_CONS_CELL | V_CLOSURE | V_NIL)

typedef struct _LISP_VALUE LISP_VALUE;
struct _LISP_VALUE {
    unsigned gc_mark;
    unsigned value_type;
    union {
        // V_INT
        int intnum;
        // V_SYMBOL
        // Symbol names occupy the same size as to pointers.
#       define SYM_SIZE (2*sizeof(void *))
        char symbol[SYM_SIZE];
        // V_CONS_CELL
        struct { struct _LISP_VALUE *car, *cdr; };
        // V_CLOSURE
        struct { struct _LISP_VALUE *env, *code; };
        // V_UNALLOCATED
        struct _LISP_VALUE *next_free;
    };
};


#define IS_TYPE(val, type) ((val)->value_type & type)
#define IS_ATOM(val) (IS_TYPE(val, V_INT) || IS_TYPE(val, V_SYMBOL) || \
                      IS_TYPE(val, V_NIL))
#define IS_MARKED(val) ((val)->value_bits & ~TYPE_BITMASK)
#define IS_WHITESPACE(c) (' ' == (c) || '\t' == (c) ||'\n' == (c))
#define IS_DIGIT(c) IN_RANGE((c), '0', '9')
#define IN_RANGE(x, a, b) (((a) <= (x)) && ((x) <= (b)))
#define STREQ(x, y) (!strcmp((x), (y)))
#define IS_ALPHANUM(c)                          \
    IN_RANGE((c), 'a', 'z') ||                  \
    IN_RANGE((c), 'A', 'Z') ||                  \
    IS_DIGIT((c))
#define KW_EQ(x, strconst)                                      \
    (IS_TYPE((x), V_SYMBOL) && STREQ((x)->symbol, (strconst)))
#define IS_SELF_EVAUATING(x) (IS_TYPE(x, V_INT) || IS_TYPE(x, V_NIL))

// size of mem[] array.
#define MAX_VALUES 100000

// size of protect_stack[]
#define MAX_PROTECTED 1024

#define ARGS_0 void
#define ARGS_1 LISP_VALUE *
#define ARGS_2 LISP_VALUE *,  ARGS_1
#define ARGS_3 LISP_VALUE *,  ARGS_2
#define ARGS_4 LISP_VALUE *,  ARGS_3
#define ARGS_5 LISP_VALUE *,  ARGS_4
#define ARGS_6 LISP_VALUE *,  ARGS_5
#define ARGS_7 LISP_VALUE *,  ARGS_6
#define ARGS_8 LISP_VALUE *,  ARGS_7
#define ARGS_9 LISP_VALUE *,  ARGS_8
#define ARGS_10 LISP_VALUE *, ARGS_9
#define ARGS_11 LISP_VALUE *, ARGS_10
#define ARGS_12 LISP_VALUE *, ARGS_11
#define ARGS_13 LISP_VALUE *, ARGS_12
#define ARGS_14 LISP_VALUE *, ARGS_13
#define ARGS_15 LISP_VALUE *, ARGS_14
#define ARGS_16 LISP_VALUE *, ARGS_15
#define BUILTIN_FN_SIG(n) typedef LISP_VALUE *(*builtin_fn_##n)(ARGS_##n)

BUILTIN_FN_SIG(0);
BUILTIN_FN_SIG(1);
BUILTIN_FN_SIG(2);
BUILTIN_FN_SIG(3);
BUILTIN_FN_SIG(4);
BUILTIN_FN_SIG(5);
BUILTIN_FN_SIG(6);
BUILTIN_FN_SIG(7);
BUILTIN_FN_SIG(8);
BUILTIN_FN_SIG(9);
BUILTIN_FN_SIG(10);
BUILTIN_FN_SIG(11);
BUILTIN_FN_SIG(12);
BUILTIN_FN_SIG(13);
BUILTIN_FN_SIG(14);
BUILTIN_FN_SIG(15);
BUILTIN_FN_SIG(16);

#define MAX_ARGS 16

typedef struct _BUILTIN_INFO BUILTIN_INFO;
struct _BUILTIN_INFO {
    char *name;
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

void print_lisp_value(LISP_VALUE *val, int print_newline);
void next_char(void);
void skip_blanks(void);
LISP_VALUE *read_symbol(void);
LISP_VALUE *read_intnum(void);
LISP_VALUE *read_list(void);
LISP_VALUE *read_lisp_value(void);
LISP_VALUE *new_value(int value_type);
void protect_from_gc(LISP_VALUE *v);
void unprotect_from_gc(void);
void mark(void);
void sweep(void);
void collect(void);
void gc_walk(LISP_VALUE *v, int depth);
void dump_protect_stack(void);
LISP_VALUE *env_extend(LISP_VALUE *var_name, LISP_VALUE *var_value,
                       LISP_VALUE *outer_env);
LISP_VALUE *env_search(LISP_VALUE *name, LISP_VALUE *env);
LISP_VALUE *env_fetch(LISP_VALUE *name, LISP_VALUE *env);
int env_set(LISP_VALUE *name, LISP_VALUE *val, LISP_VALUE *env);
