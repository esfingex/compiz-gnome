# Análisis Matemático: Rotate y Wrot (Rotación 2D y 3D Libre de Ventanas)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/wrot.cpp`
- `compiz-0.9.14.2/plugins/rotate/src/rotate.cpp`

---

## 1. Modos de Rotación

El plugin `wrot` (Window Rotation) implementa dos modos distintos de transformación:

| Modo | Eje de Rotación | Tipo de Transformación |
| :--- | :--- | :--- |
| **ROT_2D** | Eje Z (plano de la pantalla) | `view_2d_transformer_t` |
| **ROT_3D** | Eje arbitrario en 3D | `view_3d_transformer_t` |

---

## 2. Rotación 2D (Plano de Pantalla, Eje Z)

### Fundamento: Ángulo del Producto Vectorial
Cuando el usuario arrastra el ratón alrededor del centro de la ventana, el ángulo de rotación acumulado se calcula usando el **producto vectorial** entre el vector anterior y el actual:

Sea $\vec{v_1} = (x_1 - c_x, y_1 - c_y)$ y $\vec{v_2} = (x_2 - c_x, y_2 - c_y)$ los vectores desde el centro $(c_x, c_y)$ al cursor:

$$\theta_{delta} = -\arcsin\left(\frac{\vec{v_1} \times \vec{v_2}}{|\vec{v_1}| \cdot |\vec{v_2}|}\right) = -\arcsin\left(\frac{x_1 y_2 - x_2 y_1}{|\vec{v_1}| \cdot |\vec{v_2}|}\right)$$

El ángulo acumulado de la ventana se actualiza:
$$\theta_{total} = \theta_{total} + \theta_{delta}$$

Dado que $|\vec{v_1}|$ y $|\vec{v_2}|$ están normalizados, $\arcsin$ produce el ángulo exacto de giro entre los dos vectores en cada frame.

**Reset automático**: Si el cursor cae dentro de un radio `reset_radius` del centro, la transformación se elimina.

---

## 3. Rotación 3D Libre (Eje Arbitrario)

### Fundamento: Quaterniones / Rotation Accumulation (Rodrigues)
En modo 3D, el movimiento del ratón $(dx, dy)$ define un **eje de rotación** en el plano de la pantalla perpendicular al desplazamiento y una **magnitud de ángulo** proporcional a la velocidad:

$$\vec{axis} = (dir \cdot dy, dir \cdot dx, 0)$$

$$\theta_{delta} = |\vec{v}_{mouse}| \cdot ascale \quad \text{donde } ascale = \frac{sensitivity}{60} \cdot \frac{\pi}{180}$$

La rotación se acumula multiplicando la matriz de rotación existente:
```cpp
rotation = glm::rotate(rotation, vlen(dx, dy) * ascale, {dir*dy, dir*dx, 0});
```

Esto es equivalente a la acumulación de quaterniones de rotación compuesta:
$$Q_{total} = Q_{delta} \otimes Q_{total}$$

### Protección Anti-Singular (Normal Dot Product)
Al soltar el ratón, se verifica si la ventana quedó perpendicular a la pantalla (normal $\vec{n} = (0,0,1)$):
$$\cos(\alpha) = \vec{n} \cdot (Q_{total} \cdot \vec{n})$$

Si $|\cos(\alpha)| < 0.05$ (la ventana está casi de canto), se aplica una pequeña corrección de $\pm 2.5°$ para evitar que la ventana quede "atrapada" en posición invisible.

---

## 4. Implementación en C++20 / Vulkan

1. **Rotación 2D**: Uniform Buffer `{angle_radians}` → Fragment Shader aplica `mat2(cos, -sin, sin, cos)` sobre las coordenadas UV de la textura.
2. **Rotación 3D**: Uniform Buffer `{rotation_matrix_4x4}` → Vertex Shader multiplica la posición de cada vértice.
3. Ambos usan **Push Constants** en Vulkan por su tamaño pequeño y acceso rápido.
