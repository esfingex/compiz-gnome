#version 460 core

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 vUV;

layout(set = 0, binding = 0) uniform sampler2D u_normalMap;     // Normal XY + Height(A)
layout(set = 0, binding = 1) uniform sampler2D u_windowTexture; // Textura de la ventana (DMA-BUF)

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float time;
    float refractionStrength;
} pc;

void main() {
    vec4 nmap = texture(u_normalMap, vUV);
    vec3 normal = normalize(nmap.xyz);
    float height = nmap.a;

    // 1. Refracción de Snell en espacio de pantalla
    vec2 offset = normal.xy * height * pc.refractionStrength;

    // 2. Aberración Cromática (Muestra de canales R, G, B con desfase)
    float r = texture(u_windowTexture, vUV + offset * 1.05).r;
    float g = texture(u_windowTexture, vUV + offset).g;
    float b = texture(u_windowTexture, vUV + offset * 0.95).b;
    float a = texture(u_windowTexture, vUV + offset).a;

    // 3. Resplandor Especular (Specular Highlight)
    vec3 lightDir = normalize(vec3(0.5, 0.5, 1.0));
    float spec = pow(max(0.0, dot(normal, lightDir)), 32.0) * 0.5;

    // 4. Término Fresnel-Schlick
    float fresnel = pow(1.0 - max(0.0, normal.z), 3.0) * 0.3;

    vec3 finalColor = mix(vec3(r, g, b), vec3(1.0), spec) + vec3(fresnel);
    outColor = vec4(finalColor, a);
}
