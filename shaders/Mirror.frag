#version 330


uniform vec3 u_cam_pos;

uniform samplerCube u_texture_cubemap;

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;

out vec4 out_color;

void main() {
  vec3 norm = normalize(v_normal.xyz);
  vec3 w_out = normalize(v_position.xyz - u_cam_pos);
  vec3 w_in = reflect(w_out, norm);

  // sample envrionment map using reflection directioin
  vec4 env_color = texture(u_texture_cubemap, w_in);
  out_color = vec4(env_color.xyz, 1.0);
}
