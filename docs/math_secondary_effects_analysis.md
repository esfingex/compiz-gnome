# Análisis Matemático y Modernización: Los 7 Efectos Visuales Secundarios de Compiz

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/firepaint/src/firepaint.cpp`
- `compiz-0.9.14.2/plugins/cubeaddon/src/cubeaddon.cpp`
- `compiz-0.9.14.2/plugins/mblur/src/mblur.cpp`
- `compiz-0.9.14.2/plugins/reflex/src/reflex.cpp`
- `compiz-0.9.14.2/plugins/colorfilter/src/colorfilter.cpp`

---

## 1. Firepaint (Pintado Interactivo de Fuego)

### A. Física de Partículas Emitidas por Cursor
Cuando el usuario arrastra el cursor $(x, y)$, emite partículas en una cola de emisión. Para cada partícula $p$:

$$x(t+\Delta t) = x(t) + \frac{v_x}{slowdown}, \quad y(t+\Delta t) = y(t) + \frac{v_y}{slowdown}$$
$$v_x(t+\Delta t) = v_x(t) + a_x \cdot dt, \quad v_y(t+\Delta t) = v_y(t) + a_y \cdot dt$$

Donde la aceleración ascendente $a_y = -3.0$ (convección térmica de llama) y la vida $life(t) \leftarrow life(t) - fade \cdot dt$.

### B. Modernización Vulkan: Particle Compute Shader
- En Vulkan C++20, en vez de almacenar arreglos en CPU y mapear `GLVertexBuffer`, se usa un **Vulkan Storage Buffer (SSBO)** de $N=100,000$ partículas actualizadas por un `VkComputePipeline` de 1 pase y renderizadas con `vkCmdDrawIndirect`.

---

## 2. Motion Blur (`mblur` - Desenfoque de Movimiento Acumulativo)

### A. Integración Temporal Exponencial (Accumulation Buffer)
`mblur` genera un rastro de movimiento reteniendo los frames anteriores en una textura de acumulación:

$$F_{accum}(t) = \alpha \cdot F_{curr}(t) + (1 - \alpha) \cdot F_{accum}(t - \Delta t)$$

Donde $\alpha \in (0, 1]$ es el factor de persistencia.

### B. Modernización Vulkan
- En Vulkan C++20, se utiliza un `VkImage` intermedio de alta precisión (`VK_FORMAT_R16G16B16A16_SFLOAT`) mezclado dinámicamente durante el pase de composición.

---

## 3. CubeAddon (Cubo Esférico, Reflejo en Suelo y Cap Images)

### A. Morphing Geométrico Cubo $\to$ Esfera
El plugin intercala linealmente entre la superficie del cubo 3D y la esfera parametrizada:

$$\mathbf{P}_{sphere} = R \cdot \text{normalize}(\mathbf{P}_{cube})$$
$$\mathbf{P}_{final}(t) = (1 - \lambda) \cdot \mathbf{P}_{cube} + \lambda \cdot \mathbf{P}_{sphere}, \quad \lambda \in [0, 1]$$

### B. Reflexiones de Cielo/Suelo (Skybox & Ground Reflection)
Renderiza el reflejo sobre el plano $Y=0$ con atenuación de Fresnel $F(\theta)$ y textura de cielo desvanecida en el horizonte.

---

## 4. 3D Windows (`td` - Ventanas con Inclinación Depth-of-Field)

Aplica una transformación de rotación 3D inclinada $\mathbf{R}_x(\theta)$ a las ventanas no enfocadas y un desenfoque Gaussiano proporcional a la distancia $Z$ a la cámara:

$$\text{blur\_radius}(z) = k \cdot |z - z_{focus}|$$

---

## 5. Filtros de Color (`obs`, `colorfilter`, `neg`)

Transformaciones matriciales $4 \times 4$ en espacio de color aplicadas en el Fragment Shader:

$$\begin{pmatrix} R' \\ G' \\ B' \\ A' \end{pmatrix} = \mathbf{M}_{color} \cdot \begin{pmatrix} R \\ G \\ B \\ A \end{pmatrix}$$

- **Inversión (Negativo)**: $\mathbf{M}_{neg} = \begin{pmatrix} -1 & 0 & 0 & 1 \\ 0 & -1 & 0 & 1 \\ 0 & 0 & -1 & 1 \\ 0 & 0 & 0 & 1 \end{pmatrix}$
- **Daltonismo (Deuteranopia/Protanopia)**: Multiplicación por la matriz de corrección LMS de LMS-to-RGB.

---

## 6. Reflexión Especular (`reflex`)

Aplica una textura de mapa de reflejo espejado $\mathbf{T}_{reflex}(u, v)$ con degradado alpha en el borde superior o inferior de las ventanas:

$$C_{out} = C_{win} + \alpha_{reflex}(v) \cdot C_{gloss}$$
