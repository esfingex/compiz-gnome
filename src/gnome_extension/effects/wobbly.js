// src/gnome_extension/effects/wobbly.js
// WobblyEffect: Clutter.DeformEffect de ventanas gelatinosas (Física Resorte-Masa 2D en Malla)

import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import { matchCompizRule } from '../winrules.js';

export const WobblyEffect = GObject.registerClass(
{
    GTypeName: 'CompizWobblyEffect',
},
class WobblyEffect extends Clutter.DeformEffect {
    _init(settings, metaWindow) {
        super._init();
        this._settings = settings;
        this._metaWindow = metaWindow;

        const res = Math.max(4, Math.min(64, this._settings?.get_int('wobbly-grid-resolution') ?? 8));
        this.x_tiles = res;
        this.y_tiles = res;

        this._nodes = [];
        this._initGrid();

        this._lastPx = 0;
        this._lastPy = 0;
        this._isDragging = false;
        this._shiverEnergy = 0.0;
        this._animId = null;
    }

    _initGrid() {
        const nx = this.x_tiles;
        const ny = this.y_tiles;
        this._nodes = new Array((nx + 1) * (ny + 1));
        for (let j = 0; j <= ny; j++) {
            for (let i = 0; i <= nx; i++) {
                const idx = j * (nx + 1) + i;
                this._nodes[idx] = {
                    dx: 0, dy: 0,
                    vx: 0, vy: 0,
                    rx: i / nx,
                    ry: j / ny
                };
            }
        }
    }

    onGrabBegin() {
        const matchGrab = this._settings?.get_string('wobbly-match-grab') ?? '';
        if (matchGrab && !matchCompizRule(this._metaWindow, matchGrab)) return;

        const [px, py] = global.get_pointer();
        this._isDragging = true;
        this._lastPx = px;
        this._lastPy = py;
        this._startLoop();
    }

    onGrabEnd() {
        this._isDragging = false;
    }

    onActorPositionChanged(dx, dy) {
        if (this._isDragging) return; // Arrastre manual por ratón se procesa en _startLoop

        const matchMove = this._settings?.get_string('wobbly-match-move') ?? '';
        if (matchMove && !matchCompizRule(this._metaWindow, matchMove)) return;

        const impulse = this._settings?.get_double('wobbly-impulse') ?? 0.60;
        const nx = this.x_tiles;
        const ny = this.y_tiles;

        for (let j = 0; j <= ny; j++) {
            for (let i = 0; i <= nx; i++) {
                const idx = j * (nx + 1) + i;
                const weight = 1.0 - (j / ny) * 0.35;
                this._nodes[idx].vx += dx * impulse * weight;
                this._nodes[idx].vy += dy * impulse * weight;
            }
        }
        this._startLoop();
    }

    triggerFocusWobble() {
        const focusEffect = this._settings?.get_string('wobbly-focus-effect') ?? 'none';
        if (focusEffect === 'none') return;
        const matchFocus = this._settings?.get_string('wobbly-match-focus') ?? '';
        if (matchFocus && !matchCompizRule(this._metaWindow, matchFocus)) return;

        if (focusEffect === 'shiver') {
            this._shiverEnergy = 1.0; // Inicia temblor con amortiguamiento
        } else {
            const impulseStrength = 35.0;
            const nx = this.x_tiles;
            const ny = this.y_tiles;
            const cx = nx / 2;
            const cy = ny / 2;

            for (let j = 0; j <= ny; j++) {
                for (let i = 0; i <= nx; i++) {
                    const idx = j * (nx + 1) + i;
                    const dist = Math.hypot(i - cx, j - cy);
                    const factor = Math.max(0, 1.0 - dist / (nx * 0.7));
                    this._nodes[idx].vx += (Math.random() - 0.5) * impulseStrength * factor;
                    this._nodes[idx].vy += (Math.random() - 0.5) * impulseStrength * factor;
                }
            }
        }
        this._startLoop();
    }

    triggerMaximizeWobble() {
        const maxEffect = this._settings?.get_boolean('wobbly-maximize-effect') ?? true;
        if (!maxEffect) return;

        const nx = this.x_tiles;
        const ny = this.y_tiles;
        for (let j = 0; j <= ny; j++) {
            for (let i = 0; i <= nx; i++) {
                const idx = j * (nx + 1) + i;
                const dirX = (i / nx) - 0.5;
                const dirY = (j / ny) - 0.5;
                this._nodes[idx].vx += dirX * 45.0;
                this._nodes[idx].vy += dirY * 45.0;
            }
        }
        this._startLoop();
    }

    _startLoop() {
        if (this._animId) return;
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

            const springK   = this._settings?.get_double('wobbly-spring-k') ?? 0.80;
            const friction  = this._settings?.get_double('wobbly-friction') ?? 0.48;
            const maxDeform = this._settings?.get_double('wobbly-max-deformation') ?? 60.0;
            const impulse   = this._settings?.get_double('wobbly-impulse') ?? 0.60;
            const effectMode = this._settings?.get_string('wobbly-effect-mode') ?? 'wobbly';

            if (effectMode === 'none') {
                this._resetGrid();
                this.invalidate();
                try { this.get_actor()?.queue_redraw(); } catch (_) {}
                this._animId = null;
                return GLib.SOURCE_REMOVE;
            }

            const k = springK * 0.15;
            const damp = 1.0 - Math.min(0.95, friction * 0.8);
            const nx = this.x_tiles;
            const ny = this.y_tiles;

            let deltaX = 0, deltaY = 0;
            if (this._isDragging) {
                const [px, py] = global.get_pointer();
                deltaX = (px - this._lastPx) * impulse;
                deltaY = (py - this._lastPy) * impulse;
                this._lastPx = px;
                this._lastPy = py;
            }

            // Si hay energía de temblor activa (shiver), aplicar e ir decayendo
            if (effectMode === 'shiver' && this._shiverEnergy < 0.05) {
                this._shiverEnergy = 1.0;
            }

            let maxVel = 0;
            let maxDisp = 0;

            for (let j = 0; j <= ny; j++) {
                for (let i = 0; i <= nx; i++) {
                    const idx = j * (nx + 1) + i;
                    const node = this._nodes[idx];

                    if (this._isDragging) {
                        const grabWeight = 1.0 - (node.ry * 0.4);
                        node.vx += deltaX * grabWeight;
                        node.vy += deltaY * grabWeight;
                    }

                    if (this._shiverEnergy > 0.01) {
                        node.vx += (Math.random() - 0.5) * 3.5 * this._shiverEnergy;
                        node.vy += (Math.random() - 0.5) * 3.5 * this._shiverEnergy;
                    }

                    let fx = -k * node.dx;
                    let fy = -k * node.dy;

                    const neighbours = [];
                    if (i > 0) neighbours.push(idx - 1);
                    if (i < nx) neighbours.push(idx + 1);
                    if (j > 0) neighbours.push(idx - (nx + 1));
                    if (j < ny) neighbours.push(idx + (nx + 1));

                    for (const nidx of neighbours) {
                        const nnode = this._nodes[nidx];
                        fx += k * 0.5 * (nnode.dx - node.dx);
                        fy += k * 0.5 * (nnode.dy - node.dy);
                    }

                    node.vx = (node.vx + fx) * damp;
                    node.vy = (node.vy + fy) * damp;

                    node.dx += node.vx;
                    node.dy += node.vy;

                    node.dx = Math.max(-maxDeform, Math.min(maxDeform, node.dx));
                    node.dy = Math.max(-maxDeform, Math.min(maxDeform, node.dy));

                    const vMag = Math.hypot(node.vx, node.vy);
                    const dMag = Math.hypot(node.dx, node.dy);
                    if (vMag > maxVel) maxVel = vMag;
                    if (dMag > maxDisp) maxDisp = dMag;
                }
            }

            // Amortiguar shiverEnergy gradualmente por cada fotograma
            if (this._shiverEnergy > 0) {
                this._shiverEnergy *= 0.90;
                if (this._shiverEnergy < 0.01) this._shiverEnergy = 0.0;
            }

            this.invalidate();
            try { this.get_actor()?.queue_redraw(); } catch (_) {}

            if (!this._isDragging && maxVel < 0.08 && maxDisp < 0.08 && this._shiverEnergy <= 0) {
                this._resetGrid();
                this.invalidate();
                try { this.get_actor()?.queue_redraw(); } catch (_) {}
                this._animId = null;
                return GLib.SOURCE_REMOVE;
            }

            return GLib.SOURCE_CONTINUE;
        });
    }

    _resetGrid() {
        if (!this._nodes) return;
        this._shiverEnergy = 0.0;
        for (const node of this._nodes) {
            node.dx = 0; node.dy = 0;
            node.vx = 0; node.vy = 0;
        }
    }

    vfunc_deform_vertex(width, height, vertex) {
        if (!width || !height || !this._nodes || this._nodes.length === 0) return;

        const u = Math.max(0.0, Math.min(1.0, vertex.x / width));
        const v = Math.max(0.0, Math.min(1.0, vertex.y / height));

        const nx = this.x_tiles;
        const ny = this.y_tiles;

        const gx = u * nx;
        const gy = v * ny;

        const i0 = Math.min(nx - 1, Math.floor(gx));
        const j0 = Math.min(ny - 1, Math.floor(gy));
        const i1 = i0 + 1;
        const j1 = j0 + 1;

        const tx = gx - i0;
        const ty = gy - j0;

        const n00 = this._nodes[j0 * (nx + 1) + i0];
        const n10 = this._nodes[j0 * (nx + 1) + i1];
        const n01 = this._nodes[j1 * (nx + 1) + i0];
        const n11 = this._nodes[j1 * (nx + 1) + i1];

        if (!n00 || !n10 || !n01 || !n11) return;

        const dx = (1 - tx) * (1 - ty) * n00.dx + tx * (1 - ty) * n10.dx + (1 - tx) * ty * n01.dx + tx * ty * n11.dx;
        const dy = (1 - tx) * (1 - ty) * n00.dy + tx * (1 - ty) * n10.dy + (1 - tx) * ty * n01.dy + tx * ty * n11.dy;

        vertex.x += dx;
        vertex.y += dy;
    }

    vfunc_modify_paint_volume(volume) {
        try {
            if (super.vfunc_modify_paint_volume && !super.vfunc_modify_paint_volume(volume))
                return false;

            const maxDeform = this._settings?.get_double('wobbly-max-deformation') ?? 60.0;
            const pad = maxDeform + 40.0;

            const origin = volume.get_origin();
            const size = volume.get_size();

            const newOrigin = new Clutter.Vertex({
                x: origin.x - pad,
                y: origin.y - pad,
                z: origin.z || 0.0
            });
            volume.set_origin(newOrigin);
            volume.set_size(size.width + pad * 2.0, size.height + pad * 2.0, size.depth || 0.0);

            return true;
        } catch (_) {
            return false;
        }
    }

    destroy() {
        if (this._animId) {
            GLib.source_remove(this._animId);
            this._animId = null;
        }
        this._resetGrid();
        super.run_dispose?.();
    }
});
