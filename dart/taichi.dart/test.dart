import 'dart:ffi';

import 'package:taichi/taichi.dart' as ti;

double taichi_test_add(double a, double b) {
  return a + (b * 5 - 4.0);
}

void main() {
  ti.init();
  ti.terminate();

  ti.transformAst(taichi_test_add);
}
