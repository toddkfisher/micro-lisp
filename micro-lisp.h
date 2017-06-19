enum {
    V_INT         = 0x01,
    V_SYMBOL      = 0x02,
    V_CONS_CELL   = 0x04,
    V_CLOSURE     = 0x08,
    V_NIL         = 0x09,
    V_UNALLOCATED = 0x0a
};

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


f#define IS_TYPE(val, type) ((val)->value_type & type)
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
LISP_VALUE *env_set(LISP_VALUE *name, LISP_VALUE *val, LISP_VALUE *env);
