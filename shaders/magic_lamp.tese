#version 460

// Tessellation Evaluation Shader — Magic Lamp / Curl / Explode
layout(quads, equal_spacing, ccw) in;

layout(push_constant) uniform AnimationConstants {
    uint  instanceIndex;
    float progress;     // [0,1]
    float tessFactor;
    float time;
    uint  effectType;   // 0=Fade 1=MagicLamp 2=Curl 3=Explode 4=Slide
    vec4  extraParams;  // [0]=iconX/curlFront/seed, [1]=iconY, [2]=width, [3]=height
} pc;

layout(location = 0) in  vec2 vUV_in[];
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vNormal;

// ─── Magic Lamp ──────────────────────────────────────────────────────────────
// Deforma la ventana hacia un icono (taskbar) con curva sigmoide.
// extraParams: x=iconX(NDC), y=iconY(NDC), z=windowW(NDC), w=windowH(NDC)
vec4 magicLampDeform(vec2 uv, float prog) {
    vec2 iconPos = pc.extraParams.xy;
    vec2 winSize = pc.extraParams.zw;

    // Posicion original del vertice en NDC
    vec2 origPos = vec2(uv.x * winSize.x - winSize.x * 0.5,
                        uv.y * winSize.y - winSize.y * 0.5);

    // Funcion sigmoide para suavizar la succion (k controla la rigidez)
    float k  = 6.0;
    float s  = 1.0 / (1.0 + exp(-k * (prog - 0.5)));

    // Interpolar entre posicion original y posicion del icono
    vec2 deformed = mix(origPos, iconPos, s * prog);

    // Estrechamiento: la ventana se encoge en X a medida que sube hacia el icono
    float squeeze = 1.0 - prog * uv.y * 0.8;
    deformed.x *= squeeze;

    return vec4(deformed, 0.0, 1.0);
}

// ─── Curl ────────────────────────────────────────────────────────────────────
// Enrolla la ventana desde un borde (como una pagina de papel).
// extraParams: x=curlFront(0=top, 1=bottom), y=curlRadius
vec4 curlDeform(vec2 uv, float prog) {
    float front  = pc.extraParams.x; // Posicion del frente de enrollamiento
    float radius = max(pc.extraParams.y, 0.01);

    float y = uv.y;
    float dist = y - front;

    vec2 pos;
    if (dist < 0.0) {
        // Zona ya enrollada: circulo
        float angle = dist / radius;
        pos = vec2(uv.x * 2.0 - 1.0, front + radius * sin(angle));
    } else {
        // Zona plana
        pos = vec2(uv.x * 2.0 - 1.0, y);
    }

    return vec4(pos, 0.0, 1.0);
}

// ─── Explode ─────────────────────────────────────────────────────────────────
// Fragmenta la ventana en trozos que explotan hacia afuera.
vec4 explodeDeform(vec2 uv, float prog) {
    // Centro del fragmento (celda de la cuadricula)
    vec2 cell   = floor(uv * 8.0) / 8.0 + 0.5 / 8.0;
    vec2 center = cell * 2.0 - 1.0;

    // Direccion de explosion desde el centro de la ventana
    vec2 dir    = normalize(center + vec2(0.0001));
    float speed = 0.5 + fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5) * 1.0;

    vec2 pos    = uv * 2.0 - 1.0;
    pos += dir * prog * speed * 1.5;

    // Rotacion del fragmento
    float rot   = prog * speed * 3.14159;
    float c     = cos(rot), s = sin(rot);
    vec2 fragPos = uv * 2.0 - 1.0 - center;
    fragPos = vec2(fragPos.x * c - fragPos.y * s,
                   fragPos.x * s + fragPos.y * c);
    pos = center + fragPos;

    return vec4(pos, 0.0, 1.0);
}

void main() {
    // Interpolacion de UV en el quad teselado
    vec2 uv = mix(
        mix(vUV_in[0], vUV_in[1], gl_TessCoord.x),
        mix(vUV_in[3], vUV_in[2], gl_TessCoord.x),
        gl_TessCoord.y
    );
    vUV = uv;

    vec4 pos;
    switch (pc.effectType) {
        case 1u: pos = magicLampDeform(uv, pc.progress); break;
        case 2u: pos = curlDeform(uv, pc.progress);      break;
        case 3u: pos = explodeDeform(uv, pc.progress);   break;
        default: pos = vec4(uv * 2.0 - 1.0, 0.0, 1.0);  break;
    }

    // Fade out: escalar hacia cero cuando progress → 1 para efectos de salida
    if (pc.effectType == 0u) {
        pos.xy *= (1.0 - pc.progress);
    }

    gl_Position = pos;

    // Normal (siempre Z para la malla plana deformada 2D)
    vNormal = vec3(0.0, 0.0, 1.0);
}
