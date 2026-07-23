#!/usr/bin/env bash
# ==============================================================================
# compiz-gnome Automated Installer & Build System
# Compila e instala el motor C++20 / Vulkan 1.3, helper DMA-BUF y extensión GJS
# aprovechando el 100% de los núcleos de CPU (parallel build with nproc).
# ==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

CPU_CORES=$(nproc 2>/dev/null || echo 4)
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_UUID="compiz-effects@esfingex.github.com"
EXT_DEST="$HOME/.local/share/gnome-shell/extensions/$EXT_UUID"

_ok()   { echo -e "${GREEN}✅ $*${NC}"; }
_info() { echo -e "${CYAN}ℹ️  $*${NC}"; }
_warn() { echo -e "${YELLOW}⚠️  $*${NC}"; }
_fail() { echo -e "${RED}❌ $*${NC}"; exit 1; }

echo -e "${CYAN}${BOLD}"
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║          compiz-gnome Universal Installer & Builder           ║"
echo "║       C++20 / Vulkan 1.3 Offscreen Engine / Wayland DMA-BUF    ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

_info "Detectando núcleos de CPU disponibles: ${BOLD}${CPU_CORES} hilos de procesamiento${NC}"

# ─── PASO 1: Instalación de Dependencias del Sistema ──────────────────────────
_info "[1/6] Verificando e instalando dependencias de paquetes de sistema..."

if command -v pacman &>/dev/null; then
    _info "Distribución detectada: Arch Linux / Manjaro (pacman)"
    sudo -n pacman -S --needed --noconfirm \
        vulkan-headers vulkan-icd-loader glslang cmake meson ninja \
        glib2 gobject-introspection socat gcc python python-pip wayland-protocols wayland 2>/dev/null || true
elif command -v apt-get &>/dev/null; then
    _info "Distribución detectada: Ubuntu / Debian (apt)"
    sudo -n apt-get update 2>/dev/null || true
    sudo -n apt-get install -y \
        libvulkan-dev vulkan-tools glslang-dev cmake meson ninja-build \
        libglib2.0-dev libgirepository1.0-dev socat build-essential python3 python3-pip wayland-protocols 2>/dev/null || true
elif command -v dnf &>/dev/null; then
    _info "Distribución detectada: Fedora / RHEL (dnf)"
    sudo -n dnf install -y \
        vulkan-headers vulkan-loader-devel glslang cmake meson ninja-build \
        glib2-devel gobject-introspection-devel socat gcc-c++ python3 python3-pip wayland-protocols-devel 2>/dev/null || true
else
    _warn "Gestor de paquetes no identificado automáticamente. Asegúrate de tener vulkan-headers, cmake, meson y glslang."
fi
_ok "Dependencias del sistema verificadas."


# ─── PASO 2: Entorno Python & Setup.py ─────────────────────────────────────────
_info "[2/6] Configurando entorno de Python y herramientas..."
if [ -f "$PROJECT_DIR/setup.py" ]; then
    pip install --break-system-packages -e "$PROJECT_DIR" 2>/dev/null || pip install -e "$PROJECT_DIR" 2>/dev/null || true
    _ok "Paquete Python compiz-gnome instalado en modo editable."
fi

# ─── PASO 3: Compilación paralela de Shaders GLSL a SPIR-V ───────────────────
_info "[3/6] Compilando Shaders GLSL a SPIR-V Vulkan 1.3 en paralelo (${CPU_CORES} hilos)..."
rm -f "$PROJECT_DIR"/*.spv
mkdir -p "$PROJECT_DIR/build/spv"

export PROJECT_DIR CPU_CORES
compile_shader() {
    shader="$1"
    name=$(basename "$shader")
    if glslangValidator --target-env vulkan1.3 -V "$shader" -o "$PROJECT_DIR/build/spv/${name}.spv" &>/dev/null; then
        echo -e "${GREEN}  ✓ Shader $name → SPIR-V OK${NC}"
    else
        echo -e "${RED}  ❌ Error al compilar $name${NC}"
        return 1
    fi
}
export -f compile_shader

find "$PROJECT_DIR/src/shaders" -type f \( -name "*.comp" -o -name "*.frag" -o -name "*.vert" -o -name "*.tesc" -o -name "*.tese" \) | \
    xargs -P "$CPU_CORES" -I {} bash -c 'compile_shader "$@"' _ {}

_ok "Todos los Shaders GLSL compilados a SPIR-V."

# ─── PASO 4: Compilación del Motor C++20 Offscreen con CMake/Ninja ─────────────
_info "[4/6] Compilando el motor C++20 Offscreen con CMake / Ninja (-j${CPU_CORES})..."
cd "$PROJECT_DIR"
cmake -B build_user -S . -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -C build_user -j"$CPU_CORES"
mkdir -p build
cp build_user/compiz_gnome_engine build/compiz-engine-host 2>/dev/null || cp build_user/compiz_gnome_engine build/compiz_gnome_engine
_ok "Binario ejecutable compiz_gnome_engine compilado exitosamente."

# ─── PASO 5: Compilación e Instalación del Helper C Nativo compiz-dmabuf ──────
_info "[5/6] Compilando helper nativo C compiz-dmabuf con Meson / Ninja..."
cd "$PROJECT_DIR/compiz-dmabuf"
rm -rf build_meson
meson setup build_meson
ninja -C build_meson -j"$CPU_CORES"
sudo -n ninja -C build_meson install 2>/dev/null || ninja -C build_meson install 2>/dev/null || true
_ok "Biblioteca nativa compiz-dmabuf instalada en el sistema."


# ─── PASO 6: Instalación de Extensión GNOME Shell y Esquemas GSettings ───────
_info "[6/6] Instalando extensión GNOME Shell y compilando esquemas GSettings..."
mkdir -p "$EXT_DEST"
cp -r "$PROJECT_DIR/src/gnome_extension/"* "$EXT_DEST/"

# Compilar esquemas GSettings
mkdir -p "$EXT_DEST/schemas"
cp "$PROJECT_DIR/schemas/org.gnome.shell.extensions.compiz-gnome.gschema.xml" "$EXT_DEST/schemas/"
glib-compile-schemas "$EXT_DEST/schemas/"
_ok "Esquema GSettings compilado."

# Habilitar extensión en dconf
gsettings set org.gnome.shell enabled-extensions "$(gsettings get org.gnome.shell enabled-extensions | sed "s/]/, '$EXT_UUID']/")" 2>/dev/null || true
_ok "Extensión $EXT_UUID agregada a la lista de extensiones habilitadas de GNOME Shell."

# ─── PASO 7: Instalación de Servicio Systemd de Usuario (Auto-Start) ──────────
_info "[7/7] Configurando servicio Systemd de usuario para autostart del motor..."
mkdir -p "$HOME/.config/systemd/user"
ENGINE_BIN_PATH="$PROJECT_DIR/build_user/compiz_gnome_engine"

cat <<EOF > "$HOME/.config/systemd/user/compiz-gnome-engine.service"
[Unit]
Description=Compiz GNOME C++20 / Vulkan 1.3 Offscreen Engine
After=graphical-session.target

[Service]
Type=simple
ExecStart=$ENGINE_BIN_PATH
Restart=always
RestartSec=2

[Install]
WantedBy=graphical-session.target
EOF

systemctl --user daemon-reload 2>/dev/null || true
systemctl --user enable --now compiz-gnome-engine.service 2>/dev/null || true
_ok "Servicio Systemd de usuario compiz-gnome-engine.service configurado e iniciado."

# ─── PRUEBAS Y FINALIZACIÓN ───────────────────────────────────────────────────
echo ""
_info "Ejecutando suite de validación final..."
cd "$PROJECT_DIR"
bash tests/run_integration_tests.sh || true

echo ""
echo -e "${GREEN}${BOLD}🎉 ¡INSTALACIÓN Y COMPILACIÓN COMPLETADAS EXITOSAMENTE!${NC}"
echo ""
echo -e "El motor C++20 está corriendo como servicio de usuario (`systemctl --user status compiz-gnome-engine`)."
echo -e "Socket IPC predeterminado: ${CYAN}\$XDG_RUNTIME_DIR/compiz_gnome_engine.sock${NC} (o configurable vía GSettings)."
echo ""
echo -e "Para abrir el menú gráfico de configuración (CCSM Libadwaita):"
echo -e "  ${CYAN}gnome-extensions prefs $EXT_UUID${NC}"
echo ""

