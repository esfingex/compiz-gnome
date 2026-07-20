# Análisis Matemático y Modernización: Plugin Freewins (Transformaciones Libres 3D & Direct Input Raycasting)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/freewins/src/freewins.cpp`
- `compiz-0.9.14.2/plugins/freewins/src/freewins.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `freewins` permite la transformación tridimensional arbitraria de la textura de cualquier ventana (rotación en ejes X/Y/Z, escalado asimétrico y cizallamiento/skew), junto con la re-proyección de eventos de entrada.

### A. Matriz de Transformación Homogénea 4x4
La transformación completa de la ventana se representa mediante el producto acumulativo de matrices afines de 4 dimensiones:

$$\mathbf{M}_{view} = \mathbf{T}(dx, dy, dz) \cdot \mathbf{R}_z(\gamma) \cdot \mathbf{R}_y(\beta) \cdot \mathbf{R}_x(\alpha) \cdot \mathbf{H}(h_x, h_y) \cdot \mathbf{S}(s_x, s_y, s_z)$$

Donde:
1. **Traslación**:
   $$\mathbf{T}(dx, dy, dz) = \begin{pmatrix} 1 & 0 & 0 & dx \\ 0 & 1 & 0 & dy \\ 0 & 0 & 1 & dz \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

2. **Rotación Euler en 3 Ejes**:
   $$\mathbf{R}_x(\alpha) = \begin{pmatrix} 1 & 0 & 0 & 0 \\ 0 & \cos\alpha & -\sin\alpha & 0 \\ 0 & \sin\alpha & \cos\alpha & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}, \quad \mathbf{R}_y(\beta) = \begin{pmatrix} \cos\beta & 0 & \sin\beta & 0 \\ 0 & 1 & 0 & 0 \\ -\sin\beta & 0 & \cos\beta & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

3. **Cizallamiento / Skew Matrix**:
   $$\mathbf{H}(h_x, h_y) = \begin{pmatrix} 1 & \tan(h_x) & 0 & 0 \\ \tan(h_y) & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

4. **Escalado Asimétrico desde el Centro**:
   $$\mathbf{S}(s_x, s_y, s_z) = \begin{pmatrix} s_x & 0 & 0 & 0 \\ 0 & s_y & 0 & 0 \\ 0 & 0 & s_z & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

### B. Inversión de Matriz para Redirección de Input en X11
Para que una ventana inclinada o rotada responda al puntero del ratón en Compiz 0.9, el plugin calculaba la **Matriz Inversa** $\mathbf{M}^{-1}$:

$$\mathbf{P}_{local} = \mathbf{M}^{-1} \cdot \mathbf{P}_{screen}$$

Si $x_{local} \in [0, W_{win}]$ y $y_{local} \in [0, H_{win}]$, se sintetizaba un evento de entrada $XSendEvent$.

---

## 2. Ruta de Modernización para C++20 y Vulkan / Wayland

En X11, la redirección de entrada en ventanas 3D era inestable debido al protocolo monolítico del X Server. En **Wayland (GNOME Shell / C++20 Engine)**, la arquitectura de entrada es limpia y basada en raycasting.

### A. Raycasting de Intersección Ray-Plane en 3D
Cuando el usuario interactúa con una ventana rotada en 3D en Wayland:

1. **Unproject del Cursor desde Coordenadas NDC**:
   Sea el cursor en pantalla $(x_{scr}, y_{scr})$, lo convertimos a Normalized Device Coordinates (NDC) $P_{ndc} \in [-1, 1]^3$:
   $$\mathbf{P}_{near} = \mathbf{M}_{proj}^{-1} \cdot \mathbf{M}_{view}^{-1} \cdot \begin{pmatrix} x_{ndc} \\ y_{ndc} \\ -1 \\ 1 \end{pmatrix}$$
   $$\mathbf{P}_{far} = \mathbf{M}_{proj}^{-1} \cdot \mathbf{M}_{view}^{-1} \cdot \begin{pmatrix} x_{ndc} \\ y_{ndc} \\ 1 \\ 1 \end{pmatrix}$$

2. **Rayo 3D del Puntero**:
   $$\mathbf{O}_{ray} = \mathbf{P}_{near}$$
   $$\mathbf{D}_{ray} = \text{normalize}(\mathbf{P}_{far} - \mathbf{P}_{near})$$

3. **Intersección Plano-Rayo con el Plano de la Ventana**:
   Sea el plano de la ventana definido por su centro $\mathbf{P}_0$ y su vector normal transformado $\vec{N}_{win} = \mathbf{M}_{rot} \cdot (0, 0, 1)^T$:
   $$t = \frac{(\mathbf{P}_0 - \mathbf{O}_{ray}) \cdot \vec{N}_{win}}{\mathbf{D}_{ray} \cdot \vec{N}_{win}}$$
   $$\mathbf{P}_{hit} = \mathbf{O}_{ray} + t \cdot \mathbf{D}_{ray}$$

4. **Transformación a Coordenadas Superficie Wayland**:
   $$\begin{pmatrix} u \\ v \end{pmatrix} = \mathbf{M}_{local\_2d}^{-1} \cdot \mathbf{P}_{hit}$$
   Se transmite el evento $wl\_pointer.motion(u, v)$ directamente al cliente Wayland de la aplicación sin latencia.

### B. Rendimiento Vulkan (`Push Constants` + GLM Matrix Math)
- En C++20, se utiliza `glm::mat4` pre-calculada en CPU.
- La matriz final $MVP = \mathbf{P} \cdot \mathbf{V} \cdot \mathbf{M}$ se pasa al Vertex Shader de Vulkan mediante **Push Constants** (16 floats, 64 bytes), evitando asignaciones de memoria secundaria en GPU.

---

## 3. Registro en CaveMem y Verificación

- Se añade el patrón de Raycasting Inverso Wayland a la memoria de arquitectura de `compiz-gnome`.
