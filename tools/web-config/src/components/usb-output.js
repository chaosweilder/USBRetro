/** USB Output Page */
import { DirtyTracker } from './dirty-tracker.js';

// Modes that support CDC (web config): HID(0), SInput(1), Switch(5), KBMouse(11), CDC(13)
const CDC_COMPATIBLE_MODES = [0, 1, 5, 11, 13];

export class UsbOutputCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.currentModeId = null;
    }

    render() {
        this.el.innerHTML = `
            <div class="card">
                <h2>USB Device Output Mode</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Current Mode</span>
                        <select id="modeSelect"><option value="">Loading...</option></select>
                    </div>
                    <div id="modeWarning" class="mode-warning" style="display:none;">
                        Web config is not available in this mode. To return to a compatible mode, triple-click the boot button on the device.
                    </div>
                    <div class="buttons" style="margin-top: 12px;">
                        <button id="modeSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>`;

        this.el.querySelector('#modeSelect').addEventListener('change', (e) => {
            this.updateWarning(parseInt(e.target.value));
        });
        this.el.querySelector('#modeSaveBtn').addEventListener('click', () => this.save());
        this.dirty = new DirtyTracker(this.el, this.el.querySelector('#modeSaveBtn'));
    }

    async load() {
        try {
            const result = await this.protocol.listModes();
            const select = this.el.querySelector('#modeSelect');
            select.innerHTML = '';
            for (const mode of result.modes) {
                const opt = document.createElement('option');
                opt.value = mode.id;
                opt.textContent = mode.name;
                opt.selected = mode.id === result.current;
                select.appendChild(opt);
            }
            this.currentModeId = result.current;
            this.log(`Loaded ${result.modes.length} USB modes, current: ${result.current}`);
            this.dirty?.snapshot();
        } catch (e) {
            this.log(`Failed to load USB modes: ${e.message}`, 'error');
        }
    }

    updateWarning(modeId) {
        const warning = this.el.querySelector('#modeWarning');
        if (warning) {
            warning.style.display = CDC_COMPATIBLE_MODES.includes(modeId) ? 'none' : '';
        }
    }

    async save() {
        const id = parseInt(this.el.querySelector('#modeSelect').value);
        if (id === this.currentModeId) {
            this.log('Mode unchanged', 'success');
            return;
        }
        if (!CDC_COMPATIBLE_MODES.includes(id)) {
            if (!confirm('This mode does not support web config. You will lose connection.\n\nTo return to a compatible mode, triple-click the boot button on the device.\n\nContinue?')) {
                await this.load();
                return;
            }
        }
        try {
            this.log(`Setting USB mode to ${id}...`);
            const result = await this.protocol.setMode(id);
            this.log(`USB mode set to ${result.name}`, 'success');
            if (result.reboot) this.log('Device will reboot...', 'warning');
        } catch (e) {
            this.log(`Failed to set USB mode: ${e.message}`, 'error');
        }
    }
}
