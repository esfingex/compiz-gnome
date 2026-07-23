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

/**
 * Evalúa si una ventana (MetaWindow) coincide con la expresión de regla estilo Compiz CCSM.
 * Ejemplo de regla: "Splash | DropdownMenu | PopupMenu | Tooltip | Notification | Dnd | Normal"
 * @param {Meta.Window} metaWindow 
 * @param {string} ruleString 
 * @returns {boolean}
 */
export function matchCompizRule(metaWindow, ruleString) {
    if (!metaWindow || !ruleString || !ruleString.trim()) return true;

    const windowType = metaWindow.get_window_type?.() ?? Meta.WindowType.NORMAL;
    const wmClass = (metaWindow.get_wm_class?.() || '').toLowerCase();
    const title = (metaWindow.get_title?.() || '').toLowerCase();

    // Mapeo de Meta.WindowType a nombres Compiz
    const typeNames = [];
    switch (windowType) {
        case Meta.WindowType.NORMAL:        typeNames.push('normal'); break;
        case Meta.WindowType.DIALOG:
        case Meta.WindowType.MODAL_DIALOG:  typeNames.push('dialog'); break;
        case Meta.WindowType.MENU:          typeNames.push('menu'); break;
        case Meta.WindowType.TOOLBAR:       typeNames.push('toolbar'); break;
        case Meta.WindowType.UTILITY:       typeNames.push('utility'); break;
        case Meta.WindowType.SPLASHSCREEN:   typeNames.push('splash'); break;
        case Meta.WindowType.DROPDOWN_MENU: typeNames.push('dropdownmenu'); break;
        case Meta.WindowType.POPUP_MENU:    typeNames.push('popupmenu'); break;
        case Meta.WindowType.TOOLTIP:       typeNames.push('tooltip'); break;
        case Meta.WindowType.NOTIFICATION:  typeNames.push('notification'); break;
        case Meta.WindowType.DND:           typeNames.push('dnd'); break;
        default:                            typeNames.push('unknown'); break;
    }

    const tokens = ruleString.split('|').map(t => t.trim().toLowerCase()).filter(Boolean);

    for (const token of tokens) {
        // Coincidencia con tipo de ventana
        if (typeNames.includes(token)) return true;
        // Coincidencia con WM_CLASS o Título
        if (wmClass.includes(token) || title.includes(token)) return true;
        // Coincidencia tipo `class=Foo` o `type=Bar`
        if (token.startsWith('class=') && wmClass.includes(token.replace('class=', ''))) return true;
        if (token.startsWith('title=') && title.includes(token.replace('title=', ''))) return true;
    }

    return false;
}

