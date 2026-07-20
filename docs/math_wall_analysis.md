# Análisis Matemático: Wall/VSwipe (Transición Plana 2D entre Workspaces)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/vswipe.cpp`
- `wayfire/plugins/single_plugins/vswipe-processing.hpp`
- `compiz-0.9.14.2/plugins/wall/src/wall.cpp`

---

## 1. Modelo de Workspace Wall

El **Wall** (pared de escritorios) representa todos los workspaces como un mosaico 2D infinito. La transición entre workspaces consiste en un **deslizamiento del viewport** sobre este mosaico.

```
  Workspace (0,0)  │  Workspace (1,0)  │  Workspace (2,0)
───────────────────┼───────────────────┼──────────────────
  Workspace (0,1)  │  [ACTIVO]         │  Workspace (2,1)
───────────────────┼───────────────────┼──────────────────
  Workspace (0,2)  │  Workspace (1,2)  │  Workspace (2,2)
```

---

## 2. Interpolación del Viewport en la Transición

La animación se maneja como una **interpolación lineal** entre el rectángulo viewport del workspace actual y el del workspace destino.

### A. Rectángulo de Workspace
Para el workspace $(vx, vy)$ con dimensiones $W \times H$ y gap $g$ entre workspaces:
$$R_{ws}(vx, vy) = \{x = (W+g) \cdot vx,\; y = (H+g) \cdot vy,\; w = W,\; h = H\}$$

### B. Interpolación Temporal
El viewport animado interpola entre los rectángulos del workspace actual y el siguiente:

$$viewport(t) = lerp(R_{ws}(vx, vy),\; R_{ws}(vx + dx, vy + dy),\; \alpha(t))$$

Componente a componente ($\alpha_x$ y $\alpha_y$ pueden ser diferentes en movimiento diagonal):
$$viewport.x(t) = (1 - \alpha_x) \cdot R_1.x + \alpha_x \cdot R_2.x$$
$$viewport.y(t) = (1 - \alpha_y) \cdot R_1.y + \alpha_y \cdot R_2.y$$

En la implementación, `smooth_delta.dx` y `smooth_delta.dy` representan el desplazamiento acumulado normalizado $\alpha \in [0, 1]$ por componente.

---

## 3. Procesamiento de Delta del Gesto (Rubber-Band Resistance)

El algoritmo de vswipe incorpora un efecto de resistencia elástica (*rubber-band*) cuando se intenta deslizar más allá del workspace límite:

$$ease(offset) = 1 - |offset|^4 - 0.025$$

$$slowdown = \text{clamp}(ease, s_{min}, 1.0)$$

Donde $s_{min}$ depende de la dirección del movimiento:
- Continuando hacia el límite: $s_{min} = 0.005$ (casi detenido).
- Volviendo desde el límite: $s_{min} = 0.2$ (respuesta más ágil).

**Delta procesado** (con límite de velocidad `speed_cap`):
$$\delta_{proc} = \text{clamp}(\delta_{raw}, -speed\_cap, speed\_cap) \cdot slowdown$$

---

## 4. Criterio de Finalización (Snap Decision)

Al soltar el gesto, se determina si el workspace cambia basándose en:
1. **Umbral de posición** (`move_threshold`, típico 35%): Si el desplazamiento supera el 35% del ancho del workspace → cambiar.
2. **Umbral de velocidad** (`fast_threshold`): Si el último delta de velocidad supera el umbral → cambiar aunque no se haya alcanzado el umbral de posición.

$$target\_dx = \begin{cases} \lfloor dx_{acc} \rfloor + 1 & \text{si } (dx_{acc} - \lfloor dx_{acc} \rfloor > threshold) \lor (v > fast\_threshold) \\ \lfloor dx_{acc} \rfloor & \text{en caso contrario} \end{cases}$$

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. Cada workspace se renderiza en un **framebuffer Vulkan independiente** (`VkFramebuffer`).
2. El viewport de la pared se implementa como un **Scissor Rect dinámico** de Vulkan (`VkRect2D`) combinado con una traslación afín en el Vertex Shader.
3. Los offsets $\alpha_x, \alpha_y$ se pasan como **Push Constants** actualizados cada frame.
