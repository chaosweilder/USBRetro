import { DirtyTracker } from './dirty-tracker.js';

/** Bluetooth Device Output Page — BLE mode only */
export class BtOutputCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.hasBle = false;
    }

    render() {
        this.el.innerHTML = `
            <div class="card" id="bleModeCard" style="display:none;">
                <h2>BLE Device Output Mode</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Current Mode</span>
                        <select id="bleModeSelect"><option value="">Loading...</option></select>
                    </div>
                    <div class="buttons" style="margin-top: 12px;">
                        <button id="bleModeSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>`;

        this.el.querySelector('#bleModeSaveBtn').addEventListener('click', () => this.save());
        this.dirty = new DirtyTracker(this.el, this.el.querySelector('#bleModeSaveBtn'));
    }

    async load() {
        const card = this.el.querySelector('#bleModeCard');
        try {
            const result = await this.protocol.listBleModes();
            const select = this.el.querySelector('#bleModeSelect');
            select.innerHTML = '';
            for (const mode of result.modes) {
                const opt = document.createElement('option');
                opt.value = mode.id;
                opt.textContent = mode.name;
                opt.selected = mode.id === result.current;
                select.appendChild(opt);
            }
            card.style.display = '';
            this.hasBle = true;
            this.currentModeId = result.current;
            this.log(`Loaded ${result.modes.length} BLE modes, current: ${result.current}`);
            this.dirty?.snapshot();
        } catch (e) {
            card.style.display = 'none';
        }
    }

    async save() {
        const id = parseInt(this.el.querySelector('#bleModeSelect').value);
        if (id === this.currentModeId) {
            this.log('BLE mode unchanged', 'success');
            return;
        }
        try {
            this.log(`Setting BLE mode to ${id}...`);
            const result = await this.protocol.setBleMode(id);
            this.log(`BLE mode set to ${result.name}`, 'success');
            if (result.reboot) this.log('Device will reboot...', 'warning');
        } catch (e) {
            this.log(`Failed to set BLE mode: ${e.message}`, 'error');
        }
    }

    isAvailable() { return this.hasBle; }
}
