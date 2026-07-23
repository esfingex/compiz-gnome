// src/gnome_extension/effects/water.js
// WaterEffect: ClutterEffect para ondas de agua

import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';

export const WaterEffect = GObject.registerClass(
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
            try {
                if (!this.get_actor || !this.get_actor()) {
                    this._fadeId = null;
                    return GLib.SOURCE_REMOVE;
                }
            } catch (_) {
                this._fadeId = null;
                return GLib.SOURCE_REMOVE;
            }

            this._alpha -= 0.015;
            if (this._alpha <= 0.0) {
                this._alpha = 0.0;
                this._active = false;
                this._fadeId = null;
                return GLib.SOURCE_REMOVE;
            }
            try {
                this.queue_repaint();
            } catch (_) {
                this._fadeId = null;
                return GLib.SOURCE_REMOVE;
            }
            return GLib.SOURCE_CONTINUE;
        });
    }

    vfunc_paint(paintNode, paintContext) {
        try {
            const actor = this.get_actor();
            if (!actor) return;

            actor.continue_paint(paintContext);
            if (this._active && this._alpha > 0) {
                this.queue_repaint();
            }
        } catch (_) { /* silencioso */ }
    }

    destroy() {
        if (this._fadeId) {
            GLib.source_remove(this._fadeId);
            this._fadeId = null;
        }
        super.run_dispose?.();
    }
});
