#include "taichi/gui/backend.h"

TI_NAMESPACE_BEGIN

namespace gui {

static std::string glfw_backend_shader_vertex = R"V0G0N(

#version 430 core

in (location = 0) vec2 v_pos;
in (location = 1) vec2 v_uv;
in (location = 2) vec4 v_color;

out (location = 0) vec2 uv;
out (location = 1) vec4 color;

void main() {
  uv = v_uv;
  color = v_color;

  gl_Position = vec4(v_pos, 0.0, 1.0);
}

)V0G0N";

static std::string glfw_backend_shader_fragment = R"V0G0N(

#version 430 core

in (location = 0) vec2 uv;
in (location = 1) vec4 color;

out (location = 0) vec4 frag_color;

layout (binding = 0) uniform tex_id;
layout (binding = 1) uniform sampler2D tex;

void main() {
  if (tex_id > 0) {
    frag_color = color * texture(tex, uv);
  } else {
    frag_color = color;
  }
}

)V0G0N";

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

  // Load shaders
  auto load_shader = [](std::string &src, GLuint type) {
    auto s = glCreateShader(type);
    const char *buf = src.data();
    glShaderSource(s, 1, &buf, NULL);
    glCompileShader(s);
    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(s, 512, NULL, infoLog);
      TI_ERROR("Shader compilation failed");
    };
    return s;
  };

  auto vertex = load_shader(glfw_backend_shader_vertex, GL_VERTEX_SHADER);
  auto fragment = load_shader(glfw_backend_shader_fragment, GL_FRAGMENT_SHADER);

  shader_program = glCreateProgram();
  {
    glAttachShader(shader_program, vertex);
    glAttachShader(shader_program, fragment);
    glLinkProgram(shader_program);
    GLint success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
      TI_ERROR("Shader linking failed");
    }
  }

  glDeleteShader(vertex);
  glDeleteShader(fragment);

  // Create buffers
  glCreateBuffers(1, &streaming_vertices);
  glCreateBuffers(1, &streaming_indicies);
  glCreateBuffers(1, &streaming_drawcmds);

  // Create VAO
  glGenVertexArrays(1, &vertex_array_object);
  glBindVertexArray(vertex_array_object);
  glBindBuffer(GL_ARRAY_BUFFER, streaming_vertices);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, streaming_indicies);
  glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Vertex),
                        (void *)offsetof(Vertex, Vertex::p));
  glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(Vertex),
                        (void *)offsetof(Vertex, Vertex::uv));
  glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, false, sizeof(Vertex),
                        (void *)offsetof(Vertex, Vertex::c));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glBindVertexArray(0);
}

bool GLFWBackendContext::is_supported() const {
  // TODO: Query support
  return true;
}

void GLFWBackendContext::set_window_size(uint32_t width, uint32_t height) {
  this->width = width;
  this->height = height;
  glfwSetWindowSize(window, width, height);
}

void GLFWBackendContext::set_window_title(std::string title) {
  glfwSetWindowTitle(window, title.data());
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
  uint32_t current_texture = 0;

  glBindBuffer(GL_ARRAY_BUFFER, streaming_vertices);
  glBufferData(GL_ARRAY_BUFFER, list.vertices.size() * sizeof(Vertex),
               list.vertices.data(), GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, streaming_indicies);
  glBufferData(GL_ARRAY_BUFFER, list.indicies.size() * sizeof(uint16_t),
               list.indicies.data(), GL_STREAM_DRAW);

  struct GLDrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
  };

  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, streaming_drawcmds);
  glBufferData(GL_DRAW_INDIRECT_BUFFER,
               list.cmds.size() * sizeof(GLDrawElementsIndirectCommand),
               nullptr, GL_STREAM_DRAW);

  glUseProgram(shader_program);
  glBindVertexArray(vertex_array_object);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, streaming_indicies);

  size_t cmd_offset = 0;
  size_t num_cmds = 0;

  auto *indirect_mapped = (GLDrawElementsIndirectCommand *)glMapBuffer(
      GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

  for (auto &cmd : list.cmds) {
    if (current_texture != cmd.texture_id) {
      if (num_cmds != 0) {
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glMultiDrawElementsIndirect(
            GL_TRIANGLES, GL_UNSIGNED_SHORT,
            (void *)(cmd_offset * sizeof(GLDrawElementsIndirectCommand)),
            num_cmds, sizeof(GLDrawElementsIndirectCommand));

        // TODO: Figure out whether this is needed
        // When does OpenGL flush the write cache?
        indirect_mapped = (GLDrawElementsIndirectCommand *)glMapBuffer(
            GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        num_cmds = 0;
      }

      glBindTexture(GL_TEXTURE_2D, cmd.texture_id);
    }

    auto &gpucmd = indirect_mapped[cmd_offset];
    gpucmd.baseInstance = 0;
    gpucmd.instanceCount = 1;
    gpucmd.baseVertex = cmd.vertex_offset;
    gpucmd.firstIndex = cmd.first_index;
    gpucmd.count = cmd.elem_count * 3;

    cmd_offset++;
    num_cmds++;
  }

  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

  if (num_cmds != 0) {
    glMultiDrawElementsIndirect(
        GL_TRIANGLES, GL_UNSIGNED_SHORT,
        (void *)(cmd_offset * sizeof(GLDrawElementsIndirectCommand)), num_cmds,
        sizeof(GLDrawElementsIndirectCommand));
  }

  glBindVertexArray(0);
  glUseProgram(0);
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
  auto iter =
      std::find(gl_texture_handles.begin(), gl_texture_handles.end(), image_id);

  if (iter == gl_texture_handles.end()) {
    TI_WARN("Texture is not owned by the GUI backend");
    return;
  }

  gl_texture_handles.erase(iter);

  glDeleteTextures(1, &image_id);
}

// Inputs
bool GLFWBackendContext::is_key_down(std::string key) {
  return false;
}

GLFWBackendContext::~GLFWBackendContext() {
  // TODO: Check whether we still need the context for the OpenGL compute
  // backend
  glfwTerminate();
}

};  // namespace gui

TI_NAMESPACE_END