// src/gnome_extension/prefs.js
// Panel de configuración nativo para Compiz GNOME Effects usando Libadwaita (GTK4/Adw)

import { ExtensionPreferences, gettext as _ } from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';
import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

export default class CompizPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings('org.gnome.shell.extensions.compiz-gnome');

        // ─── TAB 1: EFECTOS VISUALES ──────────────────────────────────────────
        const effectsPage = new Adw.PreferencesPage({
            title: _('Efectos Visuales'),
            icon_name: 'preferences-desktop-effects-symbolic'
        });
        window.add(effectsPage);

        // SECCIÓN: EFECTOS PRINCIPALES
        const generalGroup = new Adw.PreferencesGroup({
            title: _('Efectos Principales'),
            description: _('Activa o desactiva los efectos gráficos Vulkan 1.3')
        });
        effectsPage.add(generalGroup);

        this._addSwitch(generalGroup, settings, 'water-enabled',
            _('Water Ripple (Ondas de Agua)'),
            _('Simula ondas de agua físicas a 120Hz al arrastrar ventanas'));

        this._addSwitch(generalGroup, settings, 'wobbly-enabled',
            _('Wobbly Windows (Ventanas Gelatinosas)'),
            _('Simulación de masa-resorte GPU con Bézier bicúbica'));

        this._addSwitch(generalGroup, settings, 'burn-enabled',
            _('Burn Firepaint (Efecto de Fuego)'),
            _('Quema la superficie de la ventana con 65k partículas en Compute Shader'));

        this._addSwitch(generalGroup, settings, 'shift-enabled',
            _('Shift Cover Flow (Reflexión Planar)'),
            _('Navegación de ventanas 3D con espejo y desenfoque glossy en suelo'));

        this._addSwitch(generalGroup, settings, 'ring-enabled',
            _('Ring Switcher (Carrusel 3D Alt+Tab)'),
            _('Anillo orbital 3D inclinado para selección de ventanas'));

        this._addSwitch(generalGroup, settings, 'animation-enabled',
            _('Animation Suite (Magic Lamp, Curl, Explode)'),
            _('Transiciones de teselación adaptativa al abrir/cerrar ventanas'));

        this._addSwitch(generalGroup, settings, 'blur-enabled',
            _('Dual Kawase Blur (Desenfoque de Fondo)'),
            _('Filtro de desenfoque multicapa O(8N) por GPU'));

        // SECCIÓN: ANNOTATE & PAINTFIRE
        const annotateGroup = new Adw.PreferencesGroup({
            title: _('Annotate & Paintfire'),
            description: _('Dibujo dinámico sobre la pantalla con fuego o tinta')
        });
        effectsPage.add(annotateGroup);

        this._addSwitch(annotateGroup, settings, 'annotate-enabled',
            _('Habilitar Annotate'),
            _('Permite realizar trazos gráficos flotantes sobre el escritorio'));

        this._addSpinRow(annotateGroup, settings, 'annotate-brush-size',
            _('Tamaño del Pincel'), 1, 100, 1);

        // SECCIÓN: SHOWMOUSE & MAGNIFIER
        const showmouseGroup = new Adw.PreferencesGroup({
            title: _('Showmouse & Magnifier (Cursor & Lupa)'),
            description: _('Partículas de seguimiento del cursor y lupa dinámica')
        });
        effectsPage.add(showmouseGroup);

        this._addSwitch(showmouseGroup, settings, 'showmouse-enabled',
            _('Habilitar Showmouse'),
            _('Anillos y chispas de partículas alrededor del puntero'));

        this._addSpinRow(showmouseGroup, settings, 'showmouse-ring-radius',
            _('Radio del Anillo (px)'), 10, 200, 5);

        this._addSwitch(showmouseGroup, settings, 'magnifier-enabled',
            _('Habilitar Magnifier (Lupa)'),
            _('Lupa esférica dinámica con filtrado Catmull-Rom'));

        this._addSpinRow(showmouseGroup, settings, 'magnifier-zoom',
            _('Factor de Zoom (Lupa)'), 1.0, 10.0, 0.5);

        // SECCIÓN: FÍSICA Y PARÁMETROS
        const physicsGroup = new Adw.PreferencesGroup({
            title: _('Parámetros de Física & Calidad'),
            description: _('Ajusta el comportamiento de las simulaciones en tiempo real')
        });
        effectsPage.add(physicsGroup);

        this._addSpinRow(physicsGroup, settings, 'water-speed',
            _('Velocidad del Agua'), 0.1, 2.0, 0.1);

        this._addSpinRow(physicsGroup, settings, 'wobbly-spring-k',
            _('Rigidez de Resortes (Wobbly)'), 0.5, 10.0, 0.5);

        this._addSpinRow(physicsGroup, settings, 'burn-particles',
            _('Cantidad de Partículas de Fuego'), 1000, 100000, 5000);

        this._addSpinRow(physicsGroup, settings, 'blur-radius',
            _('Radio de Desenfoque (Blur)'), 1, 50, 1);

        // ─── TAB 2: CONFIGURACIÓN DE SISTEMA & IPC ───────────────────────────
        const systemPage = new Adw.PreferencesPage({
            title: _('Sistema & IPC'),
            icon_name: 'emblem-system-symbolic'
        });
        window.add(systemPage);

        // SECCIÓN: IPC UNIX SOCKET
        const ipcGroup = new Adw.PreferencesGroup({
            title: _('Conexión Motor C++20 / Vulkan 1.3'),
            description: _('Configuración del canal IPC Unix Domain Socket y tasa física')
        });
        systemPage.add(ipcGroup);

        this._addEntryRow(ipcGroup, settings, 'socket-path',
            _('Ruta del Unix Socket IPC'),
            _('Dejar en blanco para usar $XDG_RUNTIME_DIR/compiz_gnome_engine.sock por defecto'));

        this._addSpinRow(ipcGroup, settings, 'fixed-timestep-hz',
            _('Frecuencia Física (Hz)'), 30, 240, 10);

        // SECCIÓN: ATAJOS DE TECLADO
        const shortcutsGroup = new Adw.PreferencesGroup({
            title: _('Atajos de Teclado del Sistema'),
            description: _('Combos globales para activar efectos 3D y agrupamiento')
        });
        systemPage.add(shortcutsGroup);

        this._addEntryRow(shortcutsGroup, settings, 'ring-shortcut',
            _('Atajo Ring Switcher 3D'),
            _('Por defecto: <Super>w'));

        this._addEntryRow(shortcutsGroup, settings, 'shift-shortcut',
            _('Atajo Shift Cover Flow'),
            _('Por defecto: <Super>e'));

        this._addEntryRow(shortcutsGroup, settings, 'expo-shortcut',
            _('Atajo Expo / Scale'),
            _('Por defecto: <Super>s'));

        this._addEntryRow(shortcutsGroup, settings, 'group-shortcut',
            _('Atajo Grouping Windows'),
            _('Por defecto: <Super>g'));

        // SECCIÓN: ESTADO DEL SERVICIO SYSTEMD Y DIAGNÓSTICO
        const infoGroup = new Adw.PreferencesGroup({
            title: _('Estado del Servicio Systemd & Vulkan Engine'),
            description: _('Servicio de usuario en segundo plano')
        });
        systemPage.add(infoGroup);

        const runtimeDir = GLib.get_user_runtime_dir();
        const activePath = settings.get_string('socket-path') || `${runtimeDir}/compiz_gnome_engine.sock`;

        const statusRow = new Adw.ActionRow({
            title: _('Socket Activo'),
            subtitle: activePath
        });
        infoGroup.add(statusRow);
    }

    _addSwitch(group, settings, key, title, subtitle) {
        const row = new Adw.ActionRow({ title, subtitle });
        const toggle = new Gtk.Switch({
            active: settings.get_boolean(key),
            valign: Gtk.Align.CENTER,
        });
        settings.bind(key, toggle, 'active', Gio.SettingsBindFlags.DEFAULT);
        row.add_suffix(toggle);
        row.activatable_widget = toggle;
        group.add(row);
    }

    _addSpinRow(group, settings, key, title, min, max, step) {
        const row = new Adw.ActionRow({ title });
        const value = settings.get_value(key);
        const typeChar = value.get_type_string();

        let spin = Gtk.SpinButton.new_with_range(min, max, step);
        if (typeChar === 'd') {
            spin.set_digits(2);
        }
        settings.bind(key, spin, 'value', Gio.SettingsBindFlags.DEFAULT);

        spin.valign = Gtk.Align.CENTER;
        row.add_suffix(spin);
        row.activatable_widget = spin;
        group.add(row);
    }

    _addEntryRow(group, settings, key, title, subtitle) {
        const row = new Adw.ActionRow({ title, subtitle });
        const entry = new Gtk.Entry({
            text: settings.get_string(key) || '',
            valign: Gtk.Align.CENTER,
            hexpand: true,
        });
        entry.connect('changed', (e) => {
            settings.set_string(key, e.text || e.get_text?.() || '');
        });
        row.add_suffix(entry);
        row.activatable_widget = entry;
        group.add(row);
    }

}
