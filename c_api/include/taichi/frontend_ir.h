#ifndef __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__
#define __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "taichi_platform.h"

typedef struct ti_ast_builder_class *ti_ast_builder;
typedef struct ti_expr_class *ti_expr;
typedef struct ti_stmt_class *ti_stmt;

#define MAKE_STMT(name) \
    TI_DLL_EXPORT ti_stmt TI_API_CALL ti_make_stmt_##name()

#define MAKE_EXPR(name) \
    TI_DLL_EXPORT ti_expr TI_API_CALL ti_make_expr_##name()

MAKE_STMT(frontend_external_func);
MAKE_STMT(frontend_expr);
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
MAKE_STMT(frontend_return);

MAKE_EXPR(arg_load);
MAKE_EXPR(rand);
MAKE_EXPR(unary_op);
MAKE_EXPR(binary_op);
MAKE_EXPR(ternary_op);
MAKE_EXPR(internal_func_call);
MAKE_EXPR(external_tensor);
MAKE_EXPR(global_variable);
MAKE_EXPR(index);
MAKE_EXPR(stride);
MAKE_EXPR(range_assumption);
MAKE_EXPR(loop_unique);
MAKE_EXPR(id);
MAKE_EXPR(atomic_op);
MAKE_EXPR(snode_op);
MAKE_EXPR(const);
MAKE_EXPR(external_tensor_shape_along_axis);
MAKE_EXPR(func_call);
MAKE_EXPR(mesh_patch_index);
MAKE_EXPR(mesh_relation_access);
MAKE_EXPR(mesh_index_conversion);
MAKE_EXPR(reference);
MAKE_EXPR(texture_op);
MAKE_EXPR(texture_ptr);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __INCLUDE_C_API_TAICHI_FRONTEND_IR_H__