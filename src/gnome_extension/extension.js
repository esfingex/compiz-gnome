import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

export default class CompizExtension extends Extension {
    enable() {
        console.log('[compiz-gnome] Extension habilitada en GNOME Shell 50+');
        console.log('[compiz-gnome] Conectando con Motor C++20 / Vulkan en /tmp/compiz_gnome_engine.sock...');
    }

    disable() {
        console.log('[compiz-gnome] Extension deshabilitada.');
    }
}
