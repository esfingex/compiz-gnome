# Análisis Matemático: Scale/Expo (Vista General de Workspaces)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/expo.cpp`
- `compiz-0.9.14.2/plugins/scale/src/scale.cpp`
- `compiz-0.9.14.2/plugins/expo/src/expo.cpp`

---

## 1. Fundamento Conceptual

El plugin **Expo/Scale** presenta todos los workspaces (o ventanas) simultáneamente como una cuadrícula escalada al tamaño de la pantalla, permitiendo al usuario navegar visualmente entre ellos.

---

## 2. Transformación de Viewport (Expo)

### A. Grid de Workspaces
Sea la grilla de escritorios de $N_w \times N_h$ workspaces, cada uno de resolución $W \times H$ píxeles.

El **Wall Virtual** (pared completa de workspaces) tiene dimensiones:
$$W_{wall} = N_w \times W + (N_w + 1) \times gap$$
$$H_{wall} = N_h \times H + (N_h + 1) \times gap$$

### B. Animación de Zoom-Out (Viewport Scaling)
La transición se maneja mediante una **animación de geometría interpolada** entre dos rectángulos viewport:

- **Estado inicial** (activo en un workspace): El viewport es un rectángulo del tamaño exacto del workspace actual.
- **Estado final** (expo activo): El viewport es un rectángulo que engloba **toda la pared** de workspaces más un margen centrado para igualar la relación de aspecto:

$$W_{full} = (gap + W) \times \max(N_w, N_h) + gap$$
$$H_{full} = (gap + H) \times \max(N_w, N_h) + gap$$

El centrado del wall en pantalla se realiza ajustando la posición x/y del viewport:
$$x_{rect} -= \frac{W_{full} - W_{wall}}{2}, \quad y_{rect} -= \frac{H_{full} - H_{wall}}{2}$$

### C. Interpolación Temporal (Lerp Animado)
En cada frame, la posición y tamaño del viewport se interpola suavemente entre start y end:

$$viewport(t) = lerp(viewport_{start}, viewport_{end}, ease(t))$$

Donde `ease(t)` es una curva de suavizado (típicamente cúbica o cuadrática).

---

## 3. Scale: Transformaciones Afines de Ventanas

El plugin **Scale** (dentro del contexto de un workspace) aplica una transformación afín 2D a cada ventana para que quepa en su celda asignada de la cuadrícula.

### A. Algoritmo de Asignación de Ventanas (Slot Assignment)
Compiz usa un algoritmo de empaquetado que divide la pantalla en columnas y asigna ventanas de arriba a abajo, optimizando el espacio desperdiciado (similar a un algoritmo *bin-packing greedy*).

### B. Transformación Afín por Ventana
Para una ventana $i$ con bounding box $(bx_i, by_i, bw_i, bh_i)$ y slot asignado $(sx_i, sy_i, sw_i, sh_i)$:

**Factor de escala uniforme** (mantiene aspecto):
$$s_i = \min\left(\frac{sw_i}{bw_i}, \frac{sh_i}{bh_i}\right)$$

**Traslación para centrar en el slot**:
$$\Delta x_i = sx_i + \frac{sw_i - bw_i \cdot s_i}{2} - bx_i$$
$$\Delta y_i = sy_i + \frac{sh_i - bh_i \cdot s_i}{2} - by_i$$

**Matriz de transformación 2D homogénea**:
$$M_i = \begin{pmatrix} s_i & 0 & \Delta x_i \\ 0 & s_i & \Delta y_i \\ 0 & 0 & 1 \end{pmatrix}$$

### C. Highlight de Workspace Activo
Cada workspace tiene una animación de atenuación de brillo (`ws_fade`):
- **Workspace seleccionado**: brightness $= 1.0$
- **Workspaces no seleccionados**: brightness $= inactive\_brightness$ (ej. $0.7$)

El color se multiplica en el fragment shader: `fragColor.rgb *= brightness`.

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor:
1. La animación de viewport se implementa con una interpolación lineal suave en el Vertex Shader usando matrices de proyección ortogonal 2D.
2. El escalado de ventanas se gestiona con **Push Constants** de Vulkan actualizados por frame: `{scale_x, scale_y, offset_x, offset_y}`.
3. El atenuado de brightness se aplica como un multiplicador uniforme en el Fragment Shader.
