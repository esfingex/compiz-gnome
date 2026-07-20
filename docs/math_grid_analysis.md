# Análisis Matemático: Grid (Snapping Adaptativo de Ventanas a Zonas)

## Fuentes de Referencia
- `wayfire/plugins/grid/grid.cpp`
- `compiz-0.9.14.2/plugins/grid/src/grid.cpp`

---

## 1. Concepto de Zonas de Snapping

El plugin **Grid** divide el área de trabajo (workarea) en una cuadrícula lógica de zonas, y asigna la geometría de la ventana a la zona más cercana cuando el usuario la arrastra a su borde.

Las 9 zonas estándar corresponden a las posiciones del teclado numérico:

```
 ┌──────────┬──────────┬──────────┐
 │  7 (TL)  │  8 (TC)  │  9 (TR)  │   Top
 ├──────────┼──────────┼──────────┤
 │  4 (ML)  │  5 (MAX) │  6 (MR)  │   Middle
 ├──────────┼──────────┼──────────┤
 │  1 (BL)  │  2 (BC)  │  3 (BR)  │   Bottom
 └──────────┴──────────┴──────────┘
  Left       Center     Right
```

---

## 2. Mapa de Anchos de Slot

Compiz Grid define proporciones de slot basadas en fracciones del `workarea.width`:

| Fracción | Nombre | Píxeles (en W=1920) |
| :--- | :--- | :--- |
| $W / 8$ | slotWidth12 | 240 |
| $W / 6$ | slotWidth17 | 320 |
| $W / 4$ | slotWidth25 | 480 |
| $W / 3$ | slotWidth33 | 640 |
| $W / 2$ | slotWidth50 | 960 |
| $W - W/3$ | slotWidth66 | 1280 |
| $W - W/4$ | slotWidth75 | 1440 |
| $W$ | slotWidth100 | 1920 |

---

## 3. Algoritmo de Selección de Slot

Para cada zona del teclado numérico, se calcula el rectángulo destino:

**Zona 4 (Mitad Izquierda):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W/2, \; h = H\}$$

**Zona 6 (Mitad Derecha):**
$$slot = \{x = workarea.x + W/2, \; y = workarea.y, \; w = W/2, \; h = H\}$$

**Zona 7 (Cuarto Superior Izquierdo):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W/2, \; h = H/2\}$$

**Zona 5 (Máximizado):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W, \; h = H\}$$

### Corrección por Decoraciones de Ventana
El rectángulo efectivo de la ventana se ajusta por los bordes de la decoración:
$$rect_{actual} = \{x + border.left, \; y + border.top, \; w - border.left - border.right, \; h - border.top - border.bottom\}$$

---

## 4. Animación de Snapping (Interpolación Animada)

Cuando una ventana hace snap a una zona, la transición de su geometría se anima suavemente:

$$geometry(t) = lerp(geometry_{current}, geometry_{target}, ease(t))$$

Donde $ease(t)$ es tipicamente una curva smooth-step:
$$ease(t) = 6t^5 - 15t^4 + 10t^3 \quad \text{(Smoother Step)}$$

La geometría original de la ventana se guarda en `originalSize` para poder revertir el snap.

---

## 5. Detección de Zona por Posición del Cursor

En Wayfire (plugin `tile`/`grid`), el snap se detecta basándose en la posición relativa del cursor dentro de la pantalla, dividiendo el espacio en regiones de borde:

- **Borde izquierdo** ($x < threshold$): Snap a zona izquierda.
- **Borde superior** ($y < threshold$): Snap a zona superior.
- **Borde superior izquierdo** ($x < threshold \land y < threshold$): Snap a cuarto superior izquierdo.

El umbral `threshold` es configurable (típicamente $10-20$px del borde).

---

## 6. Implementación en C++20 / Vulkan

En nuestro motor:
1. El snapping es pura **aritmética de rectángulos en CPU** (sin GPU).
2. La animación de la geometría de ventana usa **`std::lerp` de C++20** para interpolar `{x, y, width, height}` frame a frame.
3. El resultado se aplica como una **transformación de escala y traslación** en el Vertex Shader de Vulkan:

```glsl
layout(push_constant) uniform GridTransform {
    float scale_x, scale_y;
    float translate_x, translate_y;
} grid;
```
