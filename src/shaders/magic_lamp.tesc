#version 460

// Tessellation Control Shader — Magic Lamp / Curl / Explode (Animation Suite)
// Subdivision adaptativa: mas teselacion en zonas de alta curvatura.
layout(vertices = 4) out;

layout(push_constant) uniform AnimationConstants {
    uint  instanceIndex;
    float progress;       // [0,1] progreso de la animacion
    float tessFactor;     // Factor base de subdivision (4-16)
    float time;
    uint  effectType;     // 0=Fade 1=MagicLamp 2=Curl 3=Explode 4=Slide
    vec4  extraParams;    // Parametros especificos del efecto
} pc;

layout(location = 0) in  vec2 vUV_in[];
layout(location = 0) out vec2 vUV_out[];

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    vUV_out[gl_InvocationID] = vUV_in[gl_InvocationID];

    if (gl_InvocationID == 0) {
        // Teselacion base escalada por progreso (mas deformacion = mas tris)
        float tess = pc.tessFactor * (1.0 + pc.progress * 2.5);

        // Magic Lamp: mas subdivision en la zona del icono de destino
        // extraParams.x = distancia relativa al punto de succion
        if (pc.effectType == 1u) {
            float suctionBoost = 1.0 + (1.0 - pc.extraParams.x) * 8.0;
            tess *= suctionBoost;
        }

        // Curl: mas subdivision en el frente del enrollamiento
        // extraParams.x = posicion del frente de curl [0,1]
        if (pc.effectType == 2u) {
            float foldDist = abs(vUV_in[0].y - pc.extraParams.x);
            tess *= 1.0 + max(0.0, 1.0 - foldDist * 4.0) * 6.0;
        }

        tess = clamp(tess, 2.0, 64.0);

        gl_TessLevelOuter[0] = tess;
        gl_TessLevelOuter[1] = tess;
        gl_TessLevelOuter[2] = tess;
        gl_TessLevelOuter[3] = tess;
        gl_TessLevelInner[0] = tess;
        gl_TessLevelInner[1] = tess;
    }
}
