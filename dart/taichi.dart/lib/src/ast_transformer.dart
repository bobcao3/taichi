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
  final List<ti_stmt> stmtCache = [];
  final List<ti_expr> exprCache = [];

  final Map<String, int> idMap = {};
  int idCounter = 0;

  var blockDepth = 0;
  var printNewLine = false;

  void treePrint(String str) {
    if (printNewLine) {
      for (var i = 0; i < blockDepth; i++) {
        stdout.write("  ");
      }
      printNewLine = false;
    }
    stdout.write(str);
    stdout.write(" ");
  }

  void treePrintLn(String str) {
    stdout.writeln(str);
    printNewLine = true;
  }

  int getId(String str) {
    if (idMap.containsKey(str)) {
      return idMap[str]!;
    }
    final id = idCounter++;
    idMap[str] = id;
    return id;
  }

  @override
  void visitCompilationUnit(CompilationUnit node) {
    for (var decl in node.declarations) {
      if (decl is FunctionDeclaration) {
        visitFunctionExpression(decl.functionExpression);
      } else {
        throw UnimplementedError("Unsupported declaration type: ${node.runtimeType}");
      }
    }
  }

  @override
  ti_expr visitParenthesizedExpression(ParenthesizedExpression node) {
    return node.expression.accept(this);
  }

  @override
  void visitFunctionDeclarationStatement(FunctionDeclarationStatement node) {
    treePrintLn("(def ${node.functionDeclaration.name.name})");
  }

  @override
  ti_stmt visitReturnStatement(ReturnStatement node) {
    treePrint("(ret");
    blockDepth += 1;

    ti_stmt stmt;

    if (node.expression != null) {
      ti_expr value = node.expression!.accept(this);
      final pValue = malloc.allocate<ti_expr>(4);
      pValue[0] = value;
      stmt = ticore.ti_make_stmt_frontend_return(1, pValue);
      malloc.free(pValue);
    } else {
      stmt = ticore.ti_make_stmt_frontend_return(0, Pointer<ti_expr>.fromAddress(0));
    }

    stmtCache.add(stmt);
    blockDepth -= 1;
    treePrintLn(")");

    return stmt;
  }

  @override
  void visitFunctionExpression(FunctionExpression node) {
    treePrintLn("(def ${node.parameters} -> ?");
    blockDepth += 1;
    
    final body = node.body;
    if (body is EmptyFunctionBody) {
      Null;
    } else if (body is BlockFunctionBody) {
      for (var stmt in body.block.statements) {
        stmt.accept(this);
      }
    } else if (body is ExpressionFunctionBody) {
      body.expression.accept(this);
    } else {
      throw UnimplementedError("Unsupported function body type: ${body.runtimeType}");
    }

    blockDepth -= 1;
    treePrint(")");
  }

  @override
  ti_expr visitBinaryExpression(BinaryExpression node) {
    treePrint("(${node.operator}");
    blockDepth += 1;

    final lhs = node.leftOperand.accept(this);
    final rhs = node.rightOperand.accept(this);

    assert(lhs is ti_expr);
    assert(rhs is ti_expr);

    blockDepth -= 1;
    treePrint(")");

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
    exprCache.add(expr);

    return expr;
  }

  @override
  ti_expr visitSimpleIdentifier(SimpleIdentifier node) {
    treePrint("\$${node.name}");

    final nameCStr = node.name.toNativeUtf8().cast<Char>();
    final ti_expr expr = ticore.ti_make_expr_id(getId(node.name), nameCStr);
    malloc.free(nameCStr);
    exprCache.add(expr);

    return expr;
  }

  @override
  ti_expr visitIntegerLiteral(IntegerLiteral node) {
    treePrint("${node.value}");

    final ti_expr expr = ticore.ti_make_expr_const_int64(TiDataType.TI_DATA_TYPE_I32, node.value!);
    exprCache.add(expr);

    return expr;
  }

  @override
  ti_expr visitDoubleLiteral(DoubleLiteral node) {
    treePrint("${node.value}f");

    final ti_expr expr = ticore.ti_make_expr_const_double(TiDataType.TI_DATA_TYPE_F32, node.value);
    exprCache.add(expr);

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
  dartAst.accept(v);
}
