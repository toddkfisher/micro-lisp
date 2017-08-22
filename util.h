#pragma once

#define TDS(t) typedef struct t t

#ifdef DEBUG
#define DBG_MSG(msg) printf("! " msg "\n");

#define DBG_FN_MSG(msg) \
  printf("! in %s() : " #msg "\n", __func__)

#define DBG_FN_PRINT_VAR(var, fmt)                          \
  printf("! In %s() : " #var " == " fmt "\n", __func__, var)

#define DBG_PRINT_VAR(var, fmt)                     \
  printf("!" #var " == " fmt "\n", var)

#define DBG_FN_PRINT_LISP_VAR(var)              \
  do {                                          \
    printf("! In %s() : ", __func__);           \
    print_lisp_value(var, 1);                   \
  } while(0)

#define DBG_PRINT_LISP_VAR(var) print_lisp_value(var, 1)
#else
#define DBG_MSG(msg)

#define DBG_FN_MSG(msg)

#define DBG_FN_PRINT_VAR(var, fmt)

#define DBG_PRINT_VAR(var, fmt)

#define DBG_FN_PRINT_LISP_VAR(var)

#define DBG_PRINT_LISP_VAR(var)
#endif
