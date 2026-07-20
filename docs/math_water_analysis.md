# Análisis Matemático y Modernización: Plugin Water (Superficie de Agua & Ripple)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/water/src/water.cpp`
- `compiz-0.9.14.2/plugins/water/src/shaders.h`
- `compiz-0.9.14.2/plugins/water/src/water.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `water` simula la propagación de ondas de agua en la superficie del escritorio usando la **Ecuación de Onda 2D Discretizada** por el Método de Diferencias Finitas (FDM), ejecutada mediante shaders de fragmentos con tres buffers intermitentes (Ping-Pong FBOs).

### A. Ecuación Diferencial Continua de Onda 2D
La física subyacente es la ecuación de onda en un medio bidimensional continuo:

$$\frac{\partial^2 u}{\partial t^2} = c^2 \left( \frac{\partial^2 u}{\partial x^2} + \frac{\partial^2 u}{\partial y^2} \right) - \gamma \frac{\partial u}{\partial t}$$

Donde:
- $u(x,y,t)$ es la altura de la superficie en la posición $(x,y)$ y tiempo $t$.
- $c$ es la velocidad de propagación de la onda.
- $\gamma$ es el coeficiente de amortiguamiento (viscosidad del fluido).

### B. Discretización Finitad en Malla 2D (Shader `update_water_vertices_fragment_shader`)
En el fragment shader de Compiz, la aceleración Laplacian $\nabla^2 u$ se calcula usando un stencil de 5 puntos (vecinos ortogonales):

$$\text{accel} = (u_{curr}(x, y-1) + u_{curr}(x, y+1) + u_{curr}(x-1, y) + u_{curr}(x+1, y)) - 4 \cdot u_{curr}(x, y)$$

La integración temporal de Verlet/Euler calcula la nueva altura $u_{next}(x,y)$ en el componente `alpha` (`v.w`):

$$u_{next}(x, y) = \text{accel} \cdot (dt \cdot K) + 2 \cdot u_{curr}(x, y) - u_{prev}(x, y)$$

$$u_{next}(x, y) \leftarrow u_{next}(x, y) \cdot \text{fade}$$

Donde en el código fuente:
- $K = 0.1964$ (constante de escala temporal de la onda).
- $\text{fade} \in [0.90, 0.999]$ (factor de atenuación exponencial por frame para disipar la energía).

### C. Generación Dinámica del Mapa de Normales (Normal Map Generator)
Para calcular cómo la deformación del agua desvía la luz, los gradientes espaciales de altura se convierten en un vector normal 3D $\vec{N}$:

$$N_x = u(x-1, y) - u(x+1, y)$$
$$N_y = u(x, y+1) - u(x, y-1)$$
$$N_z = 0.75$$

El vector se normaliza y se empaqueta en el rango $[0, 1]$ para el formato de textura RGBA:

$$\vec{N}_{packed} = 0.5 \cdot \text{normalize}(\vec{N}) + 0.5$$

### D. Renderizado de Refracción y Mapeo Diffuso (`paint_water_vertices_fragment_shader`)
En la etapa final de pintado sobre la pantalla compositada:

1. **Offset de Refracción**:
$$\Delta u = N_x \cdot u(x,y) \cdot \frac{\text{offsetScale}}{W}$$
$$\Delta v = N_y \cdot u(x,y) \cdot \frac{\text{offsetScale}}{H}$$

2. **Muestreo de la Textura Base con Desplazamiento**:
$$C_{base} = \text{texture}(baseTex, \vec{uv} + \vec{offset})$$

3. **Iluminación Especular / Difusa**:
$$\text{diffFact} = \vec{N} \cdot \vec{L}$$
$$C_{final} = C_{base} + \text{diffFact}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En Compiz 0.9, el algoritmo requería **3 Framebuffers OpenGL (Ping-Pong FBOs)** y 3 pases de fragment shader completos, lo que generaba un cuello de botella en el rasterizador de la GPU.

### A. Sustitución del Rasterizador por Vulkan Compute Shaders (`VkComputePipeline`)
En lugar de rasterizar cuadriláteros en un FBO, la simulación física completa se traslada a un **Compute Shader de Vulkan**:

```
[ Input Mouse / Rain Events ]
              │
              ▼
[ Vulkan SSBO / Storage Image ]
              │
              ▼
┌────────────────────────────────────────────────────────┐
│  Vulkan Compute Shader (workgroup 16x16)               │
│  - Carga stencil en Workgroup Shared Memory            │
│  - Resuelve Ecuación de Onda 2D + Normal Map           │
└────────────────────────────────────────────────────────┘
              │
              ▼
[ Storage Image Result ] ──► [ Integrated into Main Composite Pipeline via DMA-BUF ]
```

### B. Optimización con Memoria Compartida (`workgroupMemory` / Local Data Store)
En OpenGL clásico, cada píxel hacía 6 lecturas de textura separadas (`prevTex`, `currTex` en 4 direcciones). En Vulkan Compute Shader:
- Se lee un bloque de $18 \times 18$ alturas a `shared float tile[18][18]`.
- Se sincroniza con `barrier()`.
- Todas las celdas internas de $16 \times 16$ calculan el laplaciano desde `shared memory` a velocidad de registro de GPU (0 accesos a VRAM adicional).

### C. Mejoras Matemáticas y Ópticas Avanzadas

1. **Refracción Física de Ley de Snell ($n_1 \sin \theta_1 = n_2 \sin \theta_2$)**:
   Sustituir el desplazamiento lineal ad-hoc por el vector de refracción exacto:
   $$\vec{R} = \frac{\eta_1}{\eta_2} \vec{I} + \left( \frac{\eta_1}{\eta_2} \cos\theta_1 - \sqrt{1 - \left(\frac{\eta_1}{\eta_2}\right)^2 (1 - \cos^2\theta_1)} \right) \vec{N}$$
   Donde $\frac{\eta_1}{\eta_2} \approx 0.75$ (aire a agua).

2. **Aberración Cromática (Dispersion Factor)**:
   Muestrear los canales R, G y B con índices de refracción ligeramente distintos ($\eta_{red} = 0.750$, $\eta_{green} = 0.753$, $\eta_{blue} = 0.757$):
   $$C_{red} = \text{texture}(baseTex, \vec{uv} + \vec{offset} \cdot 0.98).r$$
   $$C_{green} = \text{texture}(baseTex, \vec{uv} + \vec{offset} \cdot 1.00).g$$
   $$C_{blue} = \text{texture}(baseTex, \vec{uv} + \vec{offset} \cdot 1.02).b$$
   Esto genera un destello de prisma muy realista en los bordes de la onda.

3. **Ecuación de Reflexión de Fresnel (Fresnel Term Approximation)**:
   $$F(\theta) = F_0 + (1 - F_0)(1 - \cos\theta)^5 \quad \text{con } F_0 = 0.02$$
   Mezcla la textura invertida del fondo con la refracción en ángulos rasantes.

---

## 3. Integración en la Arquitectura C++20 / Extension GJS

- **C++20 Engine**: Implementa `WaterComputePass` con `vkCmdDispatch(cmd, width/16, height/16, 1)`.
- **GJS Extension**: Permite activar el efecto con gestos multi-touch o al mover ventanas rápidamente por el escritorio.
- **Exportación**: Expone el mapa de normal/refracción o la imagen resultante como `VkImage` a través de `dma-buf` (`VK_KHR_external_memory_fd`).
