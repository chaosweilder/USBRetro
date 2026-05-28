/**
 * Dirty tracker — disables a save button until form inputs change.
 * Usage:
 *   const tracker = new DirtyTracker(cardEl, saveBtn);
 *   tracker.snapshot();   // call after load() to capture initial state
 *   tracker.reset();      // call after successful save() to reset
 */
export class DirtyTracker {
    constructor(cardEl, saveBtn) {
        this.cardEl = cardEl;
        this.saveBtn = saveBtn;
        this.snapshotData = null;
        this.dirty = false;

        // Listen for any input/change in the card
        cardEl.addEventListener('input', () => this.check());
        cardEl.addEventListener('change', () => this.check());

        this.updateBtn();
    }

    snapshot() {
        this.snapshotData = this.serialize();
        this.dirty = false;
        this.updateBtn();
    }

    reset() {
        this.snapshot();
    }

    serialize() {
        const inputs = this.cardEl.querySelectorAll('input, select');
        const data = {};
        inputs.forEach(el => {
            if (!el.id) return;
            if (el.type === 'checkbox') data[el.id] = el.checked;
            else data[el.id] = el.value;
        });
        return JSON.stringify(data);
    }

    check() {
        if (this.snapshotData === null) return;
        const current = this.serialize();
        const newDirty = current !== this.snapshotData;
        if (newDirty !== this.dirty) {
            this.dirty = newDirty;
            this.updateBtn();
        }
    }

    updateBtn() {
        if (this.saveBtn) this.saveBtn.disabled = !this.dirty;
    }
}
