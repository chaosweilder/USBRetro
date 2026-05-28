/** USB + BLE Mode Selection Cards */
export class ModeSelectCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
    }

    render() {
        this.el.innerHTML = `
            <div class="card" id="usbModeCard">
                <h2>USB Output Mode</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Current Mode</span>
                        <select id="modeSelect"><option value="">Loading...</option></select>
                    </div>
                    <p class="hint">Changing mode will reboot the device.</p>
                    <div id="modeWarning" class="mode-warning" style="display:none;">
                        Web config is not available in this mode. To return to a compatible mode, triple-click the boot button on the device.
                    </div>
                </div>
            </div>
            <div class="card" id="bleModeCard" style="display:none;">
                <h2>BLE Output Mode</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Current Mode</span>
                        <select id="bleModeSelect"><option value="">Loading...</option></select>
                    </div>
                    <p class="hint">Changing mode will reboot the device.</p>
                </div>
            </div>`;

        this.el.querySelector('#modeSelect').addEventListener('change', (e) => {
            this.updateModeWarning(parseInt(e.target.value));
            this.setMode(e.target.value);
        });
        this.el.querySelector('#bleModeSelect').addEventListener('change', (e) => this.setBleMode(e.target.value));
    }

    async load() {
        await this.loadModes();
        await this.loadBleModes();
    }

    async loadModes() {
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
            this.log(`Loaded ${result.modes.length} modes, current: ${result.current}`);
        } catch (e) {
            this.log(`Failed to load modes: ${e.message}`, 'error');
        }
    }

    async loadBleModes() {
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
            this.log(`Loaded ${result.modes.length} BLE modes, current: ${result.current}`);
        } catch (e) {
            card.style.display = 'none';
        }
    }

    // Modes that support CDC (web config): HID(0), SInput(1), Switch(5), KBMouse(11), CDC(13)
    static CDC_COMPATIBLE_MODES = [0, 1, 5, 11, 13];

    updateModeWarning(modeId) {
        const warning = this.el.querySelector('#modeWarning');
        if (warning) {
            warning.style.display = ModeSelectCard.CDC_COMPATIBLE_MODES.includes(modeId) ? 'none' : '';
        }
    }

    async setMode(modeId) {
        const id = parseInt(modeId);
        if (!ModeSelectCard.CDC_COMPATIBLE_MODES.includes(id)) {
            if (!confirm('This mode does not support web config. You will lose connection.\n\nTo return to a compatible mode, triple-click the boot button on the device.\n\nContinue?')) {
                // Revert select to current mode
                await this.loadModes();
                return;
            }
        }
        try {
            this.log(`Setting mode to ${modeId}...`);
            const result = await this.protocol.setMode(id);
            this.log(`Mode set to ${result.name}`, 'success');
            if (result.reboot) this.log('Device will reboot...', 'warning');
        } catch (e) {
            this.log(`Failed to set mode: ${e.message}`, 'error');
        }
    }

    async setBleMode(modeId) {
        try {
            this.log(`Setting BLE mode to ${modeId}...`);
            const result = await this.protocol.setBleMode(parseInt(modeId));
            this.log(`BLE mode set to ${result.name}`, 'success');
            if (result.reboot) this.log('Device will reboot...', 'warning');
        } catch (e) {
            this.log(`Failed to set BLE mode: ${e.message}`, 'error');
        }
    }
}
