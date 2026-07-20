# Análisis Matemático y Modernización: Suite Completa de Animaciones (Magic Lamp, Curl, Explode, etc.)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/animation/src/magiclamp.cpp`
- `compiz-0.9.14.2/plugins/animation/src/curvedfold.cpp`
- `compiz-0.9.14.2/plugins/animation/src/dodge.cpp`
- `compiz-0.9.14.2/plugins/animation/src/glide.cpp`

---

## 1. Deconstrucción Matemática de los Sub-Efectos de Animación

El plugin `animation` es el más extenso de Compiz. Deforma la malla 2D/3D de los vértices de la ventana durante las transiciones de apertura (`map`), cierre (`unmap`), minimizado y restaurado.

---

### A. Magic Lamp (Efecto Lámpara Mágica / Genie)

#### 1. Curva Sigmoide de Deformación de Malla
Para estirar la ventana hacia la posición del icono en el dock $(x_{icon}, y_{icon})$, se calcula una interpolación no lineal basada en la función sigmoide:

$$S(f_x) = \frac{1}{1 + e^{-k (f_x - 0.5)}}$$

$$f_y = \frac{S(f_x) - S(0)}{S(1) - S(0)}$$

Donde $f_x \in [0, 1]$ es la posición vertical normalizada en el grid de la ventana.

La posición X interpolada de los vértices de la malla en cada nivel Y:
$$x_{target} = f_y \cdot (x_{orig} - x_{icon}) + x_{icon}$$

#### 2. Modulación de Ondas en Magic Lamp Wavy
Para el efecto de agua/ola de la lámpara, se agregan perturbaciones cosenoidales a lo largo de la malla:

$$\Delta x = \sum_{w=1}^{N_{waves}} \frac{A_w \cdot s_x}{2} \left[ \cos\left( \pi \cdot \frac{f_x - pos_w}{halfWidth_w} \right) + 1 \right]$$

$$x_{target} \leftarrow x_{target} + \Delta x$$

---

### B. Curl / Curved Fold (Doblado de Página)

Simula la física de una página de libro doblándose al cerrarse usando **Curvas de Bézier Cúbicas 3D**:

$$\mathbf{B}(t) = (1-t)^3 \mathbf{P}_0 + 3(1-t)^2 t \mathbf{P}_1 + 3(1-t) t^2 \mathbf{P}_2 + t^3 \mathbf{P}_3, \quad t \in [0, 1]$$

Donde los puntos de control $\mathbf{P}_1$ y $\mathbf{P}_2$ se desplazan en el eje Z para formar la parábola del pliegue.

---

### C. Explode / Triangulación de Desintegración

La ventana se tesela en una malla de $N \times M$ polígonos/triángulos. Cada fragmento $k$ recibe:
- Vector de velocidad lineal inicial $\vec{v}_k = (\text{rand}_x, \text{rand}_y, \text{rand}_z)$.
- Vector de velocidad angular $\vec{\omega}_k$.

Posición del polígono $k$ en el tiempo $t$:
$$\mathbf{P}_k(t) = \mathbf{P}_{k,0} + \vec{v}_k \cdot t + \frac{1}{2} \vec{g} t^2$$
$$\mathbf{R}_k(t) = \mathbf{R}_{k,0} + \vec{\omega}_k \cdot t$$

---

### D. Roll Up (Enrollado Cilíndrico)

Enrolla la superficie de la ventana en un cilindro de radio $R(t) = R_{start} \cdot (1 - t)$:

$$y_{cyl} = y_{bottom} - R \cdot \sin\left(\frac{y_{orig} - y_{bottom}}{R}\right)$$
$$z_{cyl} = R \cdot \left(1 - \cos\left(\frac{y_{orig} - y_{bottom}}{R}\right)\right)$$

---

### E. Dodge (Esquive Dinámico de Foco)

Las ventanas de fondo se apartan automáticamente del área de la ventana enfocada usando un campo de repulsión inversamente proporcional al cuadrado de la distancia:

$$\vec{F}_{repulsion} = \sum_{j} \frac{k_{dodge}}{\|\mathbf{P}_{win} - \mathbf{P}_j\|^2} \hat{\mathbf{r}}_{j}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En Compiz 0.9, estas animaciones requerían teselar la malla en CPU y enviar miles de vértices por frame a la GPU vía VBO dinámicos.

### A. Tessellation Shaders & Vertex Displacement Shaders en Vulkan
- **Vulkan Tessellation Evaluation Shader (`VkPipelineTessellationStateCreateInfo`)**: La ventana se envía como un único quad simple de 4 vértices.
- El **Tessellation Control Shader** subdivide la superficie dinámicamente según la cercanía a la cámara o el nivel de curvatura requerido.
- El **Tessellation Evaluation Shader** evalúa las ecuaciones de Magic Lamp, Curl o Roll Up directamente en paralelo dentro de la GPU.

### B. Geometry / Mesh Shaders para Explode
- En Vulkan 1.3, un **Mesh Shader (`VkPipelineShaderStageCreateInfo`)** descompone el quad en fragmentos de malla independientes y calcula la física de proyectil $\vec{v}_k t + \frac{1}{2} \vec{g} t^2$ a 120 FPS sin tocar la memoria de la CPU.

---

## 3. Registro en CaveMem

- Registrado el pipeline de Vulkan Tessellation & Mesh Shaders para la suite de animaciones en CaveMem.
