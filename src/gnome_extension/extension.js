// GNOME Shell 50+ Extension — Compiz GNOME Effects
// Integra: Wobbly Windows (Física Resorte-Masa parametrizable en tiempo real),
// Water Ripple, IpcClient (Unix Socket), reactividad GSettings y ciclo de vida de Mutter.

import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import GLib from 'gi://GLib';
import { IpcClient } from './ipcClient.js';

// ─────────────────────────────────────────────────────────────────────────────
// WobblyEffect: ClutterEffect de ventanas gelatinosas (Física Resorte-Masa)
// ─────────────────────────────────────────────────────────────────────────────
const WobblyEffect = GObject.registerClass(
{
    GTypeName: 'CompizWobblyEffect',
},
class WobblyEffect extends Clutter.Effect {
    _init(settings) {
        super._init();
        this._settings = settings;
        this._lastPx = 0;
        this._lastPy = 0;
        this._vx = 0;
        this._vy = 0;
        this._dx = 0;
        this._dy = 0;
        this._isDragging = false;
        this._animId = null;
    }

    onGrabBegin() {
        const [px, py] = global.get_pointer();
        this._isDragging = true;
        this._lastPx = px;
        this._lastPy = py;
        this._startLoop();
    }

    onPositionChanged(newX, newY) {
        if (!this._isDragging) {
            this.onGrabBegin();
        }
    }

    onGrabEnd() {
        this._isDragging = false;
    }

    _startLoop() {
        if (this._animId) return;
        this._animId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 16, () => {
            // Leer parámetros físicos configurables dinámicamente desde GSettings
            const springK   = this._settings?.get_double('wobbly-spring-k') ?? 2.0;
            const friction  = this._settings?.get_double('wobbly-friction') ?? 0.82;
            const maxDeform = this._settings?.get_double('wobbly-max-deformation') ?? 60.0;
            const impulse   = this._settings?.get_double('wobbly-impulse') ?? 0.60;

            const k = springK * 0.08;

            // Seguimiento directo en tiempo real del puntero en Wayland
            if (this._isDragging) {
                const [px, py] = global.get_pointer();
                const deltaX = px - this._lastPx;
                const deltaY = py - this._lastPy;
                this._lastPx = px;
                this._lastPy = py;

                this._vx += deltaX * impulse;
                this._vy += deltaY * impulse;
            }

            // Ley de Hooke con amortiguamiento viscoso
            const ax = -k * this._dx - (1.0 - friction) * this._vx;
            const ay = -k * this._dy - (1.0 - friction) * this._vy;

            this._vx += ax;
            this._vy += ay;

            this._dx += this._vx;
            this._dy += this._vy;

            // Clampear deformación máxima según preferencia del usuario
            this._dx = Math.max(-maxDeform, Math.min(maxDeform, this._dx));
            this._dy = Math.max(-maxDeform, Math.min(maxDeform, this._dy));

            // Detener bucle cuando está en reposo y se soltó la ventana
            if (!this._isDragging &&
                Math.abs(this._dx) < 0.1 && Math.abs(this._dy) < 0.1 &&
                Math.abs(this._vx) < 0.1 && Math.abs(this._vy) < 0.1) {
                this._dx = 0;
                this._dy = 0;
                this._vx = 0;
                this._vy = 0;
                this._animId = null;
                this._applyDeformation();
                return GLib.SOURCE_REMOVE;
            }

            this._applyDeformation();
            return GLib.SOURCE_CONTINUE;
        });
    }

    _applyDeformation() {
        const actor = this.get_actor();
        if (!actor) return;

        // Punto de pivote en la parte superior central de la ventana
        actor.set_pivot_point(0.5, 0.0);

        // Ángulo de inclinación según desplazamiento horizontal (máx ±30°)
        const rotZ = Math.max(-30.0, Math.min(30.0, this._dx * 0.20));

        // Deformación elástica vertical y horizontal
        const scaleX = 1.0 - Math.min(0.35, Math.abs(this._dy) * 0.0020);
        const scaleY = 1.0 + Math.min(0.35, Math.abs(this._dy) * 0.0020);

        // Traslación de inercia
        const transX = -this._dx * 0.30;
        const transY = -this._dy * 0.30;

        actor.set_rotation_angle(Clutter.RotateAxis.Z_AXIS, rotZ);
        actor.set_scale(scaleX, scaleY);
        actor.set_translation(transX, transY, 0);

        actor.queue_redraw();
    }

    destroy() {
        if (this._animId) {
            GLib.source_remove(this._animId);
            this._animId = null;
        }
        const actor = this.get_actor();
        if (actor) {
            actor.set_rotation_angle(Clutter.RotateAxis.Z_AXIS, 0);
            actor.set_scale(1.0, 1.0);
            actor.set_translation(0, 0, 0);
        }
        super.run_dispose?.();
    }
});

// ─────────────────────────────────────────────────────────────────────────────
// WaterEffect: ClutterEffect para ondas de agua
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

    triggerImpact() {
        this._active = true;
        this._alpha = 1.0;
        if (this._fadeId) GLib.source_remove(this._fadeId);
        this._fadeId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 16, () => {
            this._alpha -= 0.015;
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
        const actor = this.get_actor();
        if (!actor) return;

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
// CompizExtension: Clase principal de la extensión GNOME Shell 50+
// ─────────────────────────────────────────────────────────────────────────────
export default class CompizExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._ipc = null;
        this._settings = null;
        this._windowActors = new Map(); // windowId -> { actor, metaWindow, waterEffect, wobblyEffect, posId, destroyId }
        this._signalIds = [];
    }

    enable() {
        console.log('[compiz-gnome] Habilitando extensión en GNOME Shell 50+...');

        // 1. Obtener configuración GSettings
        this._settings = this.getSettings('org.gnome.shell.extensions.compiz-gnome');

        // Escuchar cambios reactivos de configuración desde el panel de preferencias
        this._settings.connect('changed::wobbly-enabled', () => this._syncAllEffects());
        this._settings.connect('changed::water-enabled', () => this._syncAllEffects());

        // 2. Conectar con el motor C++/Vulkan por IPC Unix Socket
        this._ipc = new IpcClient(this);
        this._ipc.connect().then(() => {
            console.log('[compiz-gnome] Motor C++ conectado exitosamente.');
            this._registerExistingWindows();
        }).catch(e => {
            console.warn(`[compiz-gnome] Motor C++ no disponible: ${e}. Modo standby activo.`);
        });

        // 3. Hooks de ciclo de vida de ventanas de Mutter
        const display = global.display;
        this._signalIds.push(
            display.connect('window-created', (_display, metaWindow) => {
                this._onWindowCreated(metaWindow);
            })
        );

        // Detectar inicio y fin de operaciones de arrastre (grab-op)
        this._signalIds.push(
            display.connect('grab-op-begin', (_display, metaWindow, op) => {
                if (!metaWindow) return;
                const entry = this._windowActors.get(metaWindow.get_id());
                if (entry && entry.wobblyEffect) {
                    entry.wobblyEffect.onGrabBegin();
                }
            })
        );

        this._signalIds.push(
            display.connect('grab-op-end', (_display, metaWindow, op) => {
                if (!metaWindow) return;
                const entry = this._windowActors.get(metaWindow.get_id());
                if (entry && entry.wobblyEffect) {
                    entry.wobblyEffect.onGrabEnd();
                }
            })
        );

        // 4. Captura global de eventos de puntero (para Water Impact y soltar agarre Wobbly)
        this._signalIds.push(
            global.stage.connect('captured-event', (_stage, event) => {
                return this._onCapturedEvent(event);
            })
        );

        this._registerExistingWindows();
        console.log('[compiz-gnome] Extensión habilitada correctamente.');
    }

    disable() {
        console.log('[compiz-gnome] Deshabilitando extensión...');

        for (const id of this._signalIds) {
            try {
                if (global.display.handler_is_connected?.(id)) global.display.disconnect(id);
                else if (global.stage.handler_is_connected?.(id)) global.stage.disconnect(id);
                else { global.display.disconnect(id); }
            } catch (_) { /* silenciar */ }
        }
        this._signalIds = [];

        for (const [, entry] of this._windowActors) {
            this._cleanupWindowEntry(entry);
        }
        this._windowActors.clear();

        this._ipc?.disconnect();
        this._ipc = null;
        this._settings = null;

        console.log('[compiz-gnome] Extensión deshabilitada limpiamente.');
    }

    onFrameReady(windowId, timelineValue) {
        const entry = this._windowActors.get(windowId);
        if (!entry || !entry.actor) return;
        this._ipc?.sendReleaseFrame(windowId, timelineValue);
        entry.actor.queue_redraw();
    }

    // ─── Métodos Privados ──────────────────────────────────────────────────────

    _registerExistingWindows() {
        const actors = global.get_window_actors();
        for (const actor of actors) {
            const win = actor.get_meta_window();
            if (win) this._onWindowCreated(win);
        }
    }

    _onWindowCreated(metaWindow) {
        GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
            const actor = metaWindow.get_compositor_private();
            if (!actor) return GLib.SOURCE_REMOVE;

            const windowId = metaWindow.get_id();
            if (this._windowActors.has(windowId)) return GLib.SOURCE_REMOVE;

            // Escuchar movimiento continuo de la ventana para Wobbly
            const posId = metaWindow.connect('position-changed', (win) => {
                const entry = this._windowActors.get(windowId);
                if (entry && entry.wobblyEffect) {
                    const rect = win.get_frame_rect();
                    entry.wobblyEffect.onPositionChanged(rect.x, rect.y);
                }
            });

            // Escuchar la destrucción de la ventana
            const destroyId = actor.connect('destroy', () => {
                this._onWindowDestroyed(windowId);
            });

            const entry = {
                actor,
                metaWindow,
                waterEffect: null,
                wobblyEffect: null,
                posId,
                destroyId
            };
            this._windowActors.set(windowId, entry);

            // Registrar en motor C++
            if (this._ipc) {
                this._ipc.sendRegisterWindow(windowId, {
                    width:  actor.width,
                    height: actor.height,
                    x:      actor.x,
                    y:      actor.y,
                });
            }

            this._syncWindowEffects(entry);
            return GLib.SOURCE_REMOVE;
        });
    }

    _syncWindowEffects(entry) {
        if (!entry.actor) return;

        const wobblyEnabled = this._settings?.get_boolean('wobbly-enabled') ?? false;
        const waterEnabled  = this._settings?.get_boolean('water-enabled') ?? false;

        // Wobbly Windows
        if (wobblyEnabled && !entry.wobblyEffect) {
            entry.wobblyEffect = new WobblyEffect(this._settings);
            entry.actor.add_effect_with_name('compiz-wobbly', entry.wobblyEffect);
        } else if (!wobblyEnabled && entry.wobblyEffect) {
            entry.actor.remove_effect(entry.wobblyEffect);
            entry.wobblyEffect.destroy();
            entry.wobblyEffect = null;
        }

        // Water Ripple
        if (waterEnabled && !entry.waterEffect) {
            entry.waterEffect = new WaterEffect(this._ipc, entry.metaWindow.get_id());
            entry.actor.add_effect_with_name('compiz-water', entry.waterEffect);
        } else if (!waterEnabled && entry.waterEffect) {
            entry.actor.remove_effect(entry.waterEffect);
            entry.waterEffect.destroy();
            entry.waterEffect = null;
        }
    }

    _syncAllEffects() {
        for (const [, entry] of this._windowActors) {
            this._syncWindowEffects(entry);
        }
    }

    _onWindowDestroyed(windowId) {
        const entry = this._windowActors.get(windowId);
        if (!entry) return;
        this._cleanupWindowEntry(entry);
        this._windowActors.delete(windowId);
    }

    _cleanupWindowEntry(entry) {
        try {
            if (entry.metaWindow && entry.posId) {
                entry.metaWindow.disconnect(entry.posId);
            }
            if (entry.actor && entry.destroyId) {
                entry.actor.disconnect(entry.destroyId);
            }
            if (entry.wobblyEffect) {
                entry.actor?.remove_effect(entry.wobblyEffect);
                entry.wobblyEffect.destroy();
                entry.wobblyEffect = null;
            }
            if (entry.waterEffect) {
                entry.actor?.remove_effect(entry.waterEffect);
                entry.waterEffect.destroy();
                entry.waterEffect = null;
            }
        } catch (_) { /* silencioso */ }
    }

    _onCapturedEvent(event) {
        const type = event.type();

        if (type === Clutter.EventType.BUTTON_RELEASE || type === Clutter.EventType.TOUCH_END) {
            for (const [, entry] of this._windowActors) {
                entry.wobblyEffect?.onGrabEnd();
            }
            return Clutter.EVENT_PROPAGATE;
        }

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

        if (entry.wobblyEffect) {
            entry.wobblyEffect.onGrabBegin();
        }

        const [ax, ay] = [
            (ex - entry.actor.x) / entry.actor.width,
            (ey - entry.actor.y) / entry.actor.height,
        ];

        if (this._ipc) {
            this._ipc.sendWaterImpact(windowId, ax, ay, 1.0);
        }

        entry.waterEffect?.triggerImpact();
        return Clutter.EVENT_PROPAGATE;
    }
}
