# Análisis Matemático: Switcher 3D (Navegador de Ventanas Alt-Tab)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/switcher.cpp`
- `compiz-0.9.14.2/plugins/switcher/src/switcher.cpp`
- `compiz-0.9.14.2/plugins/ring/src/ring.cpp`
- `compiz-0.9.14.2/plugins/shift/src/shift.cpp`

---

## 1. Modelo de Posicionamiento de 3 Slots

El switcher organiza las ventanas en un carrusel de 3 posiciones relativas:

```
  LEFT (-1)          CENTER (0)         RIGHT (+1)
┌──────────┐     ┌──────────────┐     ┌──────────┐
│  Vista   │     │ Vista Activa │     │  Vista   │
│ (atrás)  │     │ (al frente)  │     │ (atrás)  │
└──────────┘     └──────────────┘     └──────────┘
off_x < 0         off_x = 0           off_x > 0
off_z = -1.0      off_z = 0.0         off_z = -1.0
scale = 0.66      scale = 1.0         scale = 0.66
```

---

## 2. Transformaciones Animadas por Slot

Cada vista tiene un conjunto de atributos animados (`SwitcherPaintAttribs`) con transiciones temporizadas:

| Atributo | Centro (Activo) | Izquierda/Derecha (Secundario) |
| :--- | :--- | :--- |
| `off_x` | $0$ | $\pm W / 3$ |
| `off_z` | $0$ | $-1.0$ |
| `scale_x = scale_y` | $s_{fit}$ | $s_{fit} \times 0.66$ |
| `rotation` (eje Y) | $0$ | $\pm \theta_{rot}$ |
| `alpha` | $1.0$ | $0.7$ |

### A. Factor de Escala para Ajuste a Pantalla
Cada ventana se escala para ocupar como máximo el $45\%$ del ancho o alto de la pantalla:
$$s_{fit} = \min\left(\frac{0.45 \cdot W_{screen}}{bw_{view}}, \frac{0.45 \cdot H_{screen}}{bh_{view}}, 1.0\right) \times thumbnail\_scale$$

### B. Traslación Lateral al Centro de la Pantalla
Para centrar la ventana en la posición CENTER antes de cualquier desplazamiento lateral:
$$\Delta x = \frac{W_{screen}}{2} - \frac{bw_{view}}{2} - bx_{view}$$
$$\Delta y = by_{view} - \left(\frac{H_{screen}}{2} - \frac{bh_{view}}{2}\right)$$

### C. Movimiento entre Slots (Dirección $d \in \{-1, +1\}$)
Al presionar Tab, cada vista se desplaza en la dirección $d$:

$$\Delta x_{new} = \Delta x_{old} + \frac{W_{screen}}{3} \cdot d$$

**Cambio de profundidad $Z$** según el movimiento:
- Si la vista sale del **centro** → sube en Z ($z\_sign = +1$): $\Delta z_{new} = \Delta z_{old} + (-1.0)$
- Si la vista **llega al centro** desde lateral → baja en Z ($z\_sign = -1$): $\Delta z_{new} = \Delta z_{old} + (+1.0)$
- Si la vista **expira** (sale del rango) → $z\_sign = 0$

**Escala al moverse**:
$$s_{new} = s_{old} \times 0.66^{z\_sign}$$

**Rotación en eje Y** (en radianes):
$$\theta_{new} = \theta_{old} + \theta_{rot} \cdot d \quad \text{donde } \theta_{rot} = -\text{view\_thumbnail\_rotation} \cdot \frac{\pi}{180}$$

---

## 3. Matriz de Transformación 3D Final por Vista

La transformación completa de cada vista en el switcher combina:

$$M_{vista} = T(\Delta x, \Delta y, \Delta z) \cdot S(s_x, s_y, 1.0) \cdot R_y(\theta)$$

Donde en GLM (tal como lo implementa Wayfire):
```cpp
transform->translation = glm::translate(glm::mat4(1.0), {off_x, off_y, off_z});
transform->scaling     = glm::scale(glm::mat4(1.0), {scale_x, scale_y, 1.0});
transform->rotation    = glm::rotate(glm::mat4(1.0), rotation, {0.0, 1.0, 0.0});
```

---

## 4. Ordenamiento de Profundidad (Z-Sort sin Depth Buffer)
Para vistas con valores $Z$ similares se usa el **orden de apilamiento** de ventanas del sistema:
- Vistas con mayor $Z$ (más cercanas) se renderizan **después** (por encima).
- Al no usar depth testing, se ordena con un bubble-sort estable.

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. Cada ventana se renderiza con sus propias **Push Constants** de translación, escala y rotación.
2. Las interpolaciones temporales de los atributos se implementan como `std::lerp` en C++20 sincronizado con el timestamp del frame (`VkSubmitInfo`).
3. El ordenamiento Z se hace en CPU antes de cada `vkCmdDraw`.
