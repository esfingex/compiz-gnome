# Análisis Comparativo: Los 50 Plugins de Infraestructura de Compiz vs. GNOME Shell / Wayland

## 1. El Dilema de Infraestructura: Compiz X11 vs. GNOME Shell Moderno

Compiz (2006-2012) incluía unos ~50 plugins dedicados a la infraestructura de ventanas en X11 (`winrules`, `place`, `put`, `snap`, `decor`, `dbus`, `regex`, `annotate`, `showmouse`, `group`, `shelf`, etc.). 

Existe la duda común de si GNOME Shell moderno en Wayland reemplaza completamente estos plugins o si su implementación es superior o inferior a la de Compiz.

---

## 2. Comparativa Detallada por Categoría

### A. Reglas de Ventana y Posicionamiento (`winrules`, `regex`, `place`, `put`)
- **Compiz (2007)**: Sistema potente basado en expresiones regulares (regex) sobre título, clase X11, rol o tipo de ventana. Permitía fijar posición exacta por píxel, workspace asignado, opacidad inicial, estado de la ventana (siempre visible, al fondo, sin bordes) y pantalla de salida.
- **GNOME Shell (Moderno)**: Es extremadamente opinado y básico. Sutter/Mutter ubica las ventanas de forma centrada o recordada de forma muy limitada. **No existe motor de reglas regex nativo en GNOME Shell**.
- **Veredicto**: **Compiz era inmensamente superior**. Hoy en día los usuarios de GNOME deben instalar extensiones de terceros (como *Auto Move Windows* o *Window Rules*) que son fragmentadas.
- **Oportunidad de Extensión en `compiz-gnome`**: Crear un módulo GJS `CompizWindowRules` que lea expresiones regulares y aplique estados mediante la API de Mutter/Clutter.

### B. Snapping y Tiling de Ventanas (`snap`, `grid`, `maximumize`)
- **Compiz (2007)**: Atracción magnética por bordes de ventanas adyacentes (`snap`), cuadrículas del teclado numérico (`grid`) y algoritmo de expansión de área máxima libre (`maximumize`).
- **GNOME Shell (Moderno)**: Incluye snapping básico a la mitad izquierda/derecha.
- **Veredicto**: **Compiz tenía características únicas** como `maximumize` (expandir ventana hasta tocar las vecinas sin cubrir ninguna) que GNOME no tiene nativamente.
- **Oportunidad de Extensión**: Implementar el algoritmo sweep-line de `maximumize` en GJS.

### C. Personalización Visual y Decoraciones (`decor` / Emerald)
- **Compiz (2007)**: Decorador externo desacoplado (`gtk-window-decorator` / `emerald`) que permitía bordes de cristal transparente, botones personalizados, sombras dinámicas y esquemas de color independientes de la app.
- **GNOME Shell (Moderno)**: Usa Client-Side Decorations (CSD) con GTK4/Libadwaita o Server-Side Decorations (SSD) cerradas en Mutter. La personalización de bordes está fuertemente restringida por diseño.
- **Veredicto**: **Compiz Emerald era inalcanzable en personalización visual**.
- **Oportunidad de Extensión**: En nuestro motor Vulkan, la capa de sombras y bordes brillantes/animados se puede inyectar sobre el contenedor del actor de ventana en Clutter.

### D. Herramientas Interactivas de Pantalla (`annotate`, `showmouse`, `group`, `shelf`)
- **Compiz (2007)**:
  - `annotate`: Dibujar libremente en pantalla sobre cualquier ventana con atajo de teclado + ratón.
  - `showmouse`: Destello / rastro de partículas o anillos al presionar una tecla para localizar el cursor.
  - `group`: Agrupar cualquier conjunto de ventanas en un contenedor con pestañas superiores.
  - `shelf`: Minimizar ventanas como "estantes" flotantes escalados en cualquier esquina del escritorio.
- **GNOME Shell (Moderno)**: **Ninguna de estas características existe en GNOME Shell por defecto.**
- **Veredicto**: **Compiz ofrecía utilidades de productividad adelantadas a su tiempo.**
- **Oportunidad de Extensión**: Implementar `CompizAnnotate` (dibujo en superficie Vulkan/Clutter) y `CompizGroup` (módulos GJS con gran valor de uso).

---

## 3. Resumen del Gap y Hoja de Ruta para Extensiones GJS

| Plugin de Compiz | Disponible en GNOME Shell Nativo? | Superior / Inferior | Mapeo como Extensión `compiz-gnome` |
| :--- | :--- | :--- | :--- |
| `winrules` / `regex` | ❌ No | Compiz superior | Extensión GJS de Reglas Regex |
| `maximumize` | ❌ No | Compiz superior | Módulo GJS Sweep-Line |
| `annotate` | ❌ No | Compiz superior | Overlay Shader en Vulkan/GJS |
| `showmouse` | ❌ No (solo accesibilidad básica) | Compiz superior | Shader de partículas alrededor de cursor |
| `group` (Pestañas) | ❌ No | Compiz superior | Decorador GJS de Pestañas |
| `shelf` | ❌ No | Compiz superior | Extensión de Thumbnails flotantes |
| `emerald` (Decorador) | ❌ Restringido por Adwaita | Compiz superior | Inyección de Bordes/Shaders en Clutter |

---

## 4. Conclusión

Los 50 plugins de infraestructura de Compiz **no han sido superados por GNOME Shell**; al contrario, la simplicidad de GNOME dejó vacíos que hoy se pueden llenar convirtiendo esos antiguos plugins en **extensiones GJS modulares y elegantes** impulsadas por nuestro motor C++20/Vulkan.
