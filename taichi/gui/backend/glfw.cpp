#include "taichi/gui/backend.h"

TI_NAMESPACE_BEGIN

namespace gui {

GLFWBackendContext::GLFWBackendContext() {
  // TODO: Check whether GLFW has been initialized by the OpenGL backend
  glfwInit();

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  window = glfwCreateWindow(width, height, "Taichi", nullptr, nullptr);

  // TODO: Check window initialization

  // TODO: Check whether OpenGL has been loaded by GLAD
  glfwMakeContextCurrent(window);
  gladLoadGL();
}

bool GLFWBackendContext::is_supported() const {
  // TODO: Query support
  return true;
}

bool GLFWBackendContext::new_frame() {
  // This is not really a thing in OpenGL
  // For platforms like Vulkan the program needs to acquire an image to render
  // into

  // We are just going to check whether the window should close
  return !glfwWindowShouldClose(window);
}

void GLFWBackendContext::poll_events() {
  glfwPollEvents();
}

void GLFWBackendContext::submit_draw_list(DrawList &list) {
}

void GLFWBackendContext::present_frame() {
  glfwSwapBuffers(window);
}

uint32_t GLFWBackendContext::add_image(uint8_t *image,
                                       size_t width,
                                       size_t height) {
  GLuint handle = 0;
  glGenTextures(1, &handle);

  glBindTexture(GL_TEXTURE_2D, handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image);
  glBindTexture(GL_TEXTURE_2D, 0);  // Minimize the time we have any state bound

  gl_texture_handles.push_back(handle);

  return handle;
}

uint32_t GLFWBackendContext::add_image(float *image,
                                       size_t width,
                                       size_t height) {
  GLuint handle = 0;
  glGenTextures(1, &handle);

  glBindTexture(GL_TEXTURE_2D, handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
               GL_FLOAT, image);
  glBindTexture(GL_TEXTURE_2D, 0);  // Minimize the time we have any state bound

  gl_texture_handles.push_back(handle);

  return handle;
}

uint32_t GLFWBackendContext::add_image(void *image,
                                       size_t width,
                                       size_t height,
                                       uint8_t num_channels,
                                       uint8_t channel_bits) {
  GLenum internal_format;
  GLenum data_type;
  GLenum format;

  if (num_channels == 1) {
    format = GL_R;
  } else if (num_channels = 2) {
    format = GL_RG;
  } else if (num_channels = 3) {
    format = GL_RGB;
  } else if (num_channels = 4) {
    format = GL_RGBA;
  } else {
    TI_WARN("Can not support images with more than 4 channels");
    format = 0;
  }

  if (channel_bits = 8) {
    if (num_channels == 1) {
      internal_format = GL_R8;
    } else if (num_channels = 2) {
      internal_format = GL_RG8;
    } else if (num_channels = 3) {
      internal_format = GL_RGB8;
    } else if (num_channels = 4) {
      internal_format = GL_RGBA8;
    } else {
      TI_WARN("Not supported internal format");
      internal_format = 0;
    }
    data_type = GL_UNSIGNED_BYTE;
  } else if (channel_bits = 16) {
    if (num_channels == 1) {
      internal_format = GL_R16;
    } else if (num_channels = 2) {
      internal_format = GL_RG16;
    } else if (num_channels = 3) {
      internal_format = GL_RGB16;
    } else if (num_channels = 4) {
      internal_format = GL_RGBA16;
    } else {
      TI_WARN("Not supported internal format");
      internal_format = 0;
    }
    data_type = GL_UNSIGNED_SHORT;
  } else if (channel_bits = 32) {
    // There isn't a UNorm type for 32bits channel
    if (num_channels == 1) {
      internal_format = GL_R32F;
    } else if (num_channels = 2) {
      internal_format = GL_RG32F;
    } else if (num_channels = 3) {
      internal_format = GL_RGB32F;
    } else if (num_channels = 4) {
      internal_format = GL_RGBA32F;
    } else {
      TI_WARN("Not supported internal format");
      internal_format = 0;
    }
    data_type = GL_FLOAT;
  }

  GLuint handle = 0;
  glGenTextures(1, &handle);

  glBindTexture(GL_TEXTURE_2D, handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
               GL_FLOAT, image);
  glBindTexture(GL_TEXTURE_2D, 0);

  gl_texture_handles.push_back(handle);

  return handle;
}

void GLFWBackendContext::remove_image(uint32_t image_id) {
  auto iter = std::find(gl_texture_handles.begin(), gl_texture_handles.end(), image_id);

  if (iter == gl_texture_handles.end()) {
    TI_WARN("Texture is not owned by the GUI backend");
    return;
  }

  gl_texture_handles.erase(iter);

  glDeleteTextures(1, &image_id);
}

// Inputs
bool GLFWBackendContext::is_key_down(std::string key) {
}

GLFWBackendContext::~GLFWBackendContext() {
  // TODO: Check whether we still need the context for the OpenGL compute
  // backend
  glfwTerminate();
}

};  // namespace gui

TI_NAMESPACE_END