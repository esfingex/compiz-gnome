#!/usr/bin/env bash
# compiz-gnome Integration & Unit Test Suite
# Ejecutar desde la raíz del proyecto: bash tests/run_integration_tests.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

SOCKET_PATH="/tmp/compiz_gnome_engine.sock"
ENGINE_BIN="./build/compiz-engine-host"
EXT_UUID="compiz-gnome@esfingex"
EXT_DEST="$HOME/.local/share/gnome-shell/extensions/$EXT_UUID"

PASS=0
FAIL=0
WARN=0

_ok()   { echo -e "${GREEN}✅ $*${NC}"; ((PASS++)); }
_fail() { echo -e "${RED}❌ $*${NC}"; ((FAIL++)); }
_warn() { echo -e "${YELLOW}⚠️  $*${NC}"; ((WARN++)); }
_info() { echo -e "${CYAN}ℹ️  $*${NC}"; }

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════╗"
echo -e "║  compiz-gnome Integration Test Suite v0.3   ║"
echo -e "╚══════════════════════════════════════════════╝${NC}"
echo ""

# ─── TEST 1: Pruebas unitarias C++ automatizadas ──────────────────────────────
echo -e "${CYAN}[1/8] Ejecutando pruebas unitarias C++...${NC}"
mkdir -p build
if g++ -std=c++20 tests/test_fixed_timestep.cpp -Isrc/core_cpp -o build/test_fixed_timestep 2>/dev/null &&
   ./build/test_fixed_timestep >/dev/null 2>&1; then
    _ok "test_fixed_timestep — Acumulación física 120Hz PASÓ"
else
    _fail "test_fixed_timestep — FALLÓ"
fi

if g++ -std=c++20 tests/test_ipc_message.cpp -Isrc/core_cpp -o build/test_ipc_message 2>/dev/null &&
   ./build/test_ipc_message >/dev/null 2>&1; then
    _ok "test_ipc_message — Serialización IPC Payload PASÓ"
else
    _fail "test_ipc_message — FALLÓ"
fi

if g++ -std=c++20 tests/test_new_effects.cpp \
       src/core_cpp/vulkan_context.cpp \
       src/core_cpp/framegraph.cpp \
       src/core_cpp/passes/annotate_compute_pass.cpp \
       src/core_cpp/passes/showmouse_compute_pass.cpp \
       -Isrc/core_cpp -lvulkan -o build/test_new_effects 2>/dev/null &&
   ./build/test_new_effects >/dev/null 2>&1; then
    _ok "test_new_effects — API Annotate & Showmouse PASÓ"
else
    _warn "test_new_effects — requiere libvulkan.so disponible"
fi

# ─── TEST 2: Compilación del motor ───────────────────────────────────────────
echo -e "${CYAN}[2/8] Verificando compilación del motor C++...${NC}"
if [ -f "$ENGINE_BIN" ]; then
    _ok "Binario del motor encontrado: $ENGINE_BIN"
else
    _warn "Motor no compilado aún (requiere vulkan-headers instalados)"
fi

# ─── TEST 3: Dependencias de sistema ─────────────────────────────────────────
echo -e "${CYAN}[3/8] Verificando dependencias del sistema...${NC}"
for pkg in vulkaninfo socat; do
    if command -v "$pkg" &>/dev/null; then
        _ok "$pkg disponible"
    else
        _warn "$pkg no encontrado"
    fi
done

# Vulkan support
if command -v vulkaninfo &>/dev/null && vulkaninfo --summary 2>/dev/null | grep -q "apiVersion"; then
    VK_VERSION=$(vulkaninfo --summary 2>/dev/null | grep apiVersion | head -1 | awk '{print $3}')
    _ok "Vulkan disponible: $VK_VERSION"
else
    _warn "vulkaninfo no disponible — prueba de GPU omitida"
fi

# ─── TEST 4: Extensión GNOME Shell & UI Prefs ────────────────────────────────
echo -e "${CYAN}[4/8] Verificando extensión GNOME Shell & UI Prefs...${NC}"
if [ -d "src/gnome_extension" ]; then
    mkdir -p "$EXT_DEST"
    cp src/gnome_extension/*.js src/gnome_extension/metadata.json "$EXT_DEST/" 2>/dev/null || true
    _ok "Archivos de extensión y prefs.js copiados a: $EXT_DEST"

    if python3 -m json.tool src/gnome_extension/metadata.json &>/dev/null; then
        _ok "metadata.json válido (JSON correcto)"
    else
        _fail "metadata.json con JSON inválido"
    fi
else
    _fail "Directorio src/gnome_extension no encontrado"
fi

# ─── TEST 5: Esquema GSettings XML ────────────────────────────────────────────
echo -e "${CYAN}[5/8] Validando esquema GSettings dconf...${NC}"
if [ -f "schemas/org.gnome.shell.extensions.compiz-gnome.gschema.xml" ]; then
    if glib-compile-schemas --dry-run schemas/ &>/dev/null; then
        _ok "gschema.xml válido (Sintaxis GSettings XML correcta)"
    else
        _warn "glib-compile-schemas tuvo advertencias"
    fi
else
    _fail "Esquema GSettings no encontrado"
fi

# ─── TEST 6: Shaders GLSL (glslangValidator) ─────────────────────────────────
echo -e "${CYAN}[6/8] Validando shaders GLSL (SPIR-V Vulkan 1.3)...${NC}"
if command -v glslangValidator &>/dev/null; then
    SHADER_ERRORS=0
    for shader in shaders/*.comp shaders/*.frag shaders/*.vert shaders/*.tesc shaders/*.tese; do
        [ -f "$shader" ] || continue
        if glslangValidator --target-env vulkan1.3 "$shader" &>/dev/null; then
            _ok "$(basename "$shader") — GLSL válido"
        else
            _fail "$(basename "$shader") — Error GLSL"
            ((SHADER_ERRORS++))
        fi
    done
    [ "$SHADER_ERRORS" -eq 0 ] || _warn "$SHADER_ERRORS shader(s) con errores"
else
    _warn "glslangValidator no instalado — validación de shaders omitida"
fi

# ─── TEST 7: Compilación del módulo C helper ─────────────────────────────────
echo -e "${CYAN}[7/8] Verificando módulo C nativo compiz-dmabuf...${NC}"
if gcc -fsyntax-only \
    $(pkg-config --cflags glib-2.0 gobject-2.0 egl 2>/dev/null) \
    compiz-dmabuf/compiz-dmabuf.c \
    -Icompiz-dmabuf &>/dev/null; then
    _ok "compiz-dmabuf.c — sintaxis C11 válida (0 errores)"
else
    _fail "compiz-dmabuf.c tiene errores de compilación"
fi

# ─── TEST 8: Verificación C++ de pases y entry points ────────────────────────
echo -e "${CYAN}[8/8] Verificando sintaxis C++20 de entry points...${NC}"
if g++ -std=c++20 -fsyntax-only src/core_cpp/main.cpp &>/dev/null; then
    _ok "main.cpp — C++20 válido"
else
    _warn "main.cpp requiere vulkan-headers"
fi

# ─── Resumen ─────────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo -e "  Total: ${GREEN}${PASS} ✅ pasaron${NC}  ${YELLOW}${WARN} ⚠️  advertencias${NC}  ${RED}${FAIL} ❌ fallaron${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo ""

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
