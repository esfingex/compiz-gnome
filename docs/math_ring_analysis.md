# Análisis Matemático y Modernización: Plugin Ring (Carrusel Circular / Elíptico 3D)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/ring/src/ring.cpp`
- `compiz-0.9.14.2/plugins/ring/src/ring.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `ring` distribuye las miniaturas de las ventanas abiertas en una trayectoria elíptica o circular 3D alrededor del centro de la pantalla.

### A. Ecuaciones Paramétricas de la Elipse de Navegación
Dada una lista de $N$ ventanas, el índice de ventana $i \in [0, N-1]$ se mapea a un ángulo $\theta_i$:

$$\theta_i = \theta_{base} - i \cdot \left( \frac{2\pi}{N} \right)$$

Donde $\theta_{base} = \frac{2\pi \cdot \text{mRotTarget}}{3600}$ representa el ángulo de rotación global del carrusel.

Las coordenadas de la pantalla $(x_i, y_i)$ de la ventana $i$ se obtienen mediante la parametrización de la elipse:

$$x_i = C_x \pm A \cdot \sin(\theta_i)$$
$$y_i = C_y + B \cdot \cos(\theta_i)$$

Donde:
- $(C_x, C_y) = \left( \frac{W_{screen}}{2}, \frac{H_{screen}}{2} \right)$ es el centro de la pantalla.
- $A = \frac{W_{screen} \cdot \text{RingWidth}}{200}$ es el semieje mayor horizontal.
- $B = \frac{H_{screen} \cdot \text{RingHeight}}{200}$ es el semieje menor vertical.

### B. Interpolación Lineal de Profundidad ($s_{depth}$ y $b_{depth}$)
En Compiz 0.9 (2D compositing), la ilusión de profundidad 3D se lograba interpolando linealmente la escala y el brillo en función de la coordenada $y_i$ (las ventanas con mayor $y$ están más cerca de la pantalla):

$$s_{depth}(y_i) = \text{minScale} + \left( \frac{1.0 - \text{minScale}}{(C_y + B) - (C_y - B)} \right) \cdot (y_i - (C_y - B))$$

$$b_{depth}(y_i) = \text{minBrightness} + \left( \frac{1.0 - \text{minBrightness}}{(C_y + B) - (C_y - B)} \right) \cdot (y_i - (C_y - B))$$

El tamaño final de la miniatura $s_{total}$ combina el escalado para ajustar al bounding box con la profundidad:

$$s_{fit} = \min\left( \frac{\text{thumbWidth}}{W_{win}}, \frac{\text{thumbHeight}}{H_{win}}, 1.0 \right)$$
$$s_{total} = s_{fit} \cdot s_{depth}(y_i)$$

### C. Ordenamiento de Dibujo (Painter's Algorithm)
Para evitar fallos de superposición sin Z-buffer de profundidad, las ventanas se ordenan por su posición $y$:

$$\text{compareRingWindowDepth}(a, b): a.y < b.y \implies a \text{ se dibuja primero (atrás)}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En el motor `compiz-gnome` C++20/Vulkan:

### A. Espacio 3D Real con Cámara de Perspectiva (True 3D Orbit)
Sustituimos el escalado 2D simulado por un verdadero **anillo 3D en el plano XZ**:

$$\mathbf{P}_i = \begin{pmatrix} A \cdot \sin(\theta_i) \\ 0 \\ B \cdot \cos(\theta_i) \end{pmatrix}$$

Matriz de Transformación por Ventana:
$$\mathbf{M}_i = \mathbf{T}(\mathbf{P}_i) \cdot \mathbf{R}_y(-\theta_i) \cdot \mathbf{S}(s_{fit})$$

La matriz de proyección en perspectiva $\mathbf{P}_{cam} \cdot \mathbf{V}_{cam}$ aplica automáticamente el acortamiento de perspectiva ($1/z$), el ordenamiento de profundidad por **Hardware Z-Buffer** (`VkPipelineDepthStencilStateCreateInfo`), y la atenuación de luz focal.

### B. Inercia y Suavizado Amortiguado (Spring-Damper Dynamics)
En C++20, la rotación de rueda/teclado utiliza integración numérica RK4 o Spring-Damper amortiguado crítico:

$$a(t) = -k (\theta - \theta_{target}) - c \cdot v(t)$$
$$v(t + \Delta t) = v(t) + a(t) \Delta t$$
$$\theta(t + \Delta t) = \theta(t) + v(t + \Delta t) \Delta t$$

---

## 3. Registro de Arquitectura

- Registrada la modernización de Ring Switcher a 3D real XZ orbit con Vulkan Depth Buffer.
