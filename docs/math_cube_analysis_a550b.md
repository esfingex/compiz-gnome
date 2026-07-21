# Análisis Matemático y Transformaciones 3D: Desktop Cube (Cubo de Escritorio)

## 1. Geometría Espacial del Cubo de $N$ Caras

El plugin de Cubo 3D proyecta $N$ escritorios virtuales (*workspaces*) sobre las caras laterales de un prisma poligonal regular de $N$ lados.

```
                  Top View (Vista Superior N=4)
                        Face 1
                    ┌────────────┐
                    │            │
         Face 0     │  (0,0,0)   │    Face 2
                    │     •      │
                    └────────────┘
                        Face 3
```

---

## 2. Ángulos y Distancia al Centro (Identity Z-Offset)

### A. Ángulo por Cara ($\theta_{side}$)
Para $N$ escritorios en la grilla horizontal:

$$\theta_{side} = \frac{2\pi}{N}$$

- Para $N=4$ caras (Cubo perfecto): $\theta_{side} = \frac{\pi}{2} = 90^\circ$.
- Para $N=6$ caras (Hexágono): $\theta_{side} = 60^\circ$.

### B. Distancia al Centro ($z_{identity}$)
La distancia desde el centro del prisma a cada cara para que el ancho de la cara encaje exactamente en la pantalla es:

$$z_{identity} = \frac{0.5}{\tan\left(\frac{\theta_{side}}{2}\right)}$$

- Para $N=4$: $z_{identity} = \frac{0.5}{\tan(45^\circ)} = 0.5$.

---

## 3. Matrices de Transformación (Model-View-Projection)

### A. Matriz Modelo de la Cara $i$ ($M_i$)
Para la cara $i \in \{0, 1, \dots, N-1\}$ con una rotación acumulada del usuario $\theta_{user}$:

$$\theta_i = i \cdot \theta_{side} + \theta_{user}$$

$$M_i = R_y(\theta_i) \cdot T\left(0, 0, z_{identity} + z_{extra}\right)$$

Donde la matriz de rotación en el eje $Y$ es:

$$R_y(\theta) = \begin{pmatrix} \cos\theta & 0 & \sin\theta & 0 \\ 0 & 1 & 0 & 0 \\ -\sin\theta & 0 & \cos\theta & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

### B. Matriz de Vista ($V$)
Maneja el zoom del usuario ($z_{zoom}$), la inclinación vertical (pitch $\phi_y$) y la cámara:

$$V = T(0, 0, -z_{offset}) \cdot R_x(\phi_y) \cdot LookAt\left((0,0,0), (0,0,-z_{offset}), (0,1,0)\right)$$

### C. Matriz de Proyección ($P$)
Matriz de perspectiva estándar:

$$P = glm::perspective\left(FOV = 45^\circ, aspect = \frac{width}{height}, z_{near} = 0.1, z_{far} = 100.0\right)$$

### D. Matriz Final MVP
$$\text{MVP}_i = P \cdot V \cdot M_i$$

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor `compiz-gnome`:
1. **GLM (OpenGL Mathematics)** o una librería de álgebra lineal nativa en C++20 calcula las matrices $P, V, M_i$.
2. **Culling por Cintas de Caras (Backface Culling / Two-Pass Winding)**:
   - Se renderizan primero las caras traseras ($GL\_CCW$) para el interior del cubo/skybox.
   - Luego se renderizan las caras frontales ($GL\_CW$) con soporte para transparencia Alpha.
3. **Vulkan Push Constants / Uniform Buffers**:
   - Cada frame actualiza un `UniformBufferObject` con la matriz `MVP` enviada directamente a la pipeline de Vulkan.
