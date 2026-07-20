import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

export class IpcClient {
    constructor(extension) {
        this._extension = extension;
        this._socket = null;
        this._path = '/tmp/compiz-gnome-engine.sock';
        this._seq = 0;
    }

    async connect() {
        return new Promise((resolve, reject) => {
            this._client = new Gio.SocketClient();
            const addr = new Gio.UnixSocketAddress({ path: this._path });
            this._client.connect_async(addr, null, (client, res) => {
                try {
                    this._conn = client.connect_finish(res);
                    this._istream = this._conn.get_input_stream();
                    this._ostream = this._conn.get_output_stream();
                    this._readLoop();
                    console.log('[compiz-gnome] IPC Client Conectado exitosamente');
                    resolve();
                } catch (e) {
                    reject(e);
                }
            });
        });
    }

    disconnect() {
        if (this._conn) {
            this._conn.close(null);
            this._conn = null;
        }
    }

    sendMessage(cmdType, payloadObj) {
        if (!this._ostream) return;
        const msg = { cmd: cmdType, payload: payloadObj, seq: this._seq++, ts: Date.now() * 1000 };
        const json = JSON.stringify(msg);
        const data = new TextEncoder().encode(json);
        this._ostream.write_all_async(data, GLib.PRIORITY_DEFAULT, null, (stream, res) => {
            try {
                stream.write_all_finish(res);
            } catch (e) {
                console.error(`[compiz-gnome] Error enviando mensaje IPC: ${e}`);
            }
        });
    }

    sendRegisterWindow(windowId, meta) {
        this.sendMessage(10 /* RegisterWindow */, { window_id: windowId.toString(), ...meta });
    }

    sendWaterImpact(windowId, x, y, strength) {
        this.sendMessage(20 /* WaterImpact */, { window_id: windowId.toString(), x, y, strength });
    }

    sendReleaseFrame(windowId, timelineVal) {
        this.sendMessage(30 /* ReleaseFrame */, { window_id: windowId.toString(), timeline_value: timelineVal.toString() });
    }

    _readLoop() {
        if (!this._istream) return;
        this._istream.read_bytes_async(4096, GLib.PRIORITY_DEFAULT, null, (stream, res) => {
            try {
                const bytes = stream.read_bytes_finish(res);
                if (bytes.get_size() === 0) {
                    this.disconnect();
                    return;
                }
                const jsonStr = new TextDecoder().decode(bytes.toArray());
                try {
                    const msg = JSON.parse(jsonStr);
                    this._handleEngineMessage(msg);
                } catch (e) {
                    console.error(`[compiz-gnome] Error parseando JSON IPC: ${e}`);
                }
                this._readLoop();
            } catch (e) {
                console.error(`[compiz-gnome] Error de lectura IPC: ${e}`);
            }
        });
    }

    _handleEngineMessage(msg) {
        switch (msg.cmd) {
            case 2 /* SyncFrame */:
                this._extension.onFrameReady(msg.payload.window_id, msg.payload.timeline_value);
                break;
            case 3 /* UnregisterWindow */:
                break;
        }
    }
}
