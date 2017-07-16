//------------------------------------------------------------------------------
/// Utility functions

void error(
  char *msg
);

void fatal(
  char *msg
);

int sym_eq(
  LISP_VALUE *x,
  LISP_VALUE *y
);

LISP_VALUE *cons(
  LISP_VALUE *x,
  LISP_VALUE *y
);

LISP_VALUE *car(
  LISP_VALUE *x
);

LISP_VALUE *cdr(
  LISP_VALUE *x
);

LISP_VALUE *cadr(
  LISP_VALUE *x
);

LISP_VALUE *caddr(
  LISP_VALUE *x
);

//------------------------------------------------------------------------------
/// Value creation routines.

LISP_VALUE *create_intnum(
  int n
);

LISP_VALUE *create_nil(void);

LISP_VALUE *create_symbol(
  char *name
);

//------------------------------------------------------------------------------
/// Read/write routines

void print_lisp_value_aux(
  LISP_VALUE *val,
  int nest_level,
  int has_items_following
);

void print_lisp_value(
  LISP_VALUE *val,
  int print_newline
);

void next_char(void);

void skip_blanks(void);

LISP_VALUE *read_atom(void);

LISP_VALUE *read_lisp_value(void);

LISP_VALUE *read_list(void);

//------------------------------------------------------------------------------
/// Memory/gc-related routines.

void dump_protect_stack(void);

void gc(void);

void mark(void);

void sweep(void);

void indent(
  int n
);

void gc_walk(
  LISP_VALUE *v,
  int depth
);

void collect(void);

void protect_from_gc(
  LISP_VALUE *v
);

void unprotect_from_gc(void);

void init_free_list(void);

LISP_VALUE *new_value(
  int value_type
);

//------------------------------------------------------------------------------
/// Environment-related routines

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

// This function may not be called after any closure creation since
// it modifies global_env and (an|the) outer environment of all closures
// is global.  Also: this function does not prevent duplicate names
// from being added to global_env.
void global_env_init(
  LISP_VALUE *name,
  LISP_VALUE *value
);

// This function may be safely called after any closure creation.
// global_env must contain at least one name/value pair.
void global_env_extend(
  LISP_VALUE *name,
  LISP_VALUE *value
);

//------------------------------------------------------------------------------
/// Built-in functions and syntax.

LISP_VALUE *stx_dumpenv(
  LISP_VALUE *env
);

LISP_VALUE *stx_quote(
  LISP_VALUE *arg,
  LISP_VALUE *env
);

LISP_VALUE *stx_setq(
  LISP_VALUE *name,
  LISP_VALUE *val,
  LISP_VALUE *env
);

LISP_VALUE *fn_add(
  LISP_VALUE *x,
  LISP_VALUE *y,
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

LISP_VALUE *eval_builtin(
  BUILTIN_INFO *pinfo,
  LISP_VALUE *arglist,
  LISP_VALUE *env
);

BUILTIN_INFO *get_keyword_info(
  LISP_VALUE *kw_sym
);

LISP_VALUE *eval(
  LISP_VALUE *expr,
  LISP_VALUE *env
);
