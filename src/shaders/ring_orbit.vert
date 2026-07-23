#version 460

// InstanceData: una entrada por ventana en el carrusel Ring 3D
struct WindowInstance {
    vec2  position;   // Angulo en el anillo (x) + offset vertical (y)
    float zDepth;     // Profundidad calculada (0 = frente, positivo = atras)
    float scale;      // Escala relativa (ventana seleccionada = 1.0)
    float rotation;   // Rotacion en Y para mirar al centro
    float alpha;      // Opacidad [0,1]
    vec2  size;       // Tamano de la ventana en NDC
    uint  windowId;
};

layout(set = 0, binding = 0) buffer WindowData {
    WindowInstance instances[];
} windows;

layout(push_constant) uniform RingConstants {
    float totalAngle;   // Rotacion global del carrusel (animado)
    float radius;       // Radio del anillo (NDC)
    float tiltAngle;    // Inclinacion del anillo hacia el observador (radianes)
    float scaleFactor;  // Factor de escala perspectiva (1.0 = sin perspectiva)
    float spacing;      // Espaciado entre ventanas en el anillo
    uint  windowCount;
    float time;
} pc;

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec2 vUV;
layout(location = 1) out float vAlpha;
layout(location = 2) out float vDepth;

void main() {
    vUV = inUV;

    uint idx = uint(gl_InstanceIndex);
    if (idx >= pc.windowCount) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    WindowInstance inst = windows.instances[idx];

    // Angulo en el carrusel: angulo propio + rotacion global animada
    float angle = inst.position.x * pc.spacing + pc.totalAngle;

    // Posicion en el anillo 3D (antes del tilt)
    float x = sin(angle) * pc.radius;
    float z = cos(angle) * pc.radius - pc.radius; // negativo = mas cerca

    // Tilt del anillo: rotar el plano del carrusel hacia el observador
    float y = z * sin(pc.tiltAngle) + inst.position.y;
    z = z * cos(pc.tiltAngle);

    // Perspectiva simple: ventanas mas atras son mas pequenas
    float depth  = 1.0 + z * 0.5;
    float depthScale = pc.scaleFactor / max(depth, 0.1);
    float finalScale = inst.scale * depthScale;

    // La ventana rota en Y para siempre mirar al centro del anillo
    float lookAngle = -angle;
    float cosA = cos(lookAngle);
    float sinA = sin(lookAngle);

    // Transformar vertice (quad centrado)
    vec2 quadPos = (inUV * 2.0 - 1.0) * inst.size * finalScale;
    // Rotacion en Y (solo eje X se ve afectado en proyeccion 2D)
    vec2 rotated = vec2(quadPos.x * cosA, quadPos.y);

    // Posicion final en NDC
    float ndcX = x * pc.scaleFactor + rotated.x;
    float ndcY = y * pc.scaleFactor + rotated.y;

    gl_Position = vec4(ndcX, ndcY, z * 0.05 + 0.5, 1.0);

    // Atenuacion de opacidad por profundidad
    vAlpha = inst.alpha * clamp(1.0 - z * 0.08, 0.2, 1.0);
    vDepth = z;
}
