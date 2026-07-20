# Análisis Matemático y Algoritmo: Burn Effect (Efecto de Quemar Ventanas)

## 1. Fundamentos del Efecto de Combustión

El efecto de **Burn** (desintegración por fuego) simula una línea de combustión vertical que avanza a lo largo de la superficie de la ventana, combinando dos componentes:
1. **Recorte Progresivo de Geometría (Scissoring / Clipping Line)**.
2. **Sistema de Partículas Estocástico de Fuego (Particle System & Heat Convection)**.

```
       ┌───────────────────────────┐
       │   Ventana Intacta         │
       │                           │
  ═════╪═══════════════════════════╪═════ Linea de Fuego (progress_line)
       │  🔥  ✨  🔥  ✨  🔥  ✨  │ Emisión de Partículas (Heat buoyancy)
       │     (Área Quemada)        │
       └───────────────────────────┘
```

---

## 2. Frente de Combustión y Recorte

Sea $H$ la altura de la ventana y $P(t) \in [0, 1]$ la función de progreso en el tiempo $t$:

$$Y_{fire}(t) = H \cdot P(t)$$

El área visible de la ventana se recorta dinámicamente limitando la representación geométrica a la región $[0, Y_{fire}(t)]$.

---

## 3. Dinámica del Sistema de Partículas

En cada cuadro de renderizado, se emiten $N_p$ partículas a lo largo del frente $Y_{fire}(t)$.

### A. Inicialización de Partícula
Para cada partícula $p$:
- **Posición Inicial**: $x_0 \sim U(0, W)$, $y_0 \sim U(Y_{fire} - \Delta y, Y_{fire} + \Delta y)$
- **Velocidad Inicial**: $\vec{v}_0 = (v_x, v_y)$ donde $v_x \sim U(-10, 10)$, $v_y \sim U(-25, 5)$
- **Aceleración por Convección Térmica (Flotabilidad)**: $\vec{g} = (-1, -3)$ (el calor empuja las llamas hacia arriba y ligeramente a un lado).
- **Vida y Decaimiento**: $\text{life}_0 = 1.0$, $\text{fade} \sim U(0.1, 0.6)$

### B. Ecuaciones de Movimiento e Integración
En cada paso de tiempo $\Delta t$:

$$\vec{v}(t + \Delta t) = \vec{v}(t) + \vec{g} \cdot \Delta t$$

$$\vec{r}(t + \Delta t) = \vec{r}(t) + \vec{v}(t + \Delta t) \cdot \Delta t$$

$$\text{life}(t + \Delta t) = \text{life}(t) - \text{fade} \cdot \Delta t$$

$$\text{radius}(t) = \text{radius}_0 \cdot \text{life}(t)$$

### C. Modelo de Color (Fuego / Ceniza)
El color de la partícula varía dinámicamente con la temperatura/vida:

$$C = (R, G, B, A)$$

Para una paleta de fuego estándar:
- $R \sim U(0.8, 1.0)$ (Rojo intenso)
- $G \sim U(0.3, 0.7)$ (Amarillo/Naranja)
- $B \sim U(0.0, 0.2)$ (Azul/Poco aporte)
- Alpha: $A(t) = \text{life}(t)$

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor `compiz-gnome`:
1. **Compute Shader de Partículas en Vulkan**:
   - Actualiza en paralelo las posiciones, velocidades y vidas de 10,000+ partículas directamente en VRAM usando un **Storage Buffer (SSBO)**.
2. **Renderizado de Partículas con Mezcla Aditiva**:
   - `vkCmdDraw` instancia cada partícula como un Sprite / Quad orientado a la cámara.
   - El pipeline utiliza mezcla aditiva (`VK_BLEND_FACTOR_SRC_ALPHA`, `VK_BLEND_FACTOR_ONE`) para dar el brillo característico del fuego.
