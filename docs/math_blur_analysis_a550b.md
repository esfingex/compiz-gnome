# Análisis Matemático: Blur (Desenfoque Gausiano y Kawase)

## Fuentes de Referencia
- `wayfire/plugins/blur/gaussian.cpp`
- `wayfire/plugins/blur/kawase.cpp`
- `compiz-0.9.14.2/plugins/blur/src/blur.cpp`

---

## 1. Fundamentos del Desenfoque

Un desenfoque de imagen consiste en reemplazar el valor de color de cada píxel $(x, y)$ por una media ponderada de sus vecinos. La diferencia entre algoritmos reside en la forma de esa función de ponderación y su eficiencia computacional.

---

## 2. Desenfoque Gaussiano de Dos Pasadas (Separable Gaussian Blur)

### Principio de Separabilidad
La función Gaussiana 2D es **separable**: se puede descomponer en dos convoluciones 1D independientes (horizontal y vertical), lo que reduce la complejidad de $O(n^2)$ a $O(2n)$ por píxel.

$$G(x, y) = \frac{1}{2\pi\sigma^2} e^{-\frac{x^2 + y^2}{2\sigma^2}} = G_x(x) \cdot G_y(y)$$

### Implementación en Wayfire (5-tap aproximación discreta)
El shader de Wayfire implementa una aproximación eficiente de 5 muestras por pase con pesos pre-calculados que suman 1.0:

**Kernel 1D de 5 muestras (Pesos)**:

| Desplazamiento | Peso |
| :--- | :--- |
| $0$ (centro) | $0.204164$ |
| $\pm 1.5$ píxeles | $0.304005$ |
| $\pm 3.5$ píxeles | $0.093913$ |

**Verificación**: $0.204164 + 2 \times 0.304005 + 2 \times 0.093913 = 1.000000$ ✓

```glsl
// Pase Horizontal (del shader de Wayfire):
bp += texture2D(bg_texture, vec2(blurcoord[0].x, uv.y)) * 0.204164; // centro
bp += texture2D(bg_texture, vec2(blurcoord[1].x, uv.y)) * 0.304005; // +1.5px
bp += texture2D(bg_texture, vec2(blurcoord[2].x, uv.y)) * 0.304005; // -1.5px
bp += texture2D(bg_texture, vec2(blurcoord[3].x, uv.y)) * 0.093913; // +3.5px
bp += texture2D(bg_texture, vec2(blurcoord[4].x, uv.y)) * 0.093913; // -3.5px
```

Las coordenadas de muestreo se precalculan en el **vertex shader** para ahorrar cálculos en el fragment shader (optimización de GPU).

---

## 3. Desenfoque Kawase (Dual Kawase Filter)

El filtro Kawase es significativamente más eficiente para desenfoque grande. Combina:
1. **Etapa de Downsampling**: Reducción iterativa de la resolución.
2. **Etapa de Upsampling**: Reconstrucción con desenfoque implícito.

### A. Etapa de Downsampling (Muestreo hacia abajo)
En cada iteración $i$, la resolución se reduce a la mitad ($W/2^i \times H/2^i$) y se muestrea con un patrón de diamante de 5 puntos:

$$\text{pixel}_{out} = \frac{1}{8}\left(4 \cdot T(uv) + T(uv - \Delta) + T(uv + \Delta) + T(uv + \Delta') + T(uv - \Delta')\right)$$

Donde $\Delta = (halfpixel.x \cdot offset, halfpixel.y \cdot offset)$ y $\Delta'$ es la versión en espejo diagonal.

### B. Etapa de Upsampling (Muestreo hacia arriba)
La reconstitución usa un patrón de 8 muestras con un filtro de tent:

$$\text{pixel}_{out} = \frac{1}{12}\left(\sum_{i \in \text{diagonales}} 2 \cdot T(uv + d_i) + \sum_{j \in \text{ortogonales}} T(uv + d_j)\right)$$

### Radio de Desenfoque Efectivo
$$r_{kawase} = 2^{iterations + 1} \cdot offset \cdot degrade\_factor$$

---

## 4. Implementación en C++20 / Vulkan (Compute Shaders)

En nuestro motor:
- El Gaussian Blur se implementa con 2 **Compute Shaders de Vulkan** (pase horizontal + vertical), usando un `sampler2D` con filtrado `VK_FILTER_LINEAR`.
- El Kawase Blur requiere **framebuffers intermedios** (`VkImage` con `VK_IMAGE_USAGE_STORAGE_BIT`) para las etapas de downsampling y upsampling.
- Ambos operan sobre la textura de la ventana capturada vía `dma-buf` sin copias adicionales.
