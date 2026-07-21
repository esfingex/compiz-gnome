// src/gnome_extension/prefs.js
// Panel de configuración nativo para Compiz GNOME Effects usando Libadwaita (GTK4/Adw)

import { ExtensionPreferences, gettext as _ } from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';
import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

export default class CompizPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings();

        // ─── PÁGINA PRINCIPAL ───────────────────────────────────────────────────
        const page = new Adw.PreferencesPage({
            title: _('Compiz Effects'),
            icon_name: 'preferences-desktop-effects-symbolic'
        });
        window.add(page);

        // ─── SECCIÓN: EFECTOS PRINCIPALES ──────────────────────────────────────
        const generalGroup = new Adw.PreferencesGroup({
            title: _('Efectos Principales'),
            description: _('Activa o desactiva los efectos gráficos Vulkan 1.3')
        });
        page.add(generalGroup);

        // 1. Water Ripple
        this._addSwitch(generalGroup, settings, 'water-enabled',
            _('Water Ripple (Ondas de Agua)'),
            _('Simula ondas de agua físicas a 120Hz al arrastrar ventanas'));

        // 2. Wobbly Windows
        this._addSwitch(generalGroup, settings, 'wobbly-enabled',
            _('Wobbly Windows (Ventanas Gelatinosas)'),
            _('Simulación de masa-resorte GPU con Bézier bicúbica'));

        // 3. Burn / Firepaint
        this._addSwitch(generalGroup, settings, 'burn-enabled',
            _('Burn Firepaint (Efecto de Fuego)'),
            _('Quema la superficie de la ventana con 65k partículas en Compute Shader'));

        // 4. Shift / Cover Flow
        this._addSwitch(generalGroup, settings, 'shift-enabled',
            _('Shift Cover Flow (Reflexión Planar)'),
            _('Navegación de ventanas 3D con espejo y desenfoque glossy en suelo'));

        // 5. Ring Switcher
        this._addSwitch(generalGroup, settings, 'ring-enabled',
            _('Ring Switcher (Carrusel 3D Alt+Tab)'),
            _('Anillo orbital 3D inclinado para selección de ventanas'));

        // 6. Animation Suite
        this._addSwitch(generalGroup, settings, 'animation-enabled',
            _('Animation Suite (Magic Lamp, Curl, Explode)'),
            _('Transiciones de teselación adaptativa al abrir/cerrar ventanas'));

        // 7. Dual Kawase Blur
        this._addSwitch(generalGroup, settings, 'blur-enabled',
            _('Dual Kawase Blur (Desenfoque de Fondo)'),
            _('Filtro de desenfoque multicapa O(8N) por GPU'));

        // ─── SECCIÓN: FÍSICA Y PARÁMETROS ──────────────────────────────────────
        const physicsGroup = new Adw.PreferencesGroup({
            title: _('Parámetros de Física & Calidad'),
            description: _('Ajusta el comportamiento de las simulaciones en tiempo real')
        });
        page.add(physicsGroup);

        // Slider: Velocidad del Agua
        this._addSpinRow(physicsGroup, settings, 'water-speed',
            _('Velocidad del Agua'), 0.1, 2.0, 0.1);

        // Slider: Rigidez Wobbly
        this._addSpinRow(physicsGroup, settings, 'wobbly-spring-k',
            _('Rigidez de Resortes (Wobbly)'), 0.5, 10.0, 0.5);

        // Slider: Cantidad de Partículas Burn
        this._addSpinRow(physicsGroup, settings, 'burn-particles',
            _('Cantidad de Partículas de Fuego'), 1000, 100000, 5000);

        // Slider: Radio de Blur
        this._addSpinRow(physicsGroup, settings, 'blur-radius',
            _('Radio de Desenfoque (Blur)'), 1, 50, 1);

        // ─── SECCIÓN: INFORMACIÓN DEL MOTOR ─────────────────────────────────────
        const infoGroup = new Adw.PreferencesGroup({
            title: _('Estado del Motor Offscreen C++20'),
            description: _('Conexión Zero-Copy DMA-BUF vía Unix Socket SCM_RIGHTS')
        });
        page.add(infoGroup);

        const statusRow = new Adw.ActionRow({
            title: _('Servidor Offscreen'),
            subtitle: _('Socket: /tmp/compiz_gnome_engine.sock (Vulkan 1.3 / 120Hz)')
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

        let spin;
        if (typeChar === 'd') {
            spin = Gtk.SpinButton.new_with_range(min, max, step);
            spin.set_digits(2);
            settings.bind(key, spin, 'value', Gio.SettingsBindFlags.DEFAULT);
        } else {
            spin = Gtk.SpinButton.new_with_range(min, max, step);
            settings.bind(key, spin, 'value', Gio.SettingsBindFlags.DEFAULT);
        }

        spin.valign = Gtk.Align.CENTER;
        row.add_suffix(spin);
        row.activatable_widget = spin;
        group.add(row);
    }
}
