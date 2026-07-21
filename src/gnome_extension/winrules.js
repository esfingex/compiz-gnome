// src/gnome_extension/winrules.js
// Motor de Reglas de Ventana (Regex Rules Engine estilo Compiz)

import Meta from 'gi://Meta';

export class WinRulesEngine {
    constructor() {
        this.rules = this._getDefaultRules();
        this._windowCreatedId = global.display.connect('window-created',
            (_display, metaWindow) => this._onWindowCreated(metaWindow));
    }

    _getDefaultRules() {
        return [
            {
                id: 'terminal-rules',
                enabled: true,
                match: { type: 'class', pattern: '(gnome-terminal|alacritty|kitty)' },
                action: { opacity: 0.95 }
            },
            {
                id: 'devtools-always-on-top',
                enabled: true,
                match: { type: 'title', pattern: '(Developer Tools|Inspector)' },
                action: { alwaysOnTop: true }
            }
        ];
    }

    _onWindowCreated(metaWindow) {
        if (!metaWindow) return;
        const title = metaWindow.get_title() || '';
        const wmClass = metaWindow.get_wm_class() || '';

        for (const rule of this.rules) {
            if (!rule.enabled) continue;
            let match = false;
            if (rule.match.type === 'class' && new RegExp(rule.match.pattern, 'i').test(wmClass)) match = true;
            if (rule.match.type === 'title' && new RegExp(rule.match.pattern, 'i').test(title)) match = true;

            if (match && rule.action) {
                if (rule.action.opacity !== undefined) {
                    metaWindow.set_opacity?.(rule.action.opacity * 255);
                }
                if (rule.action.alwaysOnTop) {
                    metaWindow.make_above?.();
                }
            }
        }
    }

    destroy() {
        if (this._windowCreatedId) {
            global.display.disconnect(this._windowCreatedId);
            this._windowCreatedId = 0;
        }
    }
}
