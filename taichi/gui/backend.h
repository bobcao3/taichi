#pragma once

#include "taichi/math/math.h"
#include "taichi/system/timer.h"
#include "taichi/program/kernel_profiler.h"

#include <atomic>
#include <ctime>
#include <numeric>
#include <unordered_map>

#if defined(TI_PLATFORM_LINUX)
#define TI_GUI_X11
#endif

#if defined(TI_PLATFORM_WINDOWS)
#define TI_GUI_WIN32
#endif

#if defined(TI_PLATFORM_OSX)
#define TI_GUI_COCOA
#include <objc/objc.h>
#endif

TI_NAMESPACE_BEGIN

namespace gui {

enum class DrawMode : uint8_t {
  // No ordering requirement
  TRIANGLES,
  // Must be in this order: (0, 0) -> (1, 0) -> (1, 1) -> (0, 1)
  AXIS_ALIGNED_BOX,
  // Must be in this order: vertex -> control point -> vertex (Quadratic bezier)
  BEZIER,        // Render the convex side
  BEZIER_INSET,  // Render the concave side
};

struct DrawCmd {
  DrawMode mode;

  uint32_t vertex_offset;
  uint32_t first_index;
  uint32_t elem_count;
  uint32_t texture_id;

  bool enable_clipping = false;
  Pos clip_min, clip_max;
};

struct Pos {
  float x, y;
};

struct Color {
  uint8_t r, g, b, a;
};

struct Vertex {
  Pos p;
  Pos uv;
  Color c;
};

constexpr Pos static_circle[] = {{1.0, 0.0},
                                 {0.9238795325112867, 0.3826834323650898},
                                 {0.7071067811865476, 0.7071067811865476},
                                 {0.38268343236508984, 0.9238795325112867},
                                 {0.0, 1.0},
                                 {-0.3826834323650897, 0.9238795325112867},
                                 {-0.7071067811865475, 0.7071067811865476},
                                 {-0.9238795325112867, 0.3826834323650899},
                                 {-1.0, 0.0},
                                 {-0.9238795325112868, -0.38268343236508967},
                                 {-0.7071067811865477, -0.7071067811865475},
                                 {-0.38268343236509034, -0.9238795325112865},
                                 {0.0, -1.0},
                                 {0.38268343236509, -0.9238795325112866},
                                 {0.7071067811865474, -0.7071067811865477},
                                 {0.9238795325112865, -0.3826834323650904}};

struct DrawList {
  std::vector<DrawCmd> cmds;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indicies;

  // Helpers to render common primitives
  inline DrawCmd &add_rectangle(Pos p0,
                                Pos p1,
                                Color color,
                                bool filled = true) {
    uint32_t vertex_head = vertices.size();
    uint32_t index_head = indicies.size();
    vertices.push_back({p0, {}, color});
    vertices.push_back({p1, {}, color});
    indicies.push_back(0);
    indicies.push_back(1);
    cmds.push_back({DrawMode::AXIS_ALIGNED_BOX, vertex_head, index_head, 1, 0});
  }

  inline DrawCmd &add_circle(Pos p0,
                             float radius,
                             Color color,
                             bool filled = true) {
    uint32_t vertex_head = vertices.size();
    uint32_t index_head = indicies.size();
    vertices.push_back({p0, {}, color});

    for (int i = 0; i < sizeof(static_circle); i++) {
      Pos p = static_circle[i];
      vertices.push_back(Vertex{p, {}, color});
      indicies.push_back(0);
      indicies.push_back(1 + i);
      indicies.push_back(1 + (i + 1) % 16);
    }

    cmds.push_back({DrawMode::TRIANGLES, vertex_head, index_head, 16, 0});
  }

  inline DrawCmd &add_triangle(Pos v0,
                               Pos v1,
                               Pos v2,
                               Pos uv0,
                               Pos uv1,
                               Pos uv2,
                               Color c0,
                               Color c1,
                               Color c2) {
  }

  inline DrawCmd &add_image(Pos p0,
                            Pos p1,
                            Pos uv0,
                            Pos uv1,
                            uint32_t texture_id) {
  }
};

// A backend context is a graphical context and a window that accepts inputs
class BackendContext {
 public:
  // Support query
  virtual bool is_supported() const {
    return false;
  };

  virtual void set_window_size(uint32_t width, uint32_t height) {
  }
  virtual void set_window_title(std::string title) {
  }

  // Presentation & rendering loop
  virtual bool new_frame() {
  }
  virtual void poll_events() {
  }
  virtual void submit_draw_list(DrawList &list) {
  }
  virtual void present_frame() {
  }

  // Resources
  // (Image will be copied to backend, either be on CPU memory or get copied
  // into GPU memory)
  // (Image id of 0 is always not bound / not textured)
  virtual uint32_t add_image(uint8_t *image, size_t width, size_t height) {
    return 0;
  }
  virtual uint32_t add_image(float *image, size_t width, size_t height) {
    return 0;
  }
  virtual uint32_t add_image(void *image,
                             size_t width,
                             size_t height,
                             uint8_t num_channels,
                             uint8_t channel_bits) {
    return 0;
  }
  virtual void remove_image(uint32_t image_id) {
  }

  // Inputs
  virtual bool is_key_down(std::string key) {
    return false;
  }

  virtual ~BackendContext() {
  }
};

#ifdef TI_WITH_OPENGL
#include <glad/glad.h>
#include "GLFW/glfw3.h"
#endif

// Platform backends
class GLFWBackendContext : BackendContext {
 private:
#ifdef TI_WITH_OPENGL
  GLFWwindow *window;

  uint32_t width = 400, height = 400;

  std::vector<GLuint> gl_texture_handles;
#endif

 public:
#ifdef TI_WITH_OPENGL
  GLFWBackendContext();

  // Support query
  bool is_supported() const override;

  void set_window_size(uint32_t width, uint32_t height) override;
  void set_window_title(std::string title) override;

  // Presentation & rendering loop
  bool new_frame() override;
  void poll_events() override;
  void submit_draw_list(DrawList &list) override;
  void present_frame() override;

  // Resources
  // (Image will be copied to backend, either be on CPU memory or get copied
  // into GPU memory)
  // (Image id of 0 is always not bound / not textured)
  uint32_t add_image(uint8_t *image, size_t width, size_t height) override;
  uint32_t add_image(float *image, size_t width, size_t height) override;
  uint32_t add_image(void *image,
                     size_t width,
                     size_t height,
                     uint8_t num_channels,
                     uint8_t channel_bits) override;
  void remove_image(uint32_t image_id) override;

  // Inputs
  bool is_key_down(std::string key) override;

  ~GLFWBackendContext();
#endif
};

}  // namespace gui

TI_NAMESPACE_END