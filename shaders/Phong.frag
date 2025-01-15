#version 330

uniform vec4 u_color;
uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

in vec4 v_position;
in vec4 v_normal;
in vec2 v_uv;

out vec4 out_color;

void main() {

  vec3 kd = u_color.xyz;
  vec3 ks = vec3(0.5, 0.5, 0.5);
  vec3 ka = vec3(0.1, 0.1, 0.1);
  float p = 100.0;

  vec3 wi = u_light_pos - v_position.xyz;
  float distToLightSqr = dot(wi, wi);
  wi = normalize(wi);
  vec3 wo = normalize(u_cam_pos - v_position.xyz);
  vec3 n = normalize(v_normal.xyz);

  vec3 Ia = vec3(1, 1, 1);
  vec3 intensity = u_light_intensity / distToLightSqr;

  // Ambient component
  vec3 ambientComponent = ka * Ia;

  // Diffuse component (see Diffuse.frag)
  vec3 diffuseComponent = kd * intensity * max(dot(wi, n), 0.0);

  // Specular component
  vec3 h = normalize(wi + wo);
  vec3 specularComponent = ks * intensity * pow(clamp(dot(n, h), 0.0, 1.0), p);

  // Phong shading is the summation of ambient, diffuse, and specular shading components.
  out_color = vec4(ambientComponent + diffuseComponent + specularComponent, 1.0);
}

