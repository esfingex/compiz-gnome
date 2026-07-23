// src/gnome_extension/effects/burn.js
// BurnEffect: ClutterEffect para animación de fuego de Compiz (Burn Firepaint)

import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import { matchCompizRule } from '../winrules.js';

export const BurnEffect = GObject.registerClass(
{
    GTypeName: 'CompizBurnEffect',
},
class BurnEffect extends Clutter.Effect {
    _init(settings, metaWindow) {
        super._init();
        this._settings = settings;
        this._metaWindow = metaWindow;
        this._progress = 0.0; // 0.0 = intacta, 1.0 = totalmente consumida por fuego
        this._isBurning = false;
        this._animId = null;
        this._particles = [];
        this._maxParticles = this._settings?.get_int('burn-particles') ?? 50000;
        this._initParticles();
    }

    _initParticles() {
        const count = Math.min(600, Math.max(80, Math.floor(this._maxParticles / 100)));
        this._particles = new Array(count);
        for (let i = 0; i < count; i++) {
            this._particles[i] = {
                x: 0, y: 0,
                vx: 0, vy: 0,
                life: 0, maxLife: 1.0,
                size: 4 + Math.random() * 10,
                color: [1.0, 0.5, 0.1],
            };
        }
    }

    _getFireColor() {
        const preset = this._settings?.get_string('burn-color-preset') ?? 'fire';
        switch (preset) {
            case 'blue':   return [0.1 + Math.random() * 0.2, 0.6 + Math.random() * 0.4, 1.0];
            case 'green':  return [0.1, 0.8 + Math.random() * 0.2, 0.2 + Math.random() * 0.3];
            case 'purple': return [0.7 + Math.random() * 0.3, 0.1, 0.9 + Math.random() * 0.1];
            case 'gold':   return [1.0, 0.8 + Math.random() * 0.2, 0.1 + Math.random() * 0.2];
            case 'custom': {
                const hex = (this._settings?.get_string('burn-custom-color') ?? '#ff6600').replace('#', '');
                const r = parseInt(hex.substring(0, 2) || 'ff', 16) / 255.0;
                const g = parseInt(hex.substring(2, 4) || '66', 16) / 255.0;
                const b = parseInt(hex.substring(4, 6) || '00', 16) / 255.0;
                return [r, g, b];
            }
            case 'fire':
            default:       return [1.0, 0.4 + Math.random() * 0.4, 0.05];
        }
    }

    triggerBurn(onCompleteCallback) {
        const matchBurn = this._settings?.get_string('wobbly-match-assign') ?? '';
        if (matchBurn && !matchCompizRule(this._metaWindow, matchBurn)) return;

        this._progress = 0.0;
        this._isBurning = true;
        this._onComplete = onCompleteCallback;

        if (this._animId) GLib.source_remove(this._animId);

        this._animId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 16, () => {
            try {
                const actor = this.get_actor?.();
                if (!actor) {
                    this._animId = null;
                    return GLib.SOURCE_REMOVE;
                }
            } catch (_) {
                this._animId = null;
                return GLib.SOURCE_REMOVE;
            }

            this._progress += 0.02; // Avanza el frente de fuego

            const w = this.get_actor()?.width || 800;
            const h = this.get_actor()?.height || 600;
            const fireLineY = h * (1.0 - this._progress);

            for (const p of this._particles) {
                if (p.life <= 0) {
                    p.x = Math.random() * w;
                    p.y = fireLineY + (Math.random() - 0.5) * 30;
                    p.vx = (Math.random() - 0.5) * 50;
                    p.vy = -(40 + Math.random() * 80);
                    p.life = 0.3 + Math.random() * 0.5;
                    p.maxLife = p.life;
                    p.size = 8 + Math.random() * 14;
                    p.color = this._getFireColor();
                } else {
                    p.x += p.vx * 0.016;
                    p.y += p.vy * 0.016;
                    p.life -= 0.016;
                    p.size *= 0.95;
                }
            }

            try { this.get_actor()?.queue_redraw(); } catch (_) {}

            if (this._progress >= 1.0) {
                this._isBurning = false;
                this._animId = null;
                if (this._onComplete) this._onComplete();
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

            if (this._isBurning && this._progress > 0) {
                this.queue_repaint();
            }
        } catch (_) { /* silencioso */ }
    }

    destroy() {
        if (this._animId) {
            GLib.source_remove(this._animId);
            this._animId = null;
        }
        super.run_dispose?.();
    }
});
