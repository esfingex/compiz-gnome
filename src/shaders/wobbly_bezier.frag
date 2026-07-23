#version 460

// Fragment shader para la malla deformada de Wobbly Windows.
// Simplemente samplea la textura de la ventana en las UV interpoladas
// por el vertex shader, que ya vienen deformadas por la grilla de resortes.

layout(set = 0, binding = 1) uniform sampler2D u_windowTexture;

layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Sample simple — la deformacion ya la hizo el vertex shader
    outColor = texture(u_windowTexture, vUV);
}
