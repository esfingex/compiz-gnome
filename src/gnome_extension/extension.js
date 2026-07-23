// GNOME Shell 50+ Extension — Compiz GNOME Effects (Entrypoint Orquestador Limpio)
// Integra: Ciclo de vida Mutter, IPC Client, GSettings reactivos y gestión modular de efectos.

import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import { IpcClient } from './ipcClient.js';
import { WobblyEffect } from './effects/wobbly.js';
import { WaterEffect } from './effects/water.js';
import { BurnEffect } from './effects/burn.js';

export default class CompizExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._ipc = null;
        this._settings = null;
        this._windowActors = new Map(); // windowId -> { actor, metaWindow, waterEffect, wobblyEffect, burnEffect... }
        this._signalIds = [];
    }

    enable() {
        console.log('[compiz-gnome] Habilitando extensión en GNOME Shell 50+...');

        // 1. Obtener configuración GSettings
        this._settings = this.getSettings('org.gnome.shell.extensions.compiz-gnome');

        // Escuchar cambios reactivos de configuración desde el panel de preferencias
        this._settings.connect('changed::wobbly-enabled', () => this._syncAllEffects());
        this._settings.connect('changed::water-enabled', () => this._syncAllEffects());
        this._settings.connect('changed::burn-enabled', () => this._syncAllEffects());

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

            const destroyId = actor.connect('destroy', () => {
                this._onWindowDestroyed(windowId);
            });

            const focusId = metaWindow.connect('focus', () => {
                entry.wobblyEffect?.triggerFocusWobble();
            });

            const sizeId = metaWindow.connect('size-changed', () => {
                entry.wobblyEffect?.triggerMaximizeWobble();
            });

            let lastX = actor.x;
            let lastY = actor.y;

            const posXId = actor.connect('notify::x', () => {
                const curX = actor.x;
                const dx = curX - lastX;
                lastX = curX;
                if (Math.abs(dx) > 0.1 && entry.wobblyEffect) {
                    entry.wobblyEffect.onActorPositionChanged(dx, 0);
                }
            });

            const posYId = actor.connect('notify::y', () => {
                const curY = actor.y;
                const dy = curY - lastY;
                lastY = curY;
                if (Math.abs(dy) > 0.1 && entry.wobblyEffect) {
                    entry.wobblyEffect.onActorPositionChanged(0, dy);
                }
            });

            const entry = {
                actor,
                metaWindow,
                waterEffect: null,
                wobblyEffect: null,
                destroyId,
                focusId,
                sizeId,
                posXId,
                posYId,
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
        const burnEnabled   = this._settings?.get_boolean('burn-enabled') ?? false;

        // Wobbly Windows
        if (wobblyEnabled && !entry.wobblyEffect) {
            entry.wobblyEffect = new WobblyEffect(this._settings, entry.metaWindow);
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

        // Burn Firepaint
        if (burnEnabled && !entry.burnEffect) {
            entry.burnEffect = new BurnEffect(this._settings, entry.metaWindow);
            entry.actor.add_effect_with_name('compiz-burn', entry.burnEffect);
            entry.burnEffect.triggerBurn();
        } else if (!burnEnabled && entry.burnEffect) {
            entry.actor.remove_effect(entry.burnEffect);
            entry.burnEffect.destroy();
            entry.burnEffect = null;
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
            if (entry.metaWindow && entry.focusId) {
                entry.metaWindow.disconnect(entry.focusId);
            }
            if (entry.metaWindow && entry.sizeId) {
                entry.metaWindow.disconnect(entry.sizeId);
            }
            if (entry.actor && entry.destroyId) {
                entry.actor.disconnect(entry.destroyId);
            }
            if (entry.actor && entry.posXId) {
                entry.actor.disconnect(entry.posXId);
            }
            if (entry.actor && entry.posYId) {
                entry.actor.disconnect(entry.posYId);
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
            if (entry.burnEffect) {
                entry.actor?.remove_effect(entry.burnEffect);
                entry.burnEffect.destroy();
                entry.burnEffect = null;
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

        if (type === Clutter.EventType.BUTTON_PRESS || type === Clutter.EventType.TOUCH_BEGIN) {
            const [ex, ey] = event.get_coords();
            const metaWindow = global.display.get_window_at_point?.(ex, ey) || global.display.focus_window;
            if (metaWindow) {
                const entry = this._windowActors.get(metaWindow.get_id());
                if (entry && entry.wobblyEffect) {
                    entry.wobblyEffect.onGrabBegin();
                }

                if (entry) {
                    const [ax, ay] = [
                        (ex - entry.actor.x) / entry.actor.width,
                        (ey - entry.actor.y) / entry.actor.height,
                    ];
                    if (this._ipc) {
                        this._ipc.sendWaterImpact(entry.metaWindow.get_id(), ax, ay, 1.0);
                    }
                    entry.waterEffect?.triggerImpact();
                }
            }
        }

        return Clutter.EVENT_PROPAGATE;
    }
}
