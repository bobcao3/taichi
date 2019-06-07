import taichi_lang as ti
import numpy as np
import random
import cv2

real = ti.f32
dim = 2
n_particles = 4096
n_grid = 32
dx = 1.0 / n_grid
inv_dx = 1.0 / dx
dt = 1e-3

p_mass = 1.0
p_vol = 1.0
E = 1e3


def scalar():
  return ti.var(dt=real)


def vec():
  return ti.Vector(dim, dt=real)


def mat():
  return ti.Matrix(dim, dim, dt=real)


x, v = vec(), vec()
grid_v = vec()
grid_m = scalar()
C = mat()
J = scalar()


@ti.layout
def place():
  ti.root.dense(ti.k, n_particles).place(x, v, J, C)
  ti.root.dense(ti.ij, n_grid).place(grid_v, grid_m)
  ti.cfg().print_ir = True

@ti.kernel
def clear_grid():
  for i, j in grid_m:
    grid_v[i, j].assign(ti.Vector([0.0, 0.0]))
    grid_m[i, j].assign(0.0)


@ti.kernel
def p2g():
  for p in x(0):
    base = ti.cast(x[p] * inv_dx - 0.5, ti.i32)
    fx = x[p] * inv_dx - ti.cast(base, ti.f32)
    w = [0.5 * ti.sqr(1.5 - fx), 0.75 - ti.sqr(fx - 1.0), 0.5 * ti.sqr(fx - 0.5)]
    stress = dt * p_vol * (J[p] - 1.0) * 4.0
    for i in ti.static(range(3)):
      for j in ti.static(range(3)):
        dpos = (ti.cast(ti.Vector([i, j]), ti.f32) - fx) * dx
        weight = w[i](0) * w[j](1)
        grid_v[base(0) + i, base(1) + j].assign(
          grid_v[base(0) + i, base(1) + j] + p_mass * weight * v[p] + stress * dpos)
        grid_m[base(0) + i, base(1) + j].assign(
          grid_m[base(0) + i, base(1) + j] + p_mass * weight)


@ti.kernel
def grid_op():
  for i, j in grid_m:
    if grid_m[i, j] > 0.0:
      inv_m = 1.0 / grid_m[i, j]
      grid_v[i, j] = inv_m * grid_v[i, j]
      grid_v(1)[i, j] = grid_v(1)[i, j] - dt * 9.8
      if j < 5:
        if grid_v(1)[i, j] < 0.0:
          grid_v(1)[i, j] = 0.0


@ti.kernel
def g2p():
  for p in x(0):
    base = ti.cast(x[p] * inv_dx - 0.5, ti.i32)
    fx = x[p] * inv_dx - ti.cast(base, ti.f32)
    w = [0.5 * ti.sqr(1.5 - fx), 0.75 - ti.sqr(fx - 1.0), 0.5 * ti.sqr(fx - 0.5)]
    new_v = ti.Vector([0.0, 0.0])
    new_C = ti.Matrix([[0.0, 0.0], [0.0, 0.0]])

    for i in ti.static(range(3)):
      for j in ti.static(range(3)):
        dpos = (ti.cast(ti.Vector([i, j]), ti.f32) - fx) * dx
        g_v = grid_v[base(0) + i, base(1) + j]
        weight = w[i](0) * w[j](1)
        new_v = new_v + weight * g_v
        new_C = new_C + 4.0 * inv_dx * weight * ti.Matrix.outer_product(g_v, dpos)
    v[p].assign(new_v)
    x[p] = x[p] + dt * v[p]
    J[p] = J[p] * (1.0 - dt * (new_C(0, 0) + new_C(1, 1)))



def main():
  for i in range(n_particles):
    x[i] = [random.random() * 0.4 + 0.2, random.random() * 0.4 + 0.2]
    v[i] = [0, -1]
    J[i] = 1.0

  for f in range(100):
    for s in range(10):
      clear_grid()
      p2g()
      grid_op()
      g2p()
    scale = 20
    img = np.zeros(shape=(scale * n_grid, scale * n_grid))
    '''
    for i in range(scale * n_grid):
      for j in range(scale * n_grid):
        img[i, j] = grid_m[i // scale, j // scale] * 10
    print(v(0)[0])
    '''
    for i in range(n_particles):
      p_x = int(scale * x(0)[i] / dx)
      p_y = int(scale * x(1)[i] / dx)
      img[p_x, p_y] = 1
    cv2.imshow('MPM', img.swapaxes(0, 1)[::-1])
    cv2.waitKey(1)


if __name__ == '__main__':
  main()
