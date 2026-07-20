# Análisis Matemático: Zoom de Pantalla (Magnificación Digital)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/zoom.cpp`
- `compiz-0.9.14.2/plugins/ezoom/src/zoom.cpp`

---

## 1. Modelo de Magnificación

El zoom de pantalla implementa una **magnificación digital** del framebuffer final antes de la presentación. Es un efecto **post-process** (opera sobre el framebuffer completo, no sobre ventanas individuales).

---

## 2. Transformación de Viewport (Blit Recortado)

El truco fundamental es **renderizar una región reducida del framebuffer al tamaño completo de la pantalla**. Esto produce el efecto de ampliación sin alterar la geometría de ninguna ventana.

### A. Factor de Zoom
El factor de magnificación $f \in [1, 50]$ se actualiza con la rueda del ratón:
$$f_{new} = f_{old} - f_{old} \cdot \delta \cdot speed$$

Donde $\delta$ es el desplazamiento de la rueda. Esta fórmula produce un **zoom multiplicativo** (proporcional al nivel actual), lo que hace que los saltos de zoom sean perceptualmente uniformes tanto en valores bajos como altos.

### B. Coordenadas del Rectángulo de Origen (Source Region)
Partiendo de la posición del cursor $(c_x, c_y)$ en coordenadas de framebuffer:

**Escala de compensación:**
$$scale = \frac{f - 1}{f}$$

**Esquina superior izquierda del rectángulo de lectura:**
$$x_1 = \text{clamp}(c_x \cdot scale, \; 0, \; W-1)$$
$$y_1 = \text{clamp}(c_y \cdot scale, \; 0, \; H-1)$$

**Ancho y Alto del rectángulo de lectura:**
$$tw = \text{clamp}(W / f, \; 0, \; W - x_1)$$
$$th = \text{clamp}(H / f, \; 0, \; H - y_1)$$

**Interpretación**: Se lee una región de $tw \times th$ píxeles (más pequeña que la pantalla completa) centrada cerca del cursor, y se estira para llenar toda la pantalla de $W \times H$ píxeles.

### C. Ejemplo Numérico
Para una pantalla de $1920 \times 1080$, cursor en $(960, 540)$, factor $f = 2$:
- $scale = (2-1)/2 = 0.5$
- $x_1 = 960 \times 0.5 = 480, \; y_1 = 540 \times 0.5 = 270$
- $tw = 1920/2 = 960, \; th = 1080/2 = 540$

El resultado: la región central de $960 \times 540$ se estira a $1920 \times 1080$ → zoom 2x centrado en el cursor.

---

## 3. Interpolación (Linear vs. Nearest Neighbor)

El filtrado de la textura al escalar determina la calidad:
- `BILINEAR` (`WLR_SCALE_FILTER_BILINEAR`): Interpolación suave entre píxeles. Recomendado para texto y UI.
- `NEAREST` (`WLR_SCALE_FILTER_NEAREST`): Sin interpolación. Para precisión pixel-perfect en diseño/programación.

---

## 4. Animación del Factor de Zoom
El cambio de factor no es instantáneo, sino interpolado suavemente con una **animación de progresión simple**:

$$f(t) = lerp(f_{start}, f_{target}, ease(t))$$

El hook de renderizado se desactiva automáticamente cuando $|f - 1| \leq 0.01$ (zoom prácticamente en 1x).

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. El zoom es un **post-process Vulkan** que opera sobre el `VkImage` del framebuffer final.
2. Se usa `vkCmdBlitImage` con filtro `VK_FILTER_LINEAR` o `VK_FILTER_NEAREST` para copiar la región recortada al framebuffer de presentación.
3. El factor y las coordenadas de la región se actualizan cada frame como `Push Constants`.
