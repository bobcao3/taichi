import 'dart:ffi';

import 'package:taichi/taichi.dart' as ti;

double taichi_test_add() {
  var a = 12.0;
  var b = 4;
  return a + (b * 5 - 4.0);
}

void main() {
  ti.init();

  ti.transformAst(taichi_test_add);

  ti.terminate();
}
