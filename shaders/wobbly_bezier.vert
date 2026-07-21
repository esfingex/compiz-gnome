#version 460

// Entrada del buffer de posiciones de la grilla de resortes (compute-generated)
layout(set = 0, binding = 0) buffer SpringGrid {
    vec4 positions[];   // x,y = posicion actual del vertice (NDC), z,w = velocidad
} grid;

// Push constants compartidos con el compute pass
layout(push_constant) uniform WobblyVertexConstants {
    uint  gridWidth;
    uint  gridHeight;
    float scaleX;       // 2.0 / screenWidth
    float scaleY;       // 2.0 / screenHeight
    float originX;      // X esquina superior izq de la ventana (pixels)
    float originY;      // Y esquina superior izq de la ventana (pixels)
} pc;

layout(location = 0) in vec2 inUV;   // Coordenadas UV normalizadas [0,1]
layout(location = 0) out vec2 vUV;

// Evaluacion de polinomio de Bernstein para Bezier cubica
// t in [0,1], i in {0,1,2,3}
float bernstein(int i, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    float mt = 1.0 - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    if (i == 0) return mt3;
    if (i == 1) return 3.0 * mt2 * t;
    if (i == 2) return 3.0 * mt  * t2;
    return t3;
}

void main() {
    vUV = inUV;

    // Mapear UV a coordenada continua en la grilla
    float cellX = inUV.x * float(pc.gridWidth  - 1);
    float cellY = inUV.y * float(pc.gridHeight - 1);
    int   ix    = clamp(int(floor(cellX)), 0, int(pc.gridWidth)  - 2);
    int   iy    = clamp(int(floor(cellY)), 0, int(pc.gridHeight) - 2);
    float fx    = fract(cellX);
    float fy    = fract(cellY);

    // Interpolacion bilineal entre los 4 vertices de la celda (GL_NICEST alternativa
    // a la Bezier bicubica completa que requiere 4x4=16 samples y es mas cara)
    uint i00 = uint(iy)     * pc.gridWidth + uint(ix);
    uint i10 = uint(iy)     * pc.gridWidth + uint(ix + 1);
    uint i01 = uint(iy + 1) * pc.gridWidth + uint(ix);
    uint i11 = uint(iy + 1) * pc.gridWidth + uint(ix + 1);

    vec2 p00 = grid.positions[i00].xy;
    vec2 p10 = grid.positions[i10].xy;
    vec2 p01 = grid.positions[i01].xy;
    vec2 p11 = grid.positions[i11].xy;

    // Interpolacion bilineal suave (equivalente a Bezier grado 1, suficiente para 32x32)
    vec2 pos = mix(mix(p00, p10, fx), mix(p01, p11, fx), fy);

    // Transformar de espacio de ventana (pixels) a NDC [-1,1]
    // pos ya contiene la posicion en pixels desde el origen de la ventana
    float ndcX = (pos.x + pc.originX) * pc.scaleX - 1.0;
    float ndcY = 1.0 - (pos.y + pc.originY) * pc.scaleY; // Y invertida en NDC

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
}
