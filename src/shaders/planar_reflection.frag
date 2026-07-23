#version 460

layout(set = 0, binding = 0) uniform sampler2D u_windowTexture;
layout(set = 0, binding = 1) uniform sampler2D u_reflectionTexture;
layout(set = 0, binding = 2) uniform sampler2D u_glossyBlurTexture;

layout(push_constant) uniform ShiftConstants {
    float reflectionIntensity; // Intensidad del reflejo en el suelo [0,1]
    float floorY;              // Y del plano de suelo (NDC)
    float fresnelPower;        // Exponente Fresnel (tipico: 3.0-5.0)
    float blurAmount;          // Cantidad de blur en el reflejo [0,1]
    float alpha;               // Opacidad global de la ventana
    float scale;               // Escala de la ventana (Cover Flow seleccionada = 1.0)
    float rotation;            // Rotacion en Y (radianes)
    float offsetX;             // Desplazamiento horizontal en NDC
    float offsetY;             // Desplazamiento vertical en NDC
} pc;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vPosition;
layout(location = 0) out vec4 outColor;

// Fresnel de Schlick: cuanta reflexion vemos segun el angulo de incidencia
float fresnelSchlick(vec3 normal, vec3 viewDir, float F0) {
    float cosTheta = max(dot(normalize(normal), normalize(viewDir)), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, pc.fresnelPower);
}

void main() {
    vec4 winColor = texture(u_windowTexture, vUV);

    // UV de reflexion: invertir Y para simular suelo espejado
    vec2 reflectUV = vec2(vUV.x, 1.0 - vUV.y);

    // Distancia al plano de suelo: mas lejos = mas blur
    float distToFloor = abs(vPosition.y - pc.floorY);
    float blurFactor  = smoothstep(0.0, 0.8, distToFloor * 2.0) * pc.blurAmount;

    // Elegir entre textura sharpe o glossy blur segun distancia
    vec4 reflectColor = mix(
        texture(u_reflectionTexture,  reflectUV),
        texture(u_glossyBlurTexture,  reflectUV),
        blurFactor
    );

    // Fresnel: angulo de vision desde camara fija frontal
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0) - vPosition);
    float F = fresnelSchlick(vNormal, viewDir, 0.04);
    float intensity = pc.reflectionIntensity * (0.5 + 0.5 * F);

    // Gradiente lateral Cover Flow (ventanas laterales mas oscuras)
    float lateral = 1.0 - abs(vUV.x - 0.5) * 0.6;

    vec3 finalColor = mix(winColor.rgb, reflectColor.rgb, intensity) * lateral;

    outColor = vec4(finalColor, winColor.a * pc.alpha);
}
