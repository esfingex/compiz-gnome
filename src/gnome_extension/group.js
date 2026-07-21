// src/gnome_extension/group.js
// Plugin Group: Agrupamiento de ventanas en pestañas estilo navegador

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import St from 'gi://St';
import GLib from 'gi://GLib';

export class GroupManager {
    constructor() {
        this.groups = new Map(); // groupId -> { windows, activeWindow, container }
        this.windowToGroup = new Map(); // windowId -> groupId
        this.nextGroupId = 0;

        this._windowCreatedId = global.display.connect('window-created',
            (_display, metaWindow) => this._onWindowCreated(metaWindow));
    }

    createGroup(metaWindow) {
        const groupId = this.nextGroupId++;
        const group = {
            id: groupId,
            windows: [metaWindow],
            activeWindow: metaWindow,
            container: new St.BoxLayout({ vertical: true, style_class: 'compiz-group-container' })
        };

        this.groups.set(groupId, group);
        this.windowToGroup.set(metaWindow.get_id(), groupId);
        return groupId;
    }

    addToGroup(metaWindow, groupId) {
        const group = this.groups.get(groupId);
        if (!group) return false;

        group.windows.push(metaWindow);
        this.windowToGroup.set(metaWindow.get_id(), groupId);
        return true;
    }

    removeFromGroup(metaWindow) {
        const windowId = metaWindow.get_id();
        const groupId = this.windowToGroup.get(windowId);
        if (groupId === undefined) return false;

        const group = this.groups.get(groupId);
        if (!group) return false;

        const idx = group.windows.indexOf(metaWindow);
        if (idx !== -1) {
            group.windows.splice(idx, 1);
            this.windowToGroup.delete(windowId);
            if (group.windows.length === 0) {
                this.groups.delete(groupId);
            }
        }
        return true;
    }

    _onWindowCreated(metaWindow) {
        console.log(`[compiz-group] Ventana registrada para agrupación: ${metaWindow.get_title?.()}`);
    }

    destroy() {
        if (this._windowCreatedId) {
            global.display.disconnect(this._windowCreatedId);
            this._windowCreatedId = 0;
        }
        this.groups.clear();
        this.windowToGroup.clear();
    }
}
