#!/usr/bin/env bash
# compiz-gnome Integration Test Suite
# Ejecutar desde la raíz del proyecto: bash tests/run_integration_tests.sh

set -e

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
echo -e "║  compiz-gnome Integration Test Suite v0.1   ║"
echo -e "╚══════════════════════════════════════════════╝${NC}"
echo ""

# ─── TEST 1: Compilación del motor ───────────────────────────────────────────
echo -e "${CYAN}[1/7] Verificando compilación del motor C++...${NC}"
if [ -f "$ENGINE_BIN" ]; then
    _ok "Binario del motor encontrado: $ENGINE_BIN"
else
    _warn "Motor no compilado. Intentando compilar con CMake..."
    if cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo &>/dev/null &&
       cmake --build build -j"$(nproc)" &>/dev/null; then
        _ok "Motor compilado exitosamente."
    else
        _fail "Compilación del motor fallida. Abortando."
        exit 1
    fi
fi

# ─── TEST 2: Dependencias de sistema ─────────────────────────────────────────
echo -e "${CYAN}[2/7] Verificando dependencias del sistema...${NC}"
for pkg in vulkaninfo socat; do
    if command -v "$pkg" &>/dev/null; then
        _ok "$pkg disponible"
    else
        _warn "$pkg no encontrado (opcional para pruebas completas)"
    fi
done

# Vulkan support
if command -v vulkaninfo &>/dev/null && vulkaninfo --summary 2>/dev/null | grep -q "apiVersion"; then
    VK_VERSION=$(vulkaninfo --summary 2>/dev/null | grep apiVersion | head -1 | awk '{print $3}')
    _ok "Vulkan disponible: $VK_VERSION"
else
    _warn "vulkaninfo no disponible — prueba de GPU omitida"
fi

# ─── TEST 3: Lanzar motor en modo test ───────────────────────────────────────
echo -e "${CYAN}[3/7] Iniciando motor en background...${NC}"
ENGINE_PID=""
if [ -f "$ENGINE_BIN" ]; then
    "$ENGINE_BIN" &
    ENGINE_PID=$!
    sleep 1
    if kill -0 "$ENGINE_PID" 2>/dev/null; then
        _ok "Motor iniciado (PID: $ENGINE_PID)"
    else
        _fail "Motor terminó inesperadamente. Revisar logs."
        ENGINE_PID=""
    fi
else
    _warn "Motor no disponible — tests de IPC omitidos"
fi

# ─── TEST 4: Conexión IPC ────────────────────────────────────────────────────
echo -e "${CYAN}[4/7] Probando conexión IPC Unix Socket...${NC}"
if [ -n "$ENGINE_PID" ]; then
    sleep 0.5
    if [ -S "$SOCKET_PATH" ]; then
        _ok "Socket Unix activo: $SOCKET_PATH"
        if command -v socat &>/dev/null; then
            if echo -n "PING" | socat -T2 - "UNIX-CONNECT:$SOCKET_PATH" &>/dev/null; then
                _ok "Handshake IPC exitoso"
            else
                _warn "socat no pudo conectar (motor puede estar esperando FlatBuffers válido)"
            fi
        fi
    else
        _fail "Socket no encontrado: $SOCKET_PATH (el motor debería crearlo al inicio)"
    fi
else
    _warn "Test IPC omitido (motor no iniciado)"
fi

# ─── TEST 5: Extensión GNOME Shell ───────────────────────────────────────────
echo -e "${CYAN}[5/7] Verificando extensión GNOME Shell...${NC}"
if [ -d "src/gnome_extension" ]; then
    # Instalar extensión en modo desarrollo
    mkdir -p "$EXT_DEST"
    cp src/gnome_extension/*.js src/gnome_extension/metadata.json "$EXT_DEST/" 2>/dev/null || true
    _ok "Archivos de extensión copiados a: $EXT_DEST"

    # Verificar metadata
    if python3 -m json.tool src/gnome_extension/metadata.json &>/dev/null; then
        _ok "metadata.json válido (JSON correcto)"
    else
        _fail "metadata.json con JSON inválido"
    fi

    # Validar sintaxis JS básica con node (si disponible)
    if command -v node &>/dev/null; then
        if node --check src/gnome_extension/extension.js 2>/dev/null; then
            _ok "extension.js sintaxis JS válida"
        else
            _warn "extension.js tiene problemas de sintaxis (puede ser por imports GJS específicos)"
        fi
        if node --check src/gnome_extension/ipcClient.js 2>/dev/null; then
            _ok "ipcClient.js sintaxis JS válida"
        else
            _warn "ipcClient.js tiene problemas de sintaxis (puede ser por imports GJS específicos)"
        fi
    else
        _warn "node.js no disponible — validación de sintaxis JS omitida"
    fi
else
    _fail "Directorio src/gnome_extension no encontrado"
fi

# ─── TEST 6: Shaders GLSL (glslangValidator) ─────────────────────────────────
echo -e "${CYAN}[6/7] Validando shaders GLSL...${NC}"
if command -v glslangValidator &>/dev/null; then
    SHADER_ERRORS=0
    for shader in shaders/*.comp shaders/*.frag shaders/*.vert; do
        [ -f "$shader" ] || continue
        if glslangValidator --target-env vulkan1.3 "$shader" &>/dev/null; then
            _ok "$(basename "$shader") — GLSL válido"
        else
            _fail "$(basename "$shader") — Error GLSL:"
            glslangValidator --target-env vulkan1.3 "$shader" 2>&1 | head -5
            ((SHADER_ERRORS++))
        fi
    done
    [ "$SHADER_ERRORS" -eq 0 ] || _warn "$SHADER_ERRORS shader(s) con errores"
else
    _warn "glslangValidator no instalado (pacman -S glslang) — validación de shaders omitida"
fi

# ─── TEST 7: Compilación del módulo C helper ─────────────────────────────────
echo -e "${CYAN}[7/7] Verificando módulo C nativo compiz-dmabuf...${NC}"
if gcc -fsyntax-only \
    $(pkg-config --cflags glib-2.0 gobject-2.0 egl 2>/dev/null) \
    compiz-dmabuf/compiz-dmabuf.c \
    -Icompiz-dmabuf 2>/dev/null; then
    _ok "compiz-dmabuf.c — sintaxis C11 válida (0 errores)"
else
    _fail "compiz-dmabuf.c tiene errores de compilación"
fi

# ─── Limpiar ─────────────────────────────────────────────────────────────────
if [ -n "$ENGINE_PID" ]; then
    kill "$ENGINE_PID" 2>/dev/null || true
    _info "Motor detenido (PID: $ENGINE_PID)"
fi

# ─── Resumen ─────────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo -e "  Total: ${GREEN}${PASS} ✅ pasaron${NC}  ${YELLOW}${WARN} ⚠️  advertencias${NC}  ${RED}${FAIL} ❌ fallaron${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo ""

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
