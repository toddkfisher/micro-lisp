enum {
    V_INT,
    V_SYMBOL,
    V_CONS_CELL,
    V_CLOSURE,
    V_NIL,
    V_UNALLOCATED
};

typedef struct _LISP_VALUE {
#   define TYPE_BITMASK 0x7  // lower 3 bits are type.
#   define GC_MARK_BITMASK (1 << 3)
    unsigned value_bits;
    union {
        // V_INT
        int intnum;
        // V_SYMBOL
#       define SYM_SIZE (2*sizeof(void *))
        char symbol[SYM_SIZE];
        // V_CONS_CELL
        struct { struct _LISP_VALUE *car, *cdr; };
        // V_CLOSURE
        struct { struct _LISP_VALUE *env, *code; };
        // V_UNALLOCATED
        struct _LISP_VALUE *next_free;
    };
} LISP_VALUE;

void print_lisp_value(LISP_VALUE *val, int nest_level, int has_items_following);
void next_char(void);
void skip_blanks(void);
LISP_VALUE *read_symbol(void);
LISP_VALUE *read_intnum(void);
LISP_VALUE *read_list(void);
LISP_VALUE *read_lisp_value(void);
void gc_walk(

#define IS_TYPE(val, type) (((val)->value_bits & TYPE_BITMASK) == type)
#define IS_MARKED(val) ((val)->value_bits & ~TYPE_BITMASK)
#define IS_WHITESPACE(c) (' ' == (c) || '\t' == (c) ||'\n' == (c))
#define IS_DIGIT(c) IN_RANGE((c), '0', '9')
#define IN_RANGE(x, a, b) (((a) <= (x)) && ((x) <= (b)))
#define STREQ(x, y) (!strcmp((x), (y)))
#define IS_ALPHANUM(c) IN_RANGE((c), 'a', 'z') || \
                       IN_RANGE((c), 'A', 'Z') || \
                       IS_DIGIT((c))
#define MAX_VALUES 4   // size of mem[] array.
#define MAX_PROTECTED 10 // size of protect_stack[]                                             
