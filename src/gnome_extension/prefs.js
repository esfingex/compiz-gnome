// src/gnome_extension/prefs.js
// Panel de configuración nativo para Compiz GNOME Effects usando Libadwaita (GTK4/Adw)
// Estructura en Árbol / Acordeón desplegable (Adw.ExpanderRow) con persistencia dconf / GSettings

import { ExtensionPreferences, gettext as _ } from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';
import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

export default class CompizPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings('org.gnome.shell.extensions.compiz-gnome');

        // ─── TAB 1: EFECTOS VISUALES & ÁRBOLES DESPLEGABLES ──────────────────
        const effectsPage = new Adw.PreferencesPage({
            title: _('Efectos Visuales'),
            icon_name: 'preferences-desktop-effects-symbolic'
        });
        window.add(effectsPage);

        const effectsGroup = new Adw.PreferencesGroup({
            title: _('Catálogo de Efectos de Compiz'),
            description: _('Despliega cada efecto para configurar sus parámetros físicos en tiempo real (Persistencia automática en dconf)')
        });
        effectsPage.add(effectsGroup);

        // 1. WOBBLY WINDOWS
        this._addExpanderGroup(effectsGroup, settings, 'wobbly-enabled',
            _('🪟 Ventanas Gelatinosas (Wobbly Windows)'),
            _('Simulación física resorte-masa con malla de deformación 2D'),
            (expander) => {
                this._addSwitchRow(expander, settings, 'wobbly-snap-windows',
                    _('Solapar ventanas'));
                this._addSwitchRow(expander, settings, 'wobbly-snap-inverted',
                    _('Solapamiento inverso'));
                this._addComboRow(expander, settings, 'wobbly-effect-mode',
                    _('Asignar efecto'),
                    [['wobbly', _('Ventanas Gelatinosas')], ['shiver', _('Temblor')], ['none', _('Ninguno')]]);
                this._addSpinRow(expander, settings, 'wobbly-friction',
                    _('Fricción'), 0.1, 0.99, 0.01);
                this._addSpinRow(expander, settings, 'wobbly-spring-k',
                    _('Constante de tensión'), 0.1, 10.0, 0.05);
                this._addSpinRow(expander, settings, 'wobbly-grid-resolution',
                    _('Resolución de la cuadrícula'), 4, 64, 1);
                this._addSpinRow(expander, settings, 'wobbly-min-grid-size',
                    _('Tamaño mínimo de cuadrícula'), 4, 64, 1);
                this._addComboRow(expander, settings, 'wobbly-focus-effect',
                    _('Efecto Enfocar'),
                    [['none', _('Ninguno')], ['wobbly', _('Ventanas Gelatinosas')], ['shiver', _('Temblor')]]);
                this._addEntryRow(expander, settings, 'wobbly-match-assign',
                    _('Asignar ventanas'), _('Ej: Splash | DropdownMenu | PopupMenu | Tooltip | Notification | Dnd | Unknown'));
                this._addEntryRow(expander, settings, 'wobbly-match-focus',
                    _('Enfocar ventanas'), _('Reglas de coincidencia para efecto enfocar'));
                this._addEntryRow(expander, settings, 'wobbly-match-grab',
                    _('Agarrar ventanas'), _('Reglas de coincidencia para agarrar'));
                this._addEntryRow(expander, settings, 'wobbly-match-move',
                    _('Mover ventanas'), _('Ej: Toolbar | Menu | Utility | Dialog | Normal | Unknown'));
                this._addSwitchRow(expander, settings, 'wobbly-maximize-effect',
                    _('Efecto Maximizar'));
            }
        );

        // 2. WATER RIPPLE
        this._addExpanderGroup(effectsGroup, settings, 'water-enabled',
            _('💧 Water Ripple (Ondas de Agua)'),
            _('Simulación de fluidos FDM 2D en Compute Shader a 120Hz'),
            (expander) => {
                this._addSpinRow(expander, settings, 'water-speed',
                    _('Velocidad de Propagación de Ondas'), 0.1, 2.0, 0.1);
            }
        );

        // 3. BURN FIREPAINT
        this._addExpanderGroup(effectsGroup, settings, 'burn-enabled',
            _('🔥 Burn Firepaint (Efecto de Fuego)'),
            _('Quema ventanas con 65k partículas Compute Shader y paletas de color personalizadas'),
            (expander) => {
                this._addSpinRow(expander, settings, 'burn-particles',
                    _('Cantidad de Partículas de Fuego'), 1000, 100000, 5000);
                this._addComboRow(expander, settings, 'burn-color-preset',
                    _('Color / Paleta del Fuego'),
                    [
                        ['fire', _('🔥 Fuego Clásico (Naranja / Rojo)')],
                        ['blue', _('⚡ Fuego de Plasma Azul')],
                        ['green', _('🧪 Fuego Verde Tóxico')],
                        ['purple', _('🔮 Fuego Púrpura Místico')],
                        ['gold', _('🌟 Fuego Dorado Solar')],
                        ['custom', _('🎨 Color Personalizado (HEX)')]
                    ]);
                this._addEntryRow(expander, settings, 'burn-custom-color',
                    _('Color Personalizado (Hex)'), _('Ejemplo: #ff0055 o #00ffcc'));
            }
        );

        // 4. SHIFT COVER FLOW
        this._addExpanderGroup(effectsGroup, settings, 'shift-enabled',
            _('🌀 Shift Cover Flow (Reflexión Planar)'),
            _('Navegación 3D con suelo espejado y desenfoque glossy'),
            (expander) => {
                this._addEntryRow(expander, settings, 'shift-shortcut',
                    _('Atajo de Teclado'), _('Por defecto: <Super>e'));
            }
        );

        // 5. RING SWITCHER
        this._addExpanderGroup(effectsGroup, settings, 'ring-enabled',
            _('🎯 Ring Switcher (Carrusel 3D Alt+Tab)'),
            _('Anillo orbital 3D inclinado para selección de ventanas'),
            (expander) => {
                this._addEntryRow(expander, settings, 'ring-shortcut',
                    _('Atajo de Teclado'), _('Por defecto: <Super>w'));
            }
        );

        // 6. ANIMATION SUITE
        this._addExpanderGroup(effectsGroup, settings, 'animation-enabled',
            _('🎬 Animation Suite (Magic Lamp, Curl, Explode)'),
            _('Transiciones de teselación adaptativa al abrir/cerrar ventanas'),
            (expander) => {}
        );

        // 7. DUAL KAWASE BLUR
        this._addExpanderGroup(effectsGroup, settings, 'blur-enabled',
            _('🧊 Dual Kawase Blur (Desenfoque de Fondo)'),
            _('Filtro de desenfoque multicapa O(8N) por GPU'),
            (expander) => {
                this._addSpinRow(expander, settings, 'blur-radius',
                    _('Radio de Desenfoque (px)'), 1, 50, 1);
            }
        );

        // 8. ANNOTATE & PAINTFIRE
        this._addExpanderGroup(effectsGroup, settings, 'annotate-enabled',
            _('🎨 Annotate & Paintfire (Dibujo en Pantalla)'),
            _('Permite realizar trazos gráficos flotantes sobre el escritorio'),
            (expander) => {
                this._addSpinRow(expander, settings, 'annotate-brush-size',
                    _('Tamaño del Pincel'), 1, 100, 1);
            }
        );

        // 9. SHOWMOUSE
        this._addExpanderGroup(effectsGroup, settings, 'showmouse-enabled',
            _('🐭 Showmouse (Partículas del Cursor)'),
            _('Anillos y chispas de partículas alrededor del puntero'),
            (expander) => {
                this._addSpinRow(expander, settings, 'showmouse-ring-radius',
                    _('Radio del Anillo (px)'), 10, 200, 5);
                this._addSpinRow(expander, settings, 'showmouse-num-particles',
                    _('Cantidad de Partículas'), 64, 2048, 64);
            }
        );

        // 10. MAGNIFIER
        this._addExpanderGroup(effectsGroup, settings, 'magnifier-enabled',
            _('🔍 Magnifier (Lupa Dinámica)'),
            _('Lupa esférica dinámica con filtrado Catmull-Rom'),
            (expander) => {
                this._addSpinRow(expander, settings, 'magnifier-zoom',
                    _('Factor de Zoom (Lupa)'), 1.0, 10.0, 0.5);
                this._addSpinRow(expander, settings, 'magnifier-radius',
                    _('Radio de la Lupa (px)'), 50, 500, 25);
            }
        );


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

        // SECCIÓN: ATAJOS DE TECLADO GLOBAL
        const shortcutsGroup = new Adw.PreferencesGroup({
            title: _('Atajos de Teclado del Sistema'),
            description: _('Combos globales para activar efectos 3D y agrupamiento')
        });
        systemPage.add(shortcutsGroup);

        this._addEntryRow(shortcutsGroup, settings, 'expo-shortcut',
            _('Atajo Expo / Scale'),
            _('Por defecto: <Super>s'));

        this._addEntryRow(shortcutsGroup, settings, 'group-shortcut',
            _('Atajo Grouping Windows'),
            _('Por defecto: <Super>g'));

        // SECCIÓN: ESTADO DEL SERVICIO SYSTEMD
        const infoGroup = new Adw.PreferencesGroup({
            title: _('Estado del Servicio Systemd & Vulkan Engine'),
            description: _('Base de datos dconf / GSettings sincronizada')
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

    _addExpanderGroup(group, settings, enableKey, title, subtitle, setupSubRowsCallback) {
        const rowProps = { title, show_enable_switch: true };
        if (subtitle) rowProps.subtitle = subtitle;
        const expander = new Adw.ExpanderRow(rowProps);

        settings.bind(enableKey, expander, 'enable-expansion', Gio.SettingsBindFlags.DEFAULT);
        setupSubRowsCallback(expander);
        group.add(expander);
    }

    _addSpinRow(container, settings, key, title, min, max, step) {
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

        if (container.add_row) {
            container.add_row(row);
        } else {
            container.add(row);
        }
    }

    _addEntryRow(container, settings, key, title, subtitle) {
        const rowProps = { title };
        if (subtitle) rowProps.subtitle = subtitle;
        const row = new Adw.ActionRow(rowProps);
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

        if (container.add_row) {
            container.add_row(row);
        } else {
            container.add(row);
        }
    }

    _addSwitchRow(container, settings, key, title, subtitle) {
        const rowProps = { title };
        if (subtitle) rowProps.subtitle = subtitle;
        const row = new Adw.ActionRow(rowProps);
        const toggle = new Gtk.Switch({
            active: settings.get_boolean(key),
            valign: Gtk.Align.CENTER,
        });
        settings.bind(key, toggle, 'active', Gio.SettingsBindFlags.DEFAULT);
        row.add_suffix(toggle);
        row.activatable_widget = toggle;

        if (container.add_row) container.add_row(row);
        else container.add(row);
    }

    _addComboRow(container, settings, key, title, options) {
        const row = new Adw.ActionRow({ title });
        const model = new Gtk.StringList();
        options.forEach(([id, label]) => model.append(label));
        const combo = new Gtk.DropDown({
            model,
            valign: Gtk.Align.CENTER,
        });

        const currentVal = settings.get_string(key);
        const idx = options.findIndex(([id]) => id === currentVal);
        if (idx >= 0) combo.set_selected(idx);

        combo.connect('notify::selected', () => {
            const selIdx = combo.get_selected();
            if (selIdx >= 0 && selIdx < options.length) {
                settings.set_string(key, options[selIdx][0]);
            }
        });

        row.add_suffix(combo);
        row.activatable_widget = combo;

        if (container.add_row) container.add_row(row);
        else container.add(row);
    }
}

