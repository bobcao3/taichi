import 'dart:ffi';
import 'dart:io';
import 'dart:mirrors';

import 'package:ffi/ffi.dart';

import 'package:analyzer/dart/analysis/utilities.dart';
import 'package:analyzer/dart/ast/ast.dart';
import 'package:analyzer/dart/ast/token.dart';
import 'package:analyzer/dart/ast/visitor.dart';

import 'ticore_bindings.dart';
import 'core.dart' show program, ticore;

class Visitor extends ThrowingAstVisitor<dynamic> {
  final Map<String, int> idMap = {};
  int idCounter = 0;

  int getId(String str) {
    if (idMap.containsKey(str)) {
      return idMap[str]!;
    }
    final id = idCounter++;
    idMap[str] = id;
    return id;
  }

  @override
  ti_block? visitCompilationUnit(CompilationUnit node) {
    for (var decl in node.declarations) {
      if (decl is FunctionDeclaration) {
        return visitFunctionExpression(decl.functionExpression);
      } else {
        throw UnimplementedError("Unsupported declaration type: ${node.runtimeType}");
      }
    }
    return null;
  }

  @override
  List<ti_stmt> visitReturnStatement(ReturnStatement node) {
    ti_stmt stmt;

    if (node.expression != null) {
      ti_expr value = node.expression!.accept(this);
      final pValue = malloc.allocate<ti_expr>(sizeOf<Pointer>());
      pValue[0] = value;
      stmt = ticore.ti_make_stmt_frontend_return(1, pValue);
      malloc.free(pValue);
    } else {
      stmt = ticore.ti_make_stmt_frontend_return(0, Pointer<ti_expr>.fromAddress(0));
    }

    return [stmt];
  }

  @override
  List<ti_stmt> visitVariableDeclaration(VariableDeclaration node) {
    final nameCStr = node.name.name.toNativeUtf8().cast<Char>();
    final id = getId(node.name.name);
    final stmts = [ticore.ti_make_stmt_frontend_alloca(id, nameCStr, TiDataType.TI_DATA_TYPE_UNKNOWN)];
    if (node.initializer != null) {
      ti_expr valueExpr = node.initializer!.accept(this);
      ti_expr idExpr = ticore.ti_make_expr_id(id, nameCStr);
      stmts.add(ticore.ti_make_stmt_frontend_assign(idExpr, valueExpr));
    }
    malloc.free(nameCStr);
    return stmts;
  }

  @override
  List<ti_stmt> visitVariableDeclarationStatement(VariableDeclarationStatement node) {
    final stmts = <ti_stmt>[];
    for (final varDecl in node.variables.variables) {
      stmts.addAll(varDecl.accept(this));
    }
    return stmts;
  }

  @override
  ti_expr visitParenthesizedExpression(ParenthesizedExpression node) {
    return node.expression.accept(this);
  }

  List<ti_stmt> visitStatements(NodeList<Statement> nodes) {
    final stmts = <ti_stmt>[];
    for (var stmt in nodes) {
      stmts.addAll(stmt.accept(this));
    }
    return stmts;
  }

  @override
  ti_block visitFunctionExpression(FunctionExpression node) {
    final blockStmts = <ti_stmt>[];
    
    final body = node.body;
    if (body is EmptyFunctionBody) {
      Null;
    } else if (body is BlockFunctionBody) {
      blockStmts.addAll(visitStatements(body.block.statements));
    } else if (body is ExpressionFunctionBody) {
      blockStmts.add(body.expression.accept(this));
    } else {
      throw UnimplementedError("Unsupported function body type: ${body.runtimeType}");
    }

    final CBlockStmts = malloc.allocate<ti_stmt>(sizeOf<Pointer>() * blockStmts.length);
    for (var i = 0; i < blockStmts.length; i++) {
      CBlockStmts[i] = blockStmts[i];
    }
    final block = ticore.ti_make_block(blockStmts.length, CBlockStmts);
    malloc.free(CBlockStmts);

    return block;
  }

  @override
  ti_expr visitBinaryExpression(BinaryExpression node) {
    final lhs = node.leftOperand.accept(this);
    final rhs = node.rightOperand.accept(this);

    assert(lhs is ti_expr);
    assert(rhs is ti_expr);

    int op;
    if (node.operator.type == TokenType.PLUS) {
      op = ti_binary_op.TI_BINARY_OP_ADD;
    } else if (node.operator.type == TokenType.MINUS) {
      op = ti_binary_op.TI_BINARY_OP_SUB;
    } else if (node.operator.type == TokenType.STAR) {
      op = ti_binary_op.TI_BINARY_OP_MUL;
    } else if (node.operator.type == TokenType.SLASH) {
      op = ti_binary_op.TI_BINARY_OP_DIV;
    } else {
      throw UnimplementedError("Unsupported binary op type: ${node.operator.type}");
    }

    final expr = ticore.ti_make_expr_binary_op(op, lhs, rhs);

    return expr;
  }

  @override
  ti_expr visitSimpleIdentifier(SimpleIdentifier node) {
    final nameCStr = node.name.toNativeUtf8().cast<Char>();
    final ti_expr expr = ticore.ti_make_expr_id(getId(node.name), nameCStr);
    malloc.free(nameCStr);

    return expr;
  }

  @override
  ti_expr visitIntegerLiteral(IntegerLiteral node) {
    final ti_expr expr = ticore.ti_make_expr_const_int64(TiDataType.TI_DATA_TYPE_I32, node.value!);
    return expr;
  }

  @override
  ti_expr visitDoubleLiteral(DoubleLiteral node) {
    final ti_expr expr = ticore.ti_make_expr_const_double(TiDataType.TI_DATA_TYPE_F32, node.value);
    return expr;
  }
}

void transformAst(Function func) {
  final instance = reflect(func);
  final closure = instance as ClosureMirror;
  final method = closure.function;
  print(method.source);

  final dartAst = parseString(content : method.source!).unit;
  final v = Visitor();
  ti_block irNode = dartAst.accept(v);

  ticore.ti_print_ast(irNode);

  final nameCStr = func.runtimeType.toString().toNativeUtf8().cast<Char>();
  final kernel = ticore.ti_kernel_create(program, irNode, nameCStr);
  malloc.free(nameCStr);

  final kernelHandle = ticore.ti_program_compile_kernel(program, kernel);
  print(kernelHandle);
}
