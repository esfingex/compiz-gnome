#version 460

layout(set = 0, binding = 0) buffer SpringGrid {
    vec4 positions[];   // x,y = posicion actual, z,w = velocidad
} grid;

layout(push_constant) uniform WobblyRenderConstants {
    uint  gridWidth;
    uint  gridHeight;
    float scaleX;           // 2.0 / screenWidth
    float scaleY;           // 2.0 / screenHeight
    float textureWidth;     // Ancho de la ventana en pixels
    float textureHeight;    // Alto de la ventana en pixels
    float originX;          // X esquina superior izquierda en pixels
    float originY;          // Y esquina superior izquierda en pixels
} pc;

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vNormal;

// Polinomios de Bernstein para Bezier cubica
float B0(float t) { float m = 1.0 - t; return m * m * m; }
float B1(float t) { float m = 1.0 - t; return 3.0 * t * m * m; }
float B2(float t) { float m = 1.0 - t; return 3.0 * t * t * m; }
float B3(float t) { return t * t * t; }

// Derivadas de los polinomios de Bernstein
float dB0(float t) { float m = 1.0 - t; return -3.0 * m * m; }
float dB1(float t) { float m = 1.0 - t; return 3.0 * m * m - 6.0 * t * m; }
float dB2(float t) { float m = 1.0 - t; return 6.0 * t * m - 3.0 * t * t; }
float dB3(float t) { return 3.0 * t * t; }

vec2 evalBernstein(float t) { return vec2(B0(t), dB0(t)); }

void main() {
    vUV = inUV;

    // Mapear UV a coordenada continua en la grilla
    float cellU = inUV.x * float(pc.gridWidth  - 1);
    float cellV = inUV.y * float(pc.gridHeight - 1);
    int   ix    = clamp(int(floor(cellU)), 0, int(pc.gridWidth)  - 2);
    int   iy    = clamp(int(floor(cellV)), 0, int(pc.gridHeight) - 2);
    float fx    = fract(cellU);
    float fy    = fract(cellV);

    // Evaluar Bezier bicubica usando 4x4 puntos de control
    float Bu[4]  = float[4](B0(fx), B1(fx), B2(fx), B3(fx));
    float Bv[4]  = float[4](B0(fy), B1(fy), B2(fy), B3(fy));
    float dBu[4] = float[4](dB0(fx), dB1(fx), dB2(fx), dB3(fx));
    float dBv[4] = float[4](dB0(fy), dB1(fy), dB2(fy), dB3(fy));

    vec2 pos = vec2(0.0);
    vec2 du  = vec2(0.0); // Derivada parcial en U (tangente horizontal)
    vec2 dv  = vec2(0.0); // Derivada parcial en V (tangente vertical)

    for (int j = 0; j < 4; j++) {
        int jj = clamp(iy + j - 1, 0, int(pc.gridHeight) - 1);
        for (int i = 0; i < 4; i++) {
            int ii  = clamp(ix + i - 1, 0, int(pc.gridWidth) - 1);
            uint gi = uint(jj) * pc.gridWidth + uint(ii);
            vec2 P  = grid.positions[gi].xy;

            float w = Bu[i] * Bv[j];
            pos += P * w;
            du  += P * (dBu[i] * Bv[j]);
            dv  += P * (Bu[i]  * dBv[j]);
        }
    }

    // Normal de la superficie (cross product de las tangentes + eje Z fijo)
    vec3 T_u = vec3(du, 0.0);
    vec3 T_v = vec3(dv, 0.0);
    // Como la malla es 2D, el normal siempre apunta hacia +Z pero lo perturbamos
    // con la curvatura de la deformacion para shading correcto
    vNormal = normalize(cross(T_u, vec3(0.0, 0.0, 1.0)) + cross(vec3(0.0, 0.0, 1.0), T_v) + vec3(0.0, 0.0, 1.0));

    // Transformar de espacio de ventana (pixels) a NDC [-1, 1]
    float ndcX =  (pos.x + pc.originX) * pc.scaleX - 1.0;
    float ndcY = -(pos.y + pc.originY) * pc.scaleY + 1.0; // Y invertida

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
}
