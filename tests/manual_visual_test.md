# Prueba Visual Manual — compiz-gnome MVP (Water Ripple Drag)

## Prerrequisitos del Sistema

| Requisito | Versión Mínima | Verificación |
|:---|:---|:---|
| GNOME Shell | 50+ (Wayland) | `gnome-shell --version` |
| Vulkan | 1.3 | `vulkaninfo --summary \| grep apiVersion` |
| GPU NVIDIA | Driver 545+ | `nvidia-smi` |
| GPU AMD | Mesa 23.1+ (RADV) | `vulkaninfo \| grep driverVersion` |
| GPU Intel | Mesa 23.1+ (ANV) | Arquitectura Xe o superior |
| GCC / Clang | GCC 13+ / Clang 16+ | `gcc --version` |

---

## Paso 1: Instalar Dependencias

```bash
# Arch Linux
sudo pacman -S vulkan-headers vulkan-icd-loader glslang egl-wayland \
               meson ninja glib2 gobject-introspection socat

# Verificar Vulkan
vulkaninfo --summary
```

---

## Paso 2: Compilar el Motor C++20

```bash
cd /home/esfingex/workspace/compiz-gnome

# Opción A: CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

# Opción B: Compilación directa (desarrollo)
g++ -std=c++20 -O2 \
    src/core_cpp/main.cpp \
    src/core_cpp/vulkan_context.cpp \
    src/core_cpp/ipc_server.cpp \
    src/core_cpp/passes/water_compute_pass.cpp \
    src/core_cpp/framegraph.cpp \
    -Isrc/core_cpp \
    $(pkg-config --cflags --libs vulkan glib-2.0) \
    -lpthread \
    -o build/compiz-engine-host
```

---

## Paso 3: Compilar los Shaders GLSL a SPIR-V

```bash
# Compilar todos los shaders del directorio
mkdir -p build/spv

for shader in shaders/*.comp shaders/*.frag shaders/*.vert; do
    name=$(basename "$shader")
    glslangValidator --target-env vulkan1.3 -V "$shader" -o "build/spv/${name}.spv"
    echo "✅ $name → build/spv/${name}.spv"
done
```

---

## Paso 4: Compilar el Helper C nativo

```bash
cd compiz-dmabuf
meson setup build
ninja -C build

# Instalar typelib para GObject Introspection (requiere sudo)
sudo ninja -C build install
```

---

## Paso 5: Instalar la Extensión GNOME Shell

```bash
EXT_UUID="compiz-gnome@esfingex"
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/$EXT_UUID"
mkdir -p "$EXT_DIR"

cp src/gnome_extension/extension.js "$EXT_DIR/"
cp src/gnome_extension/ipcClient.js "$EXT_DIR/"
cp src/gnome_extension/metadata.json "$EXT_DIR/"

# Habilitar extensión (sin reiniciar GNOME Shell en Wayland, usar login)
gnome-extensions enable "$EXT_UUID" 2>/dev/null || \
    echo "Reinicia la sesión GNOME (logout+login) y ejecuta: gnome-extensions enable $EXT_UUID"
```

---

## Paso 6: Ejecutar el Motor y Probar

### Terminal 1: Motor C++
```bash
./build/compiz-engine-host
# Salida esperada:
# [+] Inicializando subsistema Vulkan 1.3 RAII & VMA...
# [+] Escuchando IPC Socket en /tmp/compiz_gnome_engine.sock...
```

### Terminal 2: Activar extensión y probar
```bash
# Verificar que el socket está activo
ls -la /tmp/compiz_gnome_engine.sock

# Simular un impacto de agua manualmente (para testing sin UI)
echo '{"cmd":20,"payload":{"window_id":"12345","x":0.5,"y":0.5,"strength":1.0},"seq":1,"ts":0}' | \
    socat - UNIX-CONNECT:/tmp/compiz_gnome_engine.sock

# Ver logs de GNOME Shell en tiempo real
journalctl -f -o cat | grep compiz-gnome
```

---

## Paso 7: Prueba Visual End-to-End

1. **Reiniciar sesión GNOME** (Wayland) para que la extensión se cargue.
2. **Abrir cualquier ventana** (ej: Nautilus, Terminal).
3. **Hacer click en la ventana y arrastrarla** rápidamente.
4. **Resultado esperado**: Las ondas de agua se propagan desde el punto de click sobre la superficie de la ventana mientras la arrastras.

### Indicadores de éxito:
- [ ] Las ondas son visibles y suaves (sin tearing).
- [ ] La simulación corre a 120Hz (verificar con `watch -n0.1 "cat /tmp/compiz_fps.txt"`).
- [ ] Al soltar la ventana, las ondas se disipan gradualmente.
- [ ] No hay caídas de framerate en el escritorio completo.

---

## Diagnóstico de Problemas Comunes

| Síntoma | Causa probable | Solución |
|:---|:---|:---|
| `Socket no encontrado` | Motor no iniciado | Ejecutar `./build/compiz-engine-host` primero |
| `VkResult: -7 (VK_ERROR_FEATURE_NOT_PRESENT)` | GPU sin Vulkan 1.3 | Actualizar drivers o usar GPU compatible |
| Extensión no carga | GNOME < 50 | Actualizar GNOME Shell |
| Sin efecto visual | `compiz-dmabuf` no instalado | `sudo ninja -C compiz-dmabuf/build install` |
| Ondas a 60Hz | CPU saturado | Revisar que el motor no tiene `sleep_for(16ms)` en el bucle principal |
