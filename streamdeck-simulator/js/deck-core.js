/**
 * StreamDeck Core Engine
 * Handles button rendering, states, interactions, and visual effects
 */

class StreamDeckCore {
    constructor(containerId, config = {}) {
        this.container = document.getElementById(containerId);
        this.config = {
            rows: config.rows || 3,
            cols: config.cols || 4,
            size: config.size || 'cyd', // 'mini', 'cyd', 'classic', 'xl'
            soundEnabled: config.soundEnabled !== false,
            hapticEnabled: config.hapticEnabled !== false
        };

        this.buttons = [];
        this.currentProfile = null;
        this.audioContext = null;

        this.presets = {
            'mini': { rows: 2, cols: 3, name: 'Stream Deck Mini' },
            'cyd': { rows: 3, cols: 4, name: 'CYD OpsDeck' },
            'classic': { rows: 3, cols: 5, name: 'Stream Deck Classic' },
            'xl': { rows: 4, cols: 8, name: 'Stream Deck XL' }
        };

        this.init();
    }

    init() {
        // Initialize Web Audio for haptic feedback
        if (this.config.soundEnabled) {
            this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        }

        this.applyPreset(this.config.size);
        this.render();
    }

    applyPreset(size) {
        if (this.presets[size]) {
            this.config.rows = this.presets[size].rows;
            this.config.cols = this.presets[size].cols;
            this.config.size = size;
        }
    }

    render() {
        this.container.innerHTML = '';
        this.container.className = `deck-container deck-${this.config.size}`;

        // Create deck frame
        const deckFrame = document.createElement('div');
        deckFrame.className = 'deck-frame';

        // Create button grid
        const buttonGrid = document.createElement('div');
        buttonGrid.className = 'button-grid';
        buttonGrid.style.gridTemplateColumns = `repeat(${this.config.cols}, 1fr)`;
        buttonGrid.style.gridTemplateRows = `repeat(${this.config.rows}, 1fr)`;

        // Create buttons
        this.buttons = [];
        for (let row = 0; row < this.config.rows; row++) {
            for (let col = 0; col < this.config.cols; col++) {
                const index = row * this.config.cols + col;
                const button = this.createButton(index, row, col);
                this.buttons.push(button);
                buttonGrid.appendChild(button.element);
            }
        }

        deckFrame.appendChild(buttonGrid);
        this.container.appendChild(deckFrame);
    }

    createButton(index, row, col) {
        const buttonWrapper = document.createElement('div');
        buttonWrapper.className = 'deck-button';
        buttonWrapper.dataset.index = index;
        buttonWrapper.dataset.row = row;
        buttonWrapper.dataset.col = col;

        const buttonInner = document.createElement('div');
        buttonInner.className = 'button-inner';

        const buttonFace = document.createElement('div');
        buttonFace.className = 'button-face';

        const iconContainer = document.createElement('div');
        iconContainer.className = 'button-icon';

        const labelContainer = document.createElement('div');
        labelContainer.className = 'button-label';

        buttonFace.appendChild(iconContainer);
        buttonFace.appendChild(labelContainer);
        buttonInner.appendChild(buttonFace);
        buttonWrapper.appendChild(buttonInner);

        const button = {
            element: buttonWrapper,
            index,
            row,
            col,
            state: 'idle',
            config: {
                label: '',
                icon: '',
                color: '#1a1a2e',
                action: null
            }
        };

        // Add event listeners
        this.attachButtonEvents(button);

        return button;
    }

    attachButtonEvents(button) {
        const element = button.element;

        // Store bound handlers for cleanup
        button._handlers = {
            mousedown: (e) => {
                e.preventDefault();
                this.onButtonPress(button);
            },
            mouseup: (e) => {
                e.preventDefault();
                this.onButtonRelease(button);
            },
            mouseleave: (e) => {
                if (button.state === 'pressed') {
                    this.onButtonRelease(button);
                }
            },
            touchstart: (e) => {
                e.preventDefault();
                this.onButtonPress(button);
            },
            touchend: (e) => {
                e.preventDefault();
                this.onButtonRelease(button);
            },
            touchcancel: (e) => {
                if (button.state === 'pressed') {
                    this.onButtonRelease(button);
                }
            }
        };

        // Mouse events
        element.addEventListener('mousedown', button._handlers.mousedown);
        element.addEventListener('mouseup', button._handlers.mouseup);
        element.addEventListener('mouseleave', button._handlers.mouseleave);

        // Touch events for mobile
        element.addEventListener('touchstart', button._handlers.touchstart);
        element.addEventListener('touchend', button._handlers.touchend);
        element.addEventListener('touchcancel', button._handlers.touchcancel);
    }

    onButtonPress(button) {
        if (button.state === 'disabled' || button.state === 'running') {
            return;
        }

        this.setButtonState(button, 'pressed');
        this.playHapticFeedback('press');

        // Trigger custom event
        this.dispatchEvent('buttonPress', { button });
    }

    onButtonRelease(button) {
        if (button.state === 'disabled' || button.state === 'running') {
            return;
        }

        this.setButtonState(button, 'idle');
        this.playHapticFeedback('release');

        // Trigger action
        this.dispatchEvent('buttonClick', { button });
    }

    setButtonState(button, state) {
        button.state = state;
        button.element.dataset.state = state;

        // Remove all state classes
        button.element.classList.remove('state-idle', 'state-pressed', 'state-running',
                                       'state-success', 'state-error', 'state-disabled');

        // Add new state class
        button.element.classList.add(`state-${state}`);
    }

    updateButton(index, config) {
        const button = this.buttons[index];
        if (!button) return;

        // Update config
        Object.assign(button.config, config);

        // Update visual elements
        const iconEl = button.element.querySelector('.button-icon');
        const labelEl = button.element.querySelector('.button-label');
        const faceEl = button.element.querySelector('.button-face');

        if (config.label !== undefined) {
            labelEl.textContent = config.label;
        }

        if (config.icon !== undefined) {
            iconEl.innerHTML = config.icon;
        }

        if (config.color !== undefined) {
            faceEl.style.background = config.color;
        }

        if (config.action !== undefined) {
            button.config.action = config.action;
        }
    }

    playHapticFeedback(type) {
        if (!this.config.soundEnabled || !this.audioContext) return;

        const oscillator = this.audioContext.createOscillator();
        const gainNode = this.audioContext.createGain();

        oscillator.connect(gainNode);
        gainNode.connect(this.audioContext.destination);

        if (type === 'press') {
            oscillator.frequency.value = 200;
            gainNode.gain.setValueAtTime(0.1, this.audioContext.currentTime);
            gainNode.gain.exponentialRampToValueAtTime(0.01, this.audioContext.currentTime + 0.05);
        } else if (type === 'release') {
            oscillator.frequency.value = 150;
            gainNode.gain.setValueAtTime(0.08, this.audioContext.currentTime);
            gainNode.gain.exponentialRampToValueAtTime(0.01, this.audioContext.currentTime + 0.03);
        }

        oscillator.start(this.audioContext.currentTime);
        oscillator.stop(this.audioContext.currentTime + 0.1);
    }

    showButtonFeedback(index, type, duration = 1000) {
        const button = this.buttons[index];
        if (!button) return;

        const previousState = button.state;
        this.setButtonState(button, type);

        if (type === 'success' || type === 'error') {
            setTimeout(() => {
                if (button.state === type) {
                    this.setButtonState(button, previousState === 'pressed' ? 'idle' : previousState);
                }
            }, duration);
        }
    }

    setButtonRunning(index, running) {
        const button = this.buttons[index];
        if (!button) return;

        if (running) {
            this.setButtonState(button, 'running');
        } else {
            this.setButtonState(button, 'idle');
        }
    }

    loadProfile(profile) {
        this.currentProfile = profile;

        // Apply profile settings
        if (profile.size && profile.size !== this.config.size) {
            this.applyPreset(profile.size);
            this.render();
        }

        // Load button configurations
        if (profile.buttons) {
            profile.buttons.forEach((btnConfig, index) => {
                if (index < this.buttons.length) {
                    this.updateButton(index, btnConfig);
                }
            });
        }

        this.dispatchEvent('profileLoaded', { profile });
    }

    exportProfile() {
        const profile = {
            name: this.currentProfile?.name || 'Custom Profile',
            size: this.config.size,
            rows: this.config.rows,
            cols: this.config.cols,
            buttons: this.buttons.map(btn => ({
                label: btn.config.label,
                icon: btn.config.icon,
                color: btn.config.color,
                action: btn.config.action
            }))
        };

        return profile;
    }

    resize(size) {
        this.applyPreset(size);
        this.render();

        // Reload current profile if exists
        if (this.currentProfile) {
            this.loadProfile(this.currentProfile);
        }
    }

    clear() {
        this.buttons.forEach((btn, index) => {
            // Remove event listeners to prevent memory leaks
            if (btn._handlers) {
                const element = btn.element;
                element.removeEventListener('mousedown', btn._handlers.mousedown);
                element.removeEventListener('mouseup', btn._handlers.mouseup);
                element.removeEventListener('mouseleave', btn._handlers.mouseleave);
                element.removeEventListener('touchstart', btn._handlers.touchstart);
                element.removeEventListener('touchend', btn._handlers.touchend);
                element.removeEventListener('touchcancel', btn._handlers.touchcancel);
                
                // Clear handler references
                btn._handlers = null;
            }

            this.updateButton(index, {
                label: '',
                icon: '',
                color: '#1a1a2e',
                action: null
            });
        });
    }

    /**
     * Complete cleanup and destruction
     * Call this before removing the deck from the DOM
     */
    destroy() {
        // Clear all buttons
        this.clear();

        // Stop audio context
        if (this.audioContext && this.audioContext.state !== 'closed') {
            try {
                this.audioContext.close();
            } catch (e) {
                console.warn('Error closing audio context:', e);
            }
        }
        this.audioContext = null;

        // Clear buttons array
        this.buttons = [];
        this.currentProfile = null;

        // Clear container
        if (this.container) {
            this.container.innerHTML = '';
        }

        console.log('StreamDeckCore instance destroyed');
    }

    dispatchEvent(eventName, detail) {
        const event = new CustomEvent(eventName, { detail });
        this.container.dispatchEvent(event);
    }

    getButton(index) {
        return this.buttons[index];
    }

    getAllButtons() {
        return this.buttons;
    }

    getButtonCount() {
        return this.buttons.length;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = StreamDeckCore;
}
