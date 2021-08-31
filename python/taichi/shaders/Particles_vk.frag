#version 450

struct SceneUBO {
  vec3 camera_pos;
  mat4 view;
  mat4 projection;
  vec3 ambient_light;
  int point_light_count;
};

layout(binding = 0) uniform UBO {
  SceneUBO scene;
  mat4 view_inverse;
  vec3 color;
  int use_per_vertex_color;
  float radius;
  float window_width;
  float window_height;
  float tan_half_fov;
}
ubo;

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 pos_camera_space;
layout(location = 1) in vec3 selected_color;

float project_z(float view_z) {
  vec4 projected = ubo.scene.projection * vec4(0, 0, view_z, 1);
  return projected.z / projected.w;
}

vec3 to_camera_space(vec3 pos) {
  vec4 temp = ubo.scene.view * vec4(pos, 1.0);
  return temp.xyz / temp.w;
}

#include "lighting.glslinc"

void main() {
  vec2 coord2D;
  coord2D = gl_PointCoord * 2.0 - vec2(1);
  coord2D.y *= -1;

  if (length(coord2D) >= 1.0) {
    discard;
  }

  float z_in_sphere = sqrt(1 - coord2D.x * coord2D.x - coord2D.y * coord2D.y);
  vec3 coord_in_sphere = vec3(coord2D, z_in_sphere);

  vec3 frag_pos =
      pos_camera_space.xyz / pos_camera_space.w + coord_in_sphere * ubo.radius;
  vec3 frag_normal = coord_in_sphere;

  vec4 world_pos = ubo.view_inverse * vec4(frag_pos, 1.0);
  //world_pos /= world_pos.w;
  vec3 world_normal = mat3(ubo.view_inverse) * frag_normal;

  vec3 radiance = ubo.scene.ambient_light;
  radiance += get_point_light_radiance(world_pos.xyz, world_normal, false);
  radiance *= selected_color;

  out_color = vec4(radiance, 1.0);

  float depth =
      (pos_camera_space.z / pos_camera_space.w) + z_in_sphere * ubo.radius;

  gl_FragDepth = project_z(depth);
}
