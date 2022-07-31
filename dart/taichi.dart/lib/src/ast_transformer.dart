import 'dart:mirrors';

import 'package:analyzer/dart/analysis/utilities.dart';
import 'package:analyzer/dart/analysis/features.dart';
import 'package:analyzer/dart/ast/ast.dart';
import 'package:analyzer/dart/ast/visitor.dart';

class Visitor extends RecursiveAstVisitor {
  @override
  visitFunctionDeclarationStatement(FunctionDeclarationStatement node) {
    // TODO: implement visitFunctionDeclarationStatement
    print("Func def: ${node.functionDeclaration.name.name}");
    return super.visitFunctionDeclarationStatement(node);
  }

  @override
  visitReturnStatement(ReturnStatement node) {
    print("Return -");
    return super.visitReturnStatement(node);
  }

  @override
  visitFunctionExpression(FunctionExpression node) {
    print("Func: ${node.parameters} -> ?");
    return super.visitFunctionExpression(node);
  }

  @override
  visitBinaryExpression(BinaryExpression node) {
    print("Op: ${node.operator.lexeme} \\");
    return super.visitBinaryExpression(node);
  }

  @override
  visitSimpleIdentifier(SimpleIdentifier node) {
    print("Id: ${node.name}");
    return super.visitSimpleIdentifier(node);
  }

  @override
  visitIntegerLiteral(IntegerLiteral node) {
    print("Int? ${node.value}");
    return super.visitIntegerLiteral(node);
  }

  @override
  visitDoubleLiteral(DoubleLiteral node) {
    print("Double? ${node.value}");
    return super.visitDoubleLiteral(node);
  }
}

void transformAst(Function func) {
  final instance = reflect(func);
  final closure = instance as ClosureMirror;
  final method = closure.function;
  print(method.source);

  var dartAst = parseString(content : method.source!).unit;
  var v = Visitor();
  dartAst.accept(v);
}
