import taichi as ti

ti.init(arch=ti.cpu)

@ti.kernel
def taichi_test_add(a : ti.f32, b : ti.i32) -> ti.f32:
  c = a * b - 12.0
  d = a + 4
  return c + (d * 5 - 4.0)

print(taichi_test_add(2.0, 4))
