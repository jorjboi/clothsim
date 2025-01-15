#version 330

uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

uniform vec4 u_color;

uniform sampler2D u_texture_2;
uniform vec2 u_texture_2_size;

uniform float u_normal_scaling;
uniform float u_height_scaling;

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;
in vec2 v_uv;

out vec4 out_color;

float h(vec2 uv) {
  // return ((texture(u_texture_2, uv).xyz).x);
  return length(texture(u_texture_2, uv).xyz);
  //return 0.0;
}

void main() {
    // TBN columns
    vec3 tangent = normalize(v_tangent.xyz);
    vec3 normal = normalize(v_normal.xyz);
    vec3 bitangent = normalize(cross(normal, tangent));
    
    // Scaling factors
    float k_h = u_height_scaling; // Height
    float k_n = u_normal_scaling; // Normals
    
    // Caching value of h(v_uv)
    float h_uv = h(v_uv);

    // Computing dU and dV
    float displacementU = h(v_uv + vec2(1.0 / u_texture_2_size.x, 0.0));
    float displacementV = h(v_uv + vec2(0.0, 1.0 / u_texture_2_size.y));
    float dU = (displacementU - h_uv) * k_n * k_h;
    float dV = (displacementV - h_uv) * k_n * k_h;
    
    vec3 n_o = normalize(vec3(-dU, -dV, 1.0));
    vec3 n_d = n_o.x * tangent + n_o.y * bitangent + n_o.z * normal;
    
    vec4 bumped_normal = normalize(vec4(n_d, 0.0));
    
    vec3 kd = vec3(0.8, 0.8, 0.8);
    vec3 ks = vec3(0.5, 0.5, 0.5);
    vec3 ka = vec3(0.1, 0.1, 0.1);
    float p = 100.0;

    vec3 wi = u_light_pos - v_position.xyz;
    float distToLightSqr = dot(wi, wi);
    wi = normalize(wi);
    vec3 wo = normalize(u_cam_pos - v_position.xyz);
    vec3 n = bumped_normal.xyz;

    vec3 Ia = vec3(1, 1, 1);
    vec3 intensity = u_light_intensity / distToLightSqr;

    // Ambient component
    vec3 ambientComponent = ka * Ia;

    // Diffuse component
    vec3 color = u_color.xyz;
    vec3 diffuseComponent = kd * intensity * max(dot(wi, n), 0.0) * color;

    // Specular component
    vec3 h = normalize(wi + wo);
    vec3 specularComponent = ks * intensity * pow(clamp(dot(n, h), 0.0, 1.0), p);

    // Phong shading is the summation of ambient, diffuse, and specular shading components.
    out_color = vec4(ambientComponent + diffuseComponent + specularComponent, 1.0);
    
    /*
    out_color = (vec4(1, 1, 1, 0) + bumped_normal) / 2;
    out_color.a = 1;
    */
}

