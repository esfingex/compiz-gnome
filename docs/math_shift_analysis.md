# Análisis Matemático y Modernización: Plugin Shift (Cover Flow & Flip Switcher con Reflexión Planar)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/shift/src/shift.cpp`
- `compiz-0.9.14.2/plugins/shift/src/shift.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `shift` implementa dos modos de navegación 3D de ventanas: **ModeCover** (estilo Apple Cover Flow) y **ModeFlip** (estilo apilamiento lineal en ángulo), ambos con soporte de **Reflexión Planar en el Suelo**.

---

### A. Modo Cover Flow (`layoutThumbsCover`)

Para cada ventana $i$ con distancia flotante al foco $d = \text{mMvTarget} - i$:

#### 1. Zona Central ($|d| < 1.0$)
La ventana transiciona suavemente hacia o desde el centro del escenario usando una proyección sinusoidal de $90^\circ$:

$$\text{pos} = \sin\left(d \cdot \frac{\pi}{2}\right)$$
$$x(d) = C_x + \text{pos} \cdot \text{space} \cdot \text{extraSpace}$$
$$z(d) = -|d| \cdot \frac{\text{maxThumbWidth}}{2 \cdot W_{screen}}$$
$$\theta_{rot}(d) = -\text{pos} \cdot \theta_{cover}$$

Donde $\theta_{cover} \approx 65^\circ$ es el ángulo de inclinación lateral del abanico.

#### 2. Zona Lateral ($|d| \ge 1.0$)
Las ventanas laterales se disponen en un arco logarítmico con ángulo decreciente para evitar solapamientos excesivos:

$$\text{ang}(d) = \left(\frac{\pi}{\max(72, 2N)}\right) \cdot (d - \text{pos}) + \text{pos} \cdot \left(\frac{\pi}{6}\right)$$
$$x(d) = C_x + \sin(\text{ang}) \cdot \text{rad} \cdot W_{screen} \cdot \text{extraSpace}$$
$$\theta_{rot}(d) = -\text{pos} \cdot \left( \theta_{cover} + 30^\circ - |\text{ang}| \cdot \frac{180^\circ}{\pi} \right)$$
$$z(d) = -\left(\frac{\text{maxThumbWidth}}{2 W_{screen}}\right) - \cos\left(\frac{\pi}{6}\right) \cdot \text{rad} + \cos(\text{ang}) \cdot \text{rad}$$

---

### B. Modo Flip (`layoutThumbsFlip`)

Las ventanas forman una fila en diagonal inclinada con ángulo constante $\phi_{flip}$:

$$x(d) = C_x + \sin(\phi_{flip}) \cdot d \cdot \left(\frac{\text{maxThumbWidth}}{2}\right)$$
$$z(d) = \cos(\phi_{flip}) \cdot d \cdot \left(\frac{\text{maxThumbWidth}}{2 W_{screen}}\right)$$
$$\text{opacity}(d) = \max(0.0, 1.0 - d)$$

---

### C. Reflexión Planar sobre el Suelo (Floor Mirroring)
En Compiz 0.9, el reflejo en el suelo se lograba dibujando la ventana dos veces: la segunda vez con escalado vertical invertido $S_y = -1$:

$$\mathbf{M}_{reflect} = \mathbf{T}(x, y_{floor}, z) \cdot \mathbf{S}(s_x, -s_y, 1) \cdot \mathbf{R}_y(\theta)$$
$$\alpha_{reflect} = \alpha_{win} \cdot \text{reflectBrightness}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En `compiz-gnome` C++20/Vulkan:

### A. Matriz de Reflexión Planar Homogénea 4x4 Exacta
Sustituimos la inversión ad-hoc $S_y = -1$ por la matriz de reflexión planar formal contra el plano del suelo $\Pi = (0, 1, 0, -y_{floor})^T$:

$$\mathbf{R}_{plane} = \mathbf{I} - 2 \mathbf{N} \mathbf{N}^T = \begin{pmatrix} 1 & 0 & 0 & 0 \\ 0 & -1 & 0 & 2 y_{floor} \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

Matriz final del reflejo:
$$\mathbf{M}_{reflect} = \mathbf{R}_{plane} \cdot \mathbf{M}_{window}$$

### B. Desenfoque de Suelo Glossy / Matte con Vulkan Compute Shader
En lugar de un reflejo de espejo perfecto y plano:
1. Se renderiza la reflexión en una textura temporal.
2. Un **Compute Shader de Desenfoque Gaussiano 1D** aplica un blur dependiente de la distancia vertical al suelo:
   $$\sigma(y) = \sigma_{base} + k \cdot (y_{floor} - y)$$
3. El resultado genera un suelo de mármol/cristal esmerilado (*glossy floor*) de calidad cinematográfica.

---

## 3. Registro de Arquitectura

- Registrada la matriz de reflexión planar $\mathbf{R}_{plane}$ y el blur de suelo Glossy con Compute Shader.
