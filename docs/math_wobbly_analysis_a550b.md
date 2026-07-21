# Análisis Matemático y Modelo Físico: Wobbly Windows (Ventanas Gelatinosas)

## 1. Fundamentos Físicos

El efecto de *Wobbly Windows* (ventanas gelatinosas) emula el comportamiento de un material elástico mediante una red de **masa-resorte-amortiguador** (Spring-Damper Mesh Grid) combinada con **Superficies de Bézier Bi-cúbicas**.

```
  (O_0,0)───[Spring]───(O_1,0)───[Spring]───(O_2,0)
     │                    │                    │
  [Spring]             [Spring]             [Spring]
     │                    │                    │
  (O_0,1)───[Spring]───(O_1,1)───[Spring]───(O_2,1)
```

---

## 2. Ecuaciones Diferenciales Discretizadas

Cada objeto de la malla $O_i$ tiene posición $\vec{r}_i$, velocidad $\vec{v}_i$ y masa $m$ ($WOBBLY\_MASS = 15.0$).

### A. Ley de Hooke (Fuerza Elástica del Resorte)
Para dos objetos conectados $a$ y $b$ con desplazamiento ideal de reposo $\vec{d}_{offset}$:

$$\vec{\Delta r} = \frac{1}{2} \cdot (\vec{r}_b - \vec{r}_a - \vec{d}_{offset})$$

$$\vec{F}_{spring\_a} = k \cdot \vec{\Delta r}$$
$$\vec{F}_{spring\_b} = -k \cdot \vec{\Delta r}$$

Donde $k$ ($MINIMAL\_SPRING\_K = 0.1$, $MAXIMAL\_SPRING\_K = 10.0$) es la constante de rigidez.

### B. Amortiguamiento Viscoso (Fricción)
Para evitar oscilaciones infinitas, se aplica una fuerza de arrastre opuesta a la velocidad:

$$\vec{F}_{friction} = -\mu \cdot \vec{v}_i$$

Donde $\mu$ es la constante de fricción ($MINIMAL\_FRICTION = 0.1$, $MAXIMAL\_FRICTION = 10.0$).

### C. Integración Temporal (Euler / Verlet)
En cada paso del bucle de simulación ($\Delta t \approx 16ms$):

$$\vec{a}_i = \frac{\vec{F}_{total}}{m} = \frac{\vec{F}_{spring} + \vec{F}_{friction}}{m}$$

$$\vec{v}_i(t + \Delta t) = \vec{v}_i(t) + \vec{a}_i \cdot \Delta t$$

$$\vec{r}_i(t + \Delta t) = \vec{r}_i(t) + \vec{v}_i(t + \Delta t) \cdot \Delta t$$

---

## 3. Interpolación Bézier Bi-cúbica (Smooth Surface Patch)

Para evitar que la ventana se vea como un polígono rígido de $4 \times 4$ puntos, los vértices finales del renderizado se calculan evaluando un **Parche de Bézier de orden 3**:

$$P(u, v) = \sum_{i=0}^{3} \sum_{j=0}^{3} B_i^3(u) \cdot B_j^3(v) \cdot \vec{r}_{i,j}$$

Donde los polinomios de Bernstein son:
- $B_0^3(t) = (1 - t)^3$
- $B_1^3(t) = 3t(1 - t)^2$
- $B_2^3(t) = 3t^2(1 - t)$
- $B_3^3(t) = t^3$

Para cualquier punto normado $(u, v) \in [0, 1] \times [0, 1]$ de la textura de la ventana, $P(u, v)$ retorna la coordenada deformada $(x', y')$.

---

## 4. Implementación en C++20 / Vulkan

En el motor `compiz-gnome`:
1. La simulación de los $N \times M$ resortes se puede ejecutar de dos formas:
   - **CPU**: En un hilo dedicado (`std::jthread`) en C++20 usando `std::vector` alineado con SIMD (AVX2).
   - **GPU**: Mediante un **Compute Shader en Vulkan** que calcula las fuerzas de todos los nodos en paralelo.
2. Las coordenadas deformadas se pasan directamente a un **Vertex Shader de Vulkan** para proyectar la textura de la ventana.
