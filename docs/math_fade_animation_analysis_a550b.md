# Análisis Matemático: Fade & Animaciones Generales de Ventana (Aparecer/Desaparecer)

## Fuentes de Referencia
- `wayfire/plugins/animate/animate.cpp`
- `wayfire/plugins/animate/basic_animations.hpp`
- `wayfire/plugins/animate/zap.hpp`
- `wayfire/plugins/animate/fire/fire.cpp`
- `compiz-0.9.14.2/plugins/fade/src/fade.cpp`
- `compiz-0.9.14.2/plugins/animation/src/animation.cpp`

---

## 1. Marco General de Animaciones de Ventana

Todas las animaciones de apertura/cierre de ventana comparten la misma estructura de ciclo de vida:
- **APPEAR**: Animación cuando una ventana aparece (`map`).
- **CLOSE (HIDE)**: Animación cuando una ventana se cierra o minimiza (`unmap`).

Cada efecto implementa la interfaz `animation_base_t`:
```cpp
void init(wayfire_view view, duration_t dur, animation_type type);
bool step(); // retorna true mientras la animación está corriendo
void reverse();
```

---

## 2. Fade (Fundido de Opacidad)

El más simple. Interpola el canal alpha de 0 a 1 (o 1 a 0 al cerrar).

$$\alpha(t) = lerp(0, 1, ease(t))$$

Implementado como una `simple_animation_t` (transición temporizad, curva de suavizado configurable).

**En el motor Vulkan**: Multiplicar el valor alpha en el Fragment Shader: `fragColor.a *= alpha_value`.

---

## 3. Zoom-Scale (Aparecer desde el Punto de Activación)

Combina 4 atributos animados simultáneamente con la misma duración:

| Atributo | Inicio (APPEAR) | Fin (APPEAR) |
| :--- | :--- | :--- |
| `alpha` | $0.0$ | $1.0$ |
| `zoom` (escala uniforme) | $1/3$ | $1.0$ |
| `offset_x` | $hint\_cx - view\_cx$ | $0$ |
| `offset_y` | $hint\_cy - view\_cy$ | $0$ |

Donde $(hint\_cx, hint\_cy)$ es el punto de origen de apertura (ej. posición del icono en el dock).

La matriz de transformación en cada frame:
$$M(t) = T(offset\_x(t), offset\_y(t)) \cdot S(zoom(t), zoom(t))$$

---

## 4. Zap (Desintegración Eléctrica)

El efecto zap simula una descarga eléctrica usando curvas de Bézier cuadráticas deformadas aleatoriamente.

### A. Generación del Rayo
La posición del rayo en cada segmento $(x_i, y_i)$ se calcula con ruido aditivo:
$$x_i = lerp(x_{start}, x_{end}, t_i) + \delta_x^{(i)}$$

Donde $\delta_x^{(i)} \sim U(-amplitude, amplitude)$ es un desplazamiento aleatorio por segmento.

### B. Decaimiento de Amplitud
La amplitud de la perturbación disminuye hacia el final de la animación:
$$amplitude(t) = A_{max} \cdot (1 - progress(t))$$

---

## 5. Interpolación Temporal: Curvas de Suavizado (Easing)

Todos los efectos usan `timed_transition_t` que implementa una de las curvas clásicas de interpolación:

$$ease_{smooth}(t) = 3t^2 - 2t^3 \quad \text{(Smoothstep)}$$

$$ease_{cubic}(t) = t^3 \quad \text{(Ease-in cúbico)}$$

$$ease_{elastic}(t) = 1 - e^{-5t}\cos(10\pi t) \quad \text{(Rebote elástico)}$$

La duración se mide en milisegundos y el progreso $t \in [0, 1]$ se calcula como:

$$t = \text{clamp}\left(\frac{elapsed\_ms}{duration\_ms}, 0, 1\right)$$

---

## 6. Tabla Comparativa de Efectos de Animación

| Plugin | Técnica | Parámetros Clave |
| :--- | :--- | :--- |
| **Fade** | Interpolación de alpha | `duration_ms`, curva de easing |
| **Zoom** | Scale + Translate + Alpha | `zoom_start` (1/3), `hint_cx/cy` |
| **Fire** | Sistema de partículas | `fire_particles`, `fire_particle_size` |
| **Zap** | Rayo Bézier deformado | `zap_segments`, `amplitude` |
| **Spin** | Rotación Z + Alpha | `spin_duration`, `rotation_angle` |

---

## 7. Implementación en C++20 / Vulkan

En nuestro motor:
1. Las animaciones de opacidad se implementan con un **Uniform Buffer Object (UBO)** de alpha.
2. Las animaciones de escala y traslación usan **Push Constants** por frame.
3. El sistema de partículas (Fire/Zap) corre en un **Compute Shader** de Vulkan.
4. La función de easing se evalúa en CPU para minimizar el trabajo del shader.
