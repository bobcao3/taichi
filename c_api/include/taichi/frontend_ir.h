#ifndef __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__
#define __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__

#include "taichi_core.h"
#include "taichi_platform.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
  
typedef struct ti_ast_builder_class *ti_ast_builder;
typedef struct ti_expr_class *ti_expr;
typedef struct ti_stmt_class *ti_stmt;

typedef enum {
  TI_BINARY_OP_MUL = 0,
  TI_BINARY_OP_ADD = 1,
  TI_BINARY_OP_SUB = 2,
  TI_BINARY_OP_TRUEDIV = 3,
  TI_BINARY_OP_FLOORDIV = 4,
  TI_BINARY_OP_DIV = 5,
  TI_BINARY_OP_MOD = 6,
  TI_BINARY_OP_MAX = 7,
  TI_BINARY_OP_MIN = 8,
  TI_BINARY_OP_BIT_AND = 9,
  TI_BINARY_OP_BIT_OR = 10,
  TI_BINARY_OP_BIT_XOR = 11,
  TI_BINARY_OP_BIT_SHL = 12,
  TI_BINARY_OP_BIT_SHR = 13,
  TI_BINARY_OP_BIT_SAR = 14,
  TI_BINARY_OP_CMP_LT = 15,
  TI_BINARY_OP_CMP_LE = 16,
  TI_BINARY_OP_CMP_GT = 17,
  TI_BINARY_OP_CMP_GE = 18,
  TI_BINARY_OP_CMP_EP = 19,
  TI_BINARY_OP_CMP_NE = 20,
  TI_BINARY_OP_ATAN2 = 21,
  TI_BINARY_OP_POW = 22,
  TI_BINARY_OP_UNDEFINED = 23,
  TI_BINARY_OP_LOGICAL_OR = 24,
  TI_BINARY_OP_LOGICAL_AND = 25,
  TI_BINARY_OP_MAX_ENUM
} ti_binary_op;

#define MAKE_STMT(name, ...) \
    TI_DLL_EXPORT ti_stmt TI_API_CALL ti_make_stmt_##name(__VA_ARGS__)

#define MAKE_EXPR(name, ...) \
    TI_DLL_EXPORT ti_expr TI_API_CALL ti_make_expr_##name(__VA_ARGS__)

TI_DLL_EXPORT void TI_API_CALL ti_release_stmt(ti_stmt stmt);
TI_DLL_EXPORT void TI_API_CALL ti_release_expr(ti_expr expr);

MAKE_STMT(frontend_external_func,
          void *func_addr,
          const char *asm_source,
          const char *bc_filename,
          const char *bc_funcname,
          int n_args,
          ti_expr *args,
          int n_outputs,
          ti_expr *outputs);
MAKE_STMT(frontend_expr, ti_expr expr);
MAKE_STMT(frontend_if);
MAKE_STMT(frontend_for);
MAKE_STMT(frontend_print);
MAKE_STMT(frontend_while);
MAKE_STMT(frontend_break);
MAKE_STMT(frontend_continue);
MAKE_STMT(frontend_alloca);
MAKE_STMT(frontend_assign);
MAKE_STMT(frontend_eval);
MAKE_STMT(frontend_snode_op);
MAKE_STMT(frontend_assert);
MAKE_STMT(frontend_func_def);
MAKE_STMT(frontend_return, int n_values, ti_expr *values);

MAKE_EXPR(arg_load);
MAKE_EXPR(rand);
MAKE_EXPR(unary_op);
MAKE_EXPR(binary_op, ti_binary_op op, ti_expr lhs, ti_expr rhs);
MAKE_EXPR(ternary_op);
MAKE_EXPR(internal_func_call);
MAKE_EXPR(external_tensor);
MAKE_EXPR(global_variable);
MAKE_EXPR(index);
MAKE_EXPR(stride);
MAKE_EXPR(range_assumption);
MAKE_EXPR(loop_unique);
MAKE_EXPR(id, int id, const char *name);
MAKE_EXPR(atomic_op);
MAKE_EXPR(snode_op);
MAKE_EXPR(const_double, TiDataType dtype, double value);
MAKE_EXPR(const_int32, TiDataType dtype, int value);
MAKE_EXPR(const_int64, TiDataType dtype, int64_t value);
MAKE_EXPR(external_tensor_shape_along_axis);
MAKE_EXPR(func_call);
MAKE_EXPR(mesh_patch_index);
MAKE_EXPR(mesh_relation_access);
MAKE_EXPR(mesh_index_conversion);
MAKE_EXPR(reference);
MAKE_EXPR(texture_op);
MAKE_EXPR(texture_ptr);

#undef MAKE_STMT
#undef MAKE_EXPR

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__