#version 460

// Dual Kawase Blur — Upsample Pass (Fragment Shader)
// Doblamos la imagen a la mitad baja en resolucion, aplicando un kernel de
// 8 taps en forma de diamante para reconstruccion con anti-aliasing suave.

layout(set = 0, binding = 0) uniform sampler2D u_srcLow;

layout(push_constant) uniform KawaseUpConstants {
    vec2  texel_size;  // 1.0 / textureSize(u_srcLow, 0)
    float offset;      // Desplazamiento del kernel (igual que en el downsample)
    float opacity;     // Opacidad del blur (para mezcla con la imagen original)
} pc;

layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = pc.texel_size * pc.offset;

    // Kernel de 8 taps en diamante (pesos de Kawase upsample)
    vec4 sum = vec4(0.0);
    sum += texture(u_srcLow, vUV + vec2(-2.0 * d.x,         0.0)) * 1.0;
    sum += texture(u_srcLow, vUV + vec2(-d.x,          d.y)) * 2.0;
    sum += texture(u_srcLow, vUV + vec2(0.0,      2.0 * d.y)) * 1.0;
    sum += texture(u_srcLow, vUV + vec2( d.x,          d.y)) * 2.0;
    sum += texture(u_srcLow, vUV + vec2( 2.0 * d.x,         0.0)) * 1.0;
    sum += texture(u_srcLow, vUV + vec2( d.x,         -d.y)) * 2.0;
    sum += texture(u_srcLow, vUV + vec2(0.0,     -2.0 * d.y)) * 1.0;
    sum += texture(u_srcLow, vUV + vec2(-d.x,         -d.y)) * 2.0;

    outColor = (sum / 12.0) * pc.opacity;
}
