#include "taichi/frontend_ir.h"

#include "taichi/ir/expr.h"
#include "taichi/ir/statements.h"
#include "taichi/ir/frontend_ir.h"
#include "taichi/ir/transforms.h"

#define MAKE_STMT(name, ...) \
  TI_DLL_EXPORT ti_stmt TI_API_CALL ti_make_stmt_##name(__VA_ARGS__)

#define MAKE_EXPR(name, ...) \
  TI_DLL_EXPORT ti_expr TI_API_CALL ti_make_expr_##name(__VA_ARGS__)

using namespace taichi;
using namespace lang;

namespace {

Stmt *to_cpp(ti_stmt stmt) {
  return reinterpret_cast<Stmt *>(stmt);
}
  
Expr &to_cpp(ti_expr expr) {
  return *reinterpret_cast<Expr *>(expr);
}

ti_stmt to_cptr(std::unique_ptr<Stmt> &stmt) {
  return ti_stmt(stmt.release());
}

ti_expr to_cptr(const Expr &expr) {
  return ti_expr(new Expr(expr));
}

std::vector<Expr> exprptrs_to_vec_expr(int n, ti_expr *exprs) {
  std::vector<Expr> v_exprs(n);
  for (int i = 0; i < n; i++) {
    v_exprs[i] = to_cpp(exprs[i]);
    ti_release_expr(exprs[i]);
    exprs[i] = nullptr;
  }
  return v_exprs;
}

}

extern "C" {

void ti_release_stmt(ti_stmt stmt) {
  delete reinterpret_cast<Stmt *>(stmt);
}

void ti_release_expr(ti_expr expr) {
  delete reinterpret_cast<Expr *>(expr);
}

void ti_print_ast(ti_block root) {
  std::string str;
  irpass::print(reinterpret_cast<Block *>(root), &str);
  TI_WARN("AST: \n{}", str);
}

ti_block ti_make_block(int n_stmts, ti_stmt *stmts) {
  Block *b = new Block;
  for (int i = 0; i < n_stmts; i++) {
    b->insert(std::unique_ptr<Stmt>(to_cpp(stmts[i])));
  }
  return ti_block(b);
}

MAKE_STMT(frontend_external_func,
          void *func_addr,
          const char *asm_source,
          const char *bc_filename,
          const char *bc_funcname,
          int n_args,
          ti_expr *args,
          int n_outputs,
          ti_expr *outputs) {
  std::vector<Expr> v_args = exprptrs_to_vec_expr(n_args, args);
  std::vector<Expr> v_outputs = exprptrs_to_vec_expr(n_outputs, outputs);
  std::unique_ptr<Stmt> stmt = Stmt::make<FrontendExternalFuncStmt>(
      func_addr, std::string(asm_source), std::string(bc_filename),
      std::string(bc_funcname), v_args, v_outputs);
  return to_cptr(stmt);
}

MAKE_STMT(frontend_expr, ti_expr expr) {
  std::unique_ptr<Stmt> stmt = Stmt::make<FrontendExprStmt>(to_cpp(expr));
  return to_cptr(stmt);
}

MAKE_STMT(frontend_if) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_for) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_print) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_while) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_break) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_continue) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_alloca, int id, const char *name, TiDataType dtype) {
  std::unique_ptr<Stmt> stmt = Stmt::make<FrontendAllocaStmt>(
      Identifier(id, std::string(name)),
      PrimitiveType::get(PrimitiveTypeID(dtype)));
  return to_cptr(stmt);
}

MAKE_STMT(frontend_assign, ti_expr lhs, ti_expr val) {
  std::unique_ptr<Stmt> stmt =
      Stmt::make<FrontendAssignStmt>(to_cpp(lhs), to_cpp(val));
  return to_cptr(stmt);
}

MAKE_STMT(frontend_eval) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_snode_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_assert) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_func_def) {
  TI_NOT_IMPLEMENTED;
}

MAKE_STMT(frontend_return, int n_values, ti_expr *values) {
  ExprGroup v_values;
  v_values.exprs = exprptrs_to_vec_expr(n_values, values);
  std::unique_ptr<Stmt> stmt = Stmt::make<FrontendReturnStmt>(v_values);
  return to_cptr(stmt);
}

MAKE_EXPR(arg_load) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(rand) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(unary_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(binary_op, ti_binary_op op, ti_expr lhs, ti_expr rhs) {
  Expr expr_lhs = to_cpp(lhs);
  Expr expr_rhs = to_cpp(rhs);
  ti_expr expr = to_cptr(
      Expr::make<BinaryOpExpression>(BinaryOpType(op), expr_lhs, expr_rhs));
  ti_release_expr(lhs);
  ti_release_expr(rhs);
  return expr;
}

MAKE_EXPR(ternary_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(internal_func_call) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(external_tensor) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(global_variable) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(index) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(stride) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(range_assumption) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(loop_unique) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(id, int id, const char* name) {
  return to_cptr(Expr::make<IdExpression>(Identifier(id, std::string(name))));
}

MAKE_EXPR(atomic_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(snode_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(const_double, TiDataType dtype, double value) {
  PrimitiveTypeID dt = PrimitiveTypeID(dtype);
  return to_cptr(Expr::make<ConstExpression>(PrimitiveType::get(dt), value));
}

MAKE_EXPR(const_int32, TiDataType dtype, int value) {
  PrimitiveTypeID dt = PrimitiveTypeID(dtype);
  return to_cptr(Expr::make<ConstExpression>(PrimitiveType::get(dt), value));
}

MAKE_EXPR(const_int64, TiDataType dtype, int64_t value) {
  PrimitiveTypeID dt = PrimitiveTypeID(dtype);
  return to_cptr(Expr::make<ConstExpression>(PrimitiveType::get(dt), value));
}

MAKE_EXPR(external_tensor_shape_along_axis) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(func_call) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(mesh_patch_index) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(mesh_relation_access) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(mesh_index_conversion) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(reference) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(texture_op) {
  TI_NOT_IMPLEMENTED;
}

MAKE_EXPR(texture_ptr) {
  TI_NOT_IMPLEMENTED;
}
  
}
