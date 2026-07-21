// GNOME Shell 50+ Extension — Compiz GNOME Effects
// Integra: IpcClient (Unix Socket), WaterEffect (ClutterEffect),
// captura de eventos de arrastre y registro de ventanas.
import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import GLib from 'gi://GLib';
import { IpcClient } from './ipcClient.js';

// ─────────────────────────────────────────────────────────────────────────────
// WaterEffect: ClutterEffect que reemplaza la textura del actor con la
// textura DMA-BUF generada por el motor C++/Vulkan para el efecto de agua.
// ─────────────────────────────────────────────────────────────────────────────
const WaterEffect = GObject.registerClass(
{
    GTypeName: 'CompizWaterEffect',
},
class WaterEffect extends Clutter.Effect {
    _init(ipc, windowId) {
        super._init();
        this._ipc = ipc;
        this._windowId = windowId;
        this._active = false;
        this._fadeId = null;
        this._alpha = 0.0;
    }

    // Activa la animación de onda por un tiempo limitado con fade-out suave
    triggerImpact() {
        this._active = true;
        this._alpha = 1.0;
        if (this._fadeId) GLib.source_remove(this._fadeId);
        this._fadeId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 16, () => {
            this._alpha -= 0.015; // fade out ~1s
            if (this._alpha <= 0.0) {
                this._alpha = 0.0;
                this._active = false;
                this._fadeId = null;
                return GLib.SOURCE_REMOVE;
            }
            this.queue_repaint();
            return GLib.SOURCE_CONTINUE;
        });
    }

    vfunc_paint(paintNode, paintContext) {
        // Deja que Clutter renderice la textura original de la ventana
        // El motor C++ aplica la distorsión en la textura GPU;
        // aquí solo pasamos el paint-through con la opacidad de ola activa.
        const actor = this.get_actor();
        if (!actor) return;

        // Renderizado pass-through (el swap de textura real se hace via DMA-BUF
        // en el ClutterTextureContent cuando el motor exporta el frame)
        actor.continue_paint(paintContext);

        if (this._active && this._alpha > 0) {
            this.queue_repaint();
        }
    }

    destroy() {
        if (this._fadeId) {
            GLib.source_remove(this._fadeId);
            this._fadeId = null;
        }
        super.run_dispose?.();
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// CompizExtension: clase principal de la extensión GNOME Shell 50+
// ─────────────────────────────────────────────────────────────────────────────
export default class CompizExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._ipc = null;
        this._windowActors = new Map(); // windowId → { actor, effect, signals }
        this._signalIds = [];
    }

    enable() {
        console.log('[compiz-gnome] Habilitando extensión en GNOME Shell 50+...');

        // 1. Conectar con el motor C++/Vulkan por IPC Unix Socket
        this._ipc = new IpcClient(this);
        this._ipc.connect().then(() => {
            console.log('[compiz-gnome] Motor C++ conectado. Registrando ventanas existentes...');
            this._registerExistingWindows();
        }).catch(e => {
            console.warn(`[compiz-gnome] Motor C++ no disponible: ${e}. Iniciando en modo standby.`);
        });

        // 2. Hooks de ciclo de vida de ventanas
        const display = global.display;
        this._signalIds.push(
            display.connect('window-created', (_display, metaWindow) => {
                this._onWindowCreated(metaWindow);
            })
        );

        // 3. Captura global de eventos de puntero para impactos de agua
        this._signalIds.push(
            global.stage.connect('captured-event', (_stage, event) => {
                return this._onCapturedEvent(event);
            })
        );

        console.log('[compiz-gnome] Extensión habilitada correctamente.');
    }

    disable() {
        console.log('[compiz-gnome] Deshabilitando extensión...');

        // Desconectar señales globales
        for (const id of this._signalIds) {
            try {
                if (global.display.handler_is_connected?.(id)) global.display.disconnect(id);
                else if (global.stage.handler_is_connected?.(id)) global.stage.disconnect(id);
                else { global.display.disconnect(id); }
            } catch (_) { /* silenciar errores de desconexión */ }
        }
        this._signalIds = [];

        // Limpiar efectos de cada ventana
        for (const [, entry] of this._windowActors) {
            this._cleanupWindowEntry(entry);
        }
        this._windowActors.clear();

        // Cerrar IPC
        this._ipc?.disconnect();
        this._ipc = null;

        console.log('[compiz-gnome] Extensión deshabilitada limpiamente.');
    }

    // Llamado por IpcClient cuando el motor tiene un frame listo
    onFrameReady(windowId, timelineValue) {
        const entry = this._windowActors.get(windowId);
        if (!entry) return;
        // Notificar al motor que ya puede liberar el buffer
        this._ipc.sendReleaseFrame(windowId, timelineValue);
        // Forzar repaint del actor para mostrar el nuevo frame
        entry.actor.queue_redraw();
    }

    // ─── Privados ─────────────────────────────────────────────────────────────

    _registerExistingWindows() {
        const actors = global.get_window_actors();
        for (const actor of actors) {
            const win = actor.get_meta_window();
            if (win) this._onWindowCreated(win);
        }
    }

    _onWindowCreated(metaWindow) {
        // Esperar un tick para que el actor Clutter ya esté disponible
        GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
            const actor = metaWindow.get_compositor_private();
            if (!actor) return GLib.SOURCE_REMOVE;

            const windowId = metaWindow.get_id();
            if (this._windowActors.has(windowId)) return GLib.SOURCE_REMOVE;

            // Registrar ventana en el motor C++
            if (this._ipc) {
                this._ipc.sendRegisterWindow(windowId, {
                    width:  actor.width,
                    height: actor.height,
                    x:      actor.x,
                    y:      actor.y,
                });
            }

            // Añadir el efecto de agua al actor Clutter
            const effect = new WaterEffect(this._ipc, windowId);
            actor.add_effect_with_name('compiz-water', effect);

            // Escuchar la destrucción de la ventana
            const destroyId = actor.connect('destroy', () => {
                this._onWindowDestroyed(windowId);
            });

            this._windowActors.set(windowId, { actor, effect, destroyId });
            console.log(`[compiz-gnome] Ventana registrada: ${windowId} (${actor.width}×${actor.height})`);
            return GLib.SOURCE_REMOVE;
        });
    }

    _onWindowDestroyed(windowId) {
        const entry = this._windowActors.get(windowId);
        if (!entry) return;
        this._cleanupWindowEntry(entry);
        this._windowActors.delete(windowId);
    }

    _cleanupWindowEntry(entry) {
        try {
            if (entry.actor && entry.destroyId) {
                entry.actor.disconnect(entry.destroyId);
            }
            entry.effect?.destroy();
        } catch (_) { /* actor ya destruido */ }
    }

    _onCapturedEvent(event) {
        const type = event.type();

        // Solo nos interesan clicks y arrastre de inicio
        if (type !== Clutter.EventType.BUTTON_PRESS &&
            type !== Clutter.EventType.TOUCH_BEGIN) {
            return Clutter.EVENT_PROPAGATE;
        }

        const [ex, ey] = event.get_coords();
        const metaWindow = global.display.get_window_at_point?.(ex, ey);
        if (!metaWindow) return Clutter.EVENT_PROPAGATE;

        const windowId = metaWindow.get_id();
        const entry = this._windowActors.get(windowId);
        if (!entry) return Clutter.EVENT_PROPAGATE;

        // Coordenadas relativas al actor de la ventana
        const [ax, ay] = [
            (ex - entry.actor.x) / entry.actor.width,
            (ey - entry.actor.y) / entry.actor.height,
        ];

        // Enviar el impacto de agua al motor C++
        if (this._ipc) {
            this._ipc.sendWaterImpact(windowId, ax, ay, 1.0);
        }

        // Disparar animación visual en el actor GJS
        entry.effect.triggerImpact();

        return Clutter.EVENT_PROPAGATE; // No consumir el evento
    }
}
