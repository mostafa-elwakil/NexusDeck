/**
 * NexusDeck Studio UI Editor
 * Visual editor for customizing buttons, creating actions, and managing profiles
 */

class StudioUI {
    constructor(deckCore, actionsEngine, profilesManager, liveKeysManager) {
        this.deck = deckCore;
        this.actions = actionsEngine;
        this.profiles = profilesManager;
        this.liveKeys = liveKeysManager;
        this.selectedButton = null;
        this.editorMode = 'player'; // 'player' or 'studio'
        this.theme = localStorage.getItem('streamdeck_theme') || 'dark';
        this.deviceStatusTimer = null;
        this.espSettings = null;
        this.obsHandlers = { operationChange: null, testClick: null };

        this.iconPresets = [
            '💻', '⚡', '🐳', '🐙', '🌐', '📡', '🎥', '🎮', '💬', '🖥️',
            '📷', '⏺️', '⏹️', '⏯️', '▶️', '⏱️', '⏲️', '🕐', '🔥', '💾',
            '🗑️', '📋', '📝', '📧', '📅', '🎵', '📺', '📸', '🔢', '⚙️'
        ];

        this.presetFiles = [
            { file: 'obs_profile.json', label: 'OBS Studio' },
            { file: 'devops_profile.json', label: 'DevOps' },
            { file: 'media_profile.json', label: 'Media' },
            { file: 'productivity_profile.json', label: 'Productivity' }
        ];

        this.init();
    }

    init() {
        this.createStudioUI();
        this.applyTheme(this.theme);
        this.attachEventListeners();
        this.updateServerStatus(this.actions.serverAvailable);
    }

    createStudioUI() {
        const header = this.createToolbar();
        document.body.prepend(header);

        const footer = document.createElement('footer');
        footer.className = 'app-footer';
        footer.innerHTML = `
            <p class="footer-hint"><kbd>Ctrl</kbd>+<kbd>E</kbd> studio · <kbd>Ctrl</kbd>+<kbd>S</kbd> save · click a key to run</p>
            <div class="footer-status-group">
                <div id="esp32-status" class="status-pill is-offline" role="status" title="ESP32 hardware sync">ESP32 offline</div>
                <div id="server-status" class="status-pill is-offline" role="status">Companion offline</div>
            </div>
        `;
        document.body.appendChild(footer);

        const studioContainer = document.createElement('div');
        studioContainer.id = 'studio-container';
        studioContainer.className = 'studio-hidden';
        studioContainer.appendChild(this.createInspector());
        studioContainer.appendChild(this.createProfilesPanel());
        studioContainer.appendChild(this.createHistoryPanel());
        document.body.appendChild(studioContainer);
        document.body.appendChild(this.createSystemPanel());
    }

    createToolbar() {
        const toolbar = document.createElement('header');
        toolbar.className = 'studio-toolbar app-header';
        toolbar.innerHTML = `
            <div class="toolbar-section brand-section">
                <div class="app-brand">
                    <span class="brand-mark">OD</span>
                    <div class="brand-copy">
                        <strong>NexusDeck</strong>
                        <span>Studio</span>
                    </div>
                </div>
            </div>
            <div class="toolbar-section">
                <button id="btn-toggle-mode" class="toolbar-btn" title="Toggle Studio Mode">
                    <span class="icon">✎</span>
                    <span class="label">Studio</span>
                </button>
                <button id="btn-save-profile" class="toolbar-btn" title="Save Current Profile">
                    <span class="icon">▾</span>
                    <span class="label">Save</span>
                </button>
                <button id="btn-clear-deck" class="toolbar-btn" title="Clear All Buttons">
                    <span class="icon">✕</span>
                    <span class="label">Clear</span>
                </button>
                <button id="btn-toggle-history" class="toolbar-btn" title="Show/hide activity">
                    <span class="icon">≡</span>
                    <span class="label">Activity</span>
                    <span class="badge" id="history-badge" style="display: none;">0</span>
                </button>
                <button id="btn-toggle-theme" class="toolbar-btn" title="Switch to light mode" aria-label="Switch to light mode">
                    <span class="icon">☀</span>
                    <span class="label">Day</span>
                </button>
                <button id="btn-system" class="toolbar-btn" title="Background & system settings" aria-label="System settings">
                    <span class="icon">⚙</span>
                    <span class="label">System</span>
                </button>
            </div>
            <div class="toolbar-section">
                <label>Size</label>
                <span class="toolbar-select">4×3</span>
            </div>
            <div class="toolbar-section">
                <button id="btn-export-profile" class="toolbar-btn" title="Export Profile">
                    <span class="label">Export</span>
                </button>
                <button id="btn-import-profile" class="toolbar-btn" title="Import Profile">
                    <span class="label">Import</span>
                </button>
                <input type="file" id="file-import-profile" accept=".json" style="display: none;">
            </div>
        `;
        return toolbar;
    }

    createInspector() {
        const inspector = document.createElement('div');
        inspector.id = 'inspector-panel';
        inspector.className = 'inspector-panel';
        inspector.innerHTML = `
            <div class="inspector-header">
                <div>
                    <p class="panel-kicker">Key editor</p>
                    <h3>Inspector</h3>
                </div>
                <button id="btn-close-inspector" class="close-btn">×</button>
            </div>
            <div class="inspector-content">
                <div class="inspector-empty">
                    <p>Select a key on the deck to edit its label, color, and action.</p>
                </div>
                <div class="inspector-editor" style="display: none;">
                    <div class="form-group">
                        <label>Label</label>
                        <input type="text" id="input-label" class="form-control" placeholder="Button Label">
                    </div>

                    <div class="form-group">
                        <label>Icon (Emoji or text for CYD)</label>
                        <input type="text" id="input-icon" class="form-control" placeholder="🎯 or OBS">
                        <div id="icon-picker" class="icon-picker" role="listbox" aria-label="Icon presets"></div>
                    </div>

                    <div id="button-preview" class="button-preview" aria-label="Key preview">
                        <div class="button-preview-face">
                            <span class="button-preview-icon"></span>
                            <span class="button-preview-label"></span>
                        </div>
                        <p class="button-preview-note"></p>
                    </div>

                    <div class="form-group">
                        <label>Background Color</label>
                        <div class="color-picker-group">
                            <input type="color" id="input-color" class="color-input" value="#1a1a2e">
                            <input type="text" id="input-color-text" class="form-control color-text" value="#1a1a2e">
                        </div>
                        <div class="color-presets">
                            <button class="color-preset" data-color="#1a1a2e" style="background: #1a1a2e"></button>
                            <button class="color-preset" data-color="#007acc" style="background: #007acc"></button>
                            <button class="color-preset" data-color="#2496ed" style="background: #2496ed"></button>
                            <button class="color-preset" data-color="#1db954" style="background: #1db954"></button>
                            <button class="color-preset" data-color="#ff0000" style="background: #ff0000"></button>
                            <button class="color-preset" data-color="#ea4335" style="background: #ea4335"></button>
                            <button class="color-preset" data-color="#4285f4" style="background: #4285f4"></button>
                            <button class="color-preset" data-color="#217346" style="background: #217346"></button>
                        </div>
                    </div>

                    <div class="form-group">
                        <label>Action Type</label>
                        <select id="select-action-type" class="form-control">
                            <option value="">No Action</option>
                            <option value="open_url">Open URL</option>
                            <option value="open_app">Open Application</option>
                            <option value="run_command">Run Command</option>
                            <option value="keyboard_shortcut">Keyboard Shortcut</option>
                            <option value="home_assistant">Home Assistant</option>
                            <option value="obs_control">OBS Control</option>
                            <option value="copy_text">Copy Text</option>
                            <option value="http_check">HTTP Health Check</option>
                            <option value="ping">Ping Host</option>
                            <option value="docker_command">Docker Command</option>
                            <option value="switch_profile">Switch Profile (Cycle)</option>
                            <option value="macro">Macro (multi-step)</option>
                            <option value="widget">Live Widget</option>
                        </select>
                    </div>

                    <div id="action-config" class="action-config">
                        <!-- Dynamic action configuration will be inserted here -->
                    </div>

                    <div class="form-actions">
                        <button id="btn-apply-changes" class="btn btn-primary">Apply Changes</button>
                        <button id="btn-test-action" class="btn btn-secondary">Test Action</button>
                    </div>
                </div>
            </div>
        `;
        return inspector;
    }

    createProfilesPanel() {
        const panel = document.createElement('div');
        panel.id = 'profiles-panel';
        panel.className = 'profiles-panel';
        panel.innerHTML = `
            <div class="panel-header">
                <p class="panel-kicker">Layouts</p>
                <h3>Profiles</h3>
            </div>
            <div class="panel-content">
                <div id="profiles-list" class="profiles-list">
                    <!-- Profiles will be populated here -->
                </div>
                <div class="panel-actions">
                    <button id="btn-new-profile" class="btn btn-block">+ New Profile</button>
                </div>
                <div class="preset-section" id="home-page-section">
                    <p class="panel-kicker">ESP32 home page</p>
                    <div class="form-group">
                        <label>City</label>
                        <input type="text" id="home-city" class="form-control" placeholder="Cairo">
                    </div>
                    <div class="form-group">
                        <label>Country</label>
                        <input type="text" id="home-country" class="form-control" placeholder="Egypt">
                    </div>
                    <div class="form-group">
                        <label>Prayer method</label>
                        <select id="home-prayer-method" class="form-control">
                            <option value="5">Egypt (General Authority)</option>
                            <option value="4">Makkah (Umm al-Qura)</option>
                            <option value="3">Muslim World League</option>
                            <option value="2">ISNA North America</option>
                            <option value="1">Karachi</option>
                            <option value="0">Jafari / Shia</option>
                            <option value="7">Tehran</option>
                            <option value="8">Gulf Region</option>
                            <option value="9">Kuwait</option>
                            <option value="10">Qatar</option>
                            <option value="11">Singapore</option>
                            <option value="12">France (UOIF)</option>
                            <option value="13">Turkey (Diyanet)</option>
                            <option value="14">Russia</option>
                            <option value="15">Moonsighting</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Coordinates (optional, skip geocoding)</label>
                        <div style="display: flex; gap: 6px;">
                            <input type="text" id="home-lat" class="form-control" placeholder="Lat">
                            <input type="text" id="home-lon" class="form-control" placeholder="Lon">
                        </div>
                    </div>

                    <div class="form-group">
                        <label>Screen brightness (<span id="home-brightness-val">100</span>%)</label>
                        <input type="range" id="home-brightness" class="form-control" min="10" max="100" value="100">
                    </div>
                    <div class="form-group">
                        <label>Home profile buttons (slot order on ESP32)</label>
                        <select id="home-slot-0" class="form-control home-slot" style="margin-bottom: 6px;"></select>
                        <select id="home-slot-1" class="form-control home-slot" style="margin-bottom: 6px;"></select>
                        <select id="home-slot-2" class="form-control home-slot" style="margin-bottom: 6px;"></select>
                        <select id="home-slot-3" class="form-control home-slot"></select>
                    </div>
                    <div class="panel-actions">
                        <button id="btn-save-home" class="btn btn-block">Save home settings</button>
                    </div>
                    <div id="home-preview" class="history-empty" style="margin-top: 6px;">Home preview: —</div>
                </div>
            </div>
        `;
        return panel;
    }

    createSystemPanel() {
        const panel = document.createElement('div');
        panel.id = 'system-panel';
        panel.className = 'system-panel';
        panel.innerHTML = `
            <div class="history-header">
                <div>
                    <p class="panel-kicker">Background & system</p>
                    <h3>System</h3>
                </div>
                <button id="btn-close-system" class="close-btn" title="Hide panel">×</button>
            </div>
            <div style="padding: 4px 16px 16px;">
                <div class="system-row">
                    <div>
                        <div class="system-label">Run in background</div>
                        <div class="system-hint" id="system-autostart-via">Start automatically at login</div>
                    </div>
                    <label class="switch" title="Toggle background startup">
                        <input type="checkbox" id="system-autostart-toggle">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="system-row">
                    <div>
                        <div class="system-label">Companion server</div>
                        <div class="system-hint">Stop the background server process</div>
                    </div>
                    <button id="btn-quit-server" class="btn btn-secondary" type="button" style="flex: none;">Quit</button>
                </div>
                <div class="system-status" id="system-status-line"></div>
            </div>
        `;
        return panel;
    }

    async loadSystemState() {
        const toggle = document.getElementById('system-autostart-toggle');
        const via = document.getElementById('system-autostart-via');
        const status = document.getElementById('system-status-line');
        try {
            const response = await fetch(`${this.actions.serverUrl}/api/system/autostart`);
            const result = await response.json();
            if (!result.success) {
                throw new Error(result.error || 'Failed to load system state');
            }
            if (toggle) {
                toggle.checked = !!result.enabled;
                toggle.disabled = !result.supported;
            }
            if (via) via.textContent = result.via || 'Start automatically at login';
            if (status) {
                status.textContent = result.supported
                    ? `Background startup is ${result.enabled ? 'ON' : 'OFF'} (${result.platform})`
                    : `Background startup not supported here: ${result.via || ''}`;
            }
        } catch (error) {
            if (status) status.textContent = `System status unavailable: ${error.message}`;
            if (toggle) toggle.disabled = true;
        }
    }

    createHistoryPanel() {
        const panel = document.createElement('div');
        panel.id = 'action-history-panel';
        panel.className = 'action-history-panel';
        panel.innerHTML = `
            <div class="history-header">
                <div>
                    <p class="panel-kicker">Activity</p>
                    <h3>Action History</h3>
                </div>
                <div style="display: flex; gap: 6px;">
                    <button id="btn-clear-history" class="close-btn" title="Clear history">⌫</button>
                    <button id="btn-close-history" class="close-btn" title="Hide panel">×</button>
                </div>
            </div>
            <div id="action-history-list" class="action-history-list">
                <p class="history-empty">No actions yet. Press a key to run one.</p>
            </div>
        `;
        return panel;
    }

    attachEventListeners() {
        // Toggle mode
        document.getElementById('btn-toggle-mode')?.addEventListener('click', () => {
            this.toggleMode();
        });

        document.getElementById('btn-toggle-theme')?.addEventListener('click', () => {
            this.applyTheme(this.theme === 'dark' ? 'light' : 'dark');
        });

// Removed: Deck size change listener

        // Save profile
        document.getElementById('btn-save-profile')?.addEventListener('click', () => {
            this.profiles.saveCurrentProfile();
            this.showToast('Profile saved successfully');
        });

        // Clear deck
        document.getElementById('btn-clear-deck')?.addEventListener('click', () => {
            if (confirm('Clear all buttons?')) {
                this.deck.clear();
            }
        });

        // Export/Import
        document.getElementById('btn-export-profile')?.addEventListener('click', () => {
            this.profiles.exportProfileToFile();
        });

        document.getElementById('btn-import-profile')?.addEventListener('click', () => {
            document.getElementById('file-import-profile')?.click();
        });

        document.getElementById('file-import-profile')?.addEventListener('change', async (e) => {
            const file = e.target.files[0];
            if (file) {
                try {
                    const profile = await this.profiles.importProfileFromFile(file);
                    this.refreshProfilesList();
                    this.showToast('Profile imported successfully');
                } catch (error) {
                    alert('Failed to import profile: ' + error.message);
                }
            }
        });

        // Inspector controls
        document.getElementById('btn-close-inspector')?.addEventListener('click', () => {
            this.closeInspector();
        });

        document.getElementById('btn-apply-changes')?.addEventListener('click', () => {
            this.applyButtonChanges();
        });

        document.getElementById('btn-test-action')?.addEventListener('click', () => {
            this.testButtonAction();
        });

        // Action type change
        document.getElementById('select-action-type')?.addEventListener('change', (e) => {
            this.updateActionConfig(e.target.value);
        });

        // Color picker sync
        const colorInput = document.getElementById('input-color');
        const colorText = document.getElementById('input-color-text');

        colorInput?.addEventListener('input', (e) => {
            colorText.value = e.target.value;
        });

        colorText?.addEventListener('input', (e) => {
            colorInput.value = e.target.value;
        });

        // Color presets
        document.querySelectorAll('.color-preset').forEach(btn => {
            btn.addEventListener('click', () => {
                const color = btn.dataset.color;
                colorInput.value = color;
                colorText.value = color;
            });
        });

        // Button click for editing
        this.deck.container.addEventListener('buttonClick', (e) => {
            if (this.editorMode === 'studio') {
                this.openInspector(e.detail.button);
            } else {
                this.actions.executeAction(e.detail.button);
            }
        });

        // New profile
        document.getElementById('btn-new-profile')?.addEventListener('click', () => {
            const name = prompt('Enter profile name:');
            if (name) {
                const profile = this.profiles.createProfile(name);
                if (profile) {
                    this.refreshProfilesList();
                    this.showToast('Profile created');
                }
            }
        });

        // Home page settings (city/country/prayer method for ESP32 home screen)
        document.getElementById('btn-save-home')?.addEventListener('click', () => {
            this.saveHomeSettings();
        });
        document.getElementById('home-brightness')?.addEventListener('input', (e) => {
            const label = document.getElementById('home-brightness-val');
            if (label) label.textContent = e.target.value;
        });
        this.loadHomeSettings();

        this.deck.container.addEventListener('action:serverStatus', (e) => {
            this.updateServerStatus(e.detail.available);
            if (e.detail.available) {
                this.pollDeviceStatus();
            }
        });

        this.deck.container.addEventListener('action:historyUpdated', () => {
            this.refreshActionHistory();
        });

        this.deck.container.addEventListener('profile:profileLoaded', () => {
            this.refreshProfilesList();
        });

        document.getElementById('btn-clear-history')?.addEventListener('click', () => {
            this.actions.clearHistory();
            this.refreshActionHistory();
        });

        document.getElementById('btn-toggle-history')?.addEventListener('click', () => {
            const open = document.body.classList.toggle('history-open');
            document.getElementById('btn-toggle-history')?.classList.toggle('active', open);
        });

        document.getElementById('btn-close-history')?.addEventListener('click', () => {
            document.body.classList.remove('history-open');
            document.getElementById('btn-toggle-history')?.classList.remove('active');
        });

        document.getElementById('btn-system')?.addEventListener('click', () => {
            const open = document.body.classList.toggle('system-open');
            document.getElementById('btn-system')?.classList.toggle('active', open);
            if (open) this.loadSystemState();
        });

        document.getElementById('btn-close-system')?.addEventListener('click', () => {
            document.body.classList.remove('system-open');
            document.getElementById('btn-system')?.classList.remove('active');
        });

        document.getElementById('system-autostart-toggle')?.addEventListener('change', async (e) => {
            const toggle = e.target;
            toggle.disabled = true;
            try {
                const response = await fetch(`${this.actions.serverUrl}/api/system/autostart`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ enabled: toggle.checked })
                });
                const result = await response.json();
                if (!result.success) {
                    throw new Error(result.error || 'Failed to update background startup');
                }
                this.showToast(result.message || 'Background startup updated');
            } catch (error) {
                toggle.checked = !toggle.checked;
                this.showToast(error.message, 3000);
            } finally {
                this.loadSystemState();
            }
        });

        document.getElementById('btn-quit-server')?.addEventListener('click', async () => {
            if (!confirm('Stop the companion server? The simulator will go offline until you start it again.')) {
                return;
            }
            try {
                await fetch(`${this.actions.serverUrl}/api/system/exit`, { method: 'POST' });
                this.showToast('Server stopped');
            } catch (error) {
                this.showToast('Server stopped');
            }
        });

        this.setupIconPicker();
        this.setupInspectorPreviewListeners();
        this.renderPresetTemplates();
        this.refreshProfilesList();
        this.refreshActionHistory();
        this.startDeviceStatusPolling();
    }

    setupIconPicker() {
        const picker = document.getElementById('icon-picker');
        if (!picker) return;

        picker.innerHTML = this.iconPresets.map((icon) =>
            `<button type="button" class="icon-preset-btn" data-icon="${this.escapeHtml(icon)}" title="${this.escapeHtml(icon)}">${icon}</button>`
        ).join('');

        picker.addEventListener('click', (event) => {
            const button = event.target.closest('.icon-preset-btn');
            if (!button) return;
            const iconInput = document.getElementById('input-icon');
            if (iconInput) {
                iconInput.value = button.dataset.icon;
                this.updateButtonPreview();
            }
        });
    }

    setupInspectorPreviewListeners() {
        ['input-label', 'input-icon', 'input-color', 'input-color-text'].forEach((id) => {
            document.getElementById(id)?.addEventListener('input', () => this.updateButtonPreview());
        });
    }

    updateButtonPreview() {
        const preview = document.getElementById('button-preview');
        if (!preview) return;

        const label = document.getElementById('input-label')?.value || '';
        const icon = document.getElementById('input-icon')?.value || '';
        const colorText = document.getElementById('input-color-text')?.value || '#1a1a2e';
        const face = preview.querySelector('.button-preview-face');
        const iconEl = preview.querySelector('.button-preview-icon');
        const labelEl = preview.querySelector('.button-preview-label');
        const noteEl = preview.querySelector('.button-preview-note');

        if (face) {
            face.style.background = colorText.startsWith('#') || colorText.startsWith('linear-gradient')
                ? colorText
                : '#1a1a2e';
        }
        if (iconEl) iconEl.textContent = icon || '◻';
        if (labelEl) labelEl.textContent = label || 'Label';

        if (noteEl) {
            const warnings = [];
            if (colorText.includes('gradient')) {
                warnings.push('Gradient shows as solid color on CYD display');
            }
            if (icon && icon.length > 4 && !/[\u{1F300}-\u{1FAFF}]/u.test(icon)) {
                warnings.push('Long text icons are truncated on CYD');
            }
            noteEl.textContent = warnings.join(' · ');
            noteEl.style.display = warnings.length ? 'block' : 'none';
        }
    }

    startDeviceStatusPolling() {
        this.pollDeviceStatus();
        if (this.deviceStatusTimer) {
            clearInterval(this.deviceStatusTimer);
        }
        this.deviceStatusTimer = setInterval(() => this.pollDeviceStatus(), 5000);
    }

    async pollDeviceStatus() {
        if (!this.actions.serverAvailable) {
            this.updateEsp32Status(null);
            return;
        }

        try {
            const response = await fetch(`${this.actions.serverUrl}/api/device-status`);
            if (!response.ok) return;
            const data = await response.json();
            this.updateEsp32Status(data.esp32 || null);

            // Auto-sync profile if changed on server/ESP32 (pull only)
            if (data.profile_name && data.profile_name !== this.profiles.currentProfile?.name) {
                console.log('Profile change detected, syncing...');
                this.profiles.loadProfile(data.profile_name, { push: false });
            }
        } catch (error) {
            this.updateEsp32Status(null);
        }
    }

    updateEsp32Status(esp32State) {
        const pill = document.getElementById('esp32-status');
        if (!pill) return;

        if (!esp32State || !esp32State.last_sync) {
            pill.classList.remove('is-online');
            pill.classList.add('is-offline');
            pill.textContent = 'ESP32 not synced';
            pill.title = 'No ESP32 sync detected yet';
            return;
        }

        const connected = !!esp32State.connected;
        pill.classList.toggle('is-online', connected);
        pill.classList.toggle('is-offline', !connected);

        const ago = esp32State.seconds_ago != null ? `${Math.round(esp32State.seconds_ago)}s ago` : '';
        const profile = esp32State.profile_name ? ` · ${esp32State.profile_name}` : '';
        pill.textContent = connected ? `ESP32 synced ${ago}${profile}` : `ESP32 stale ${ago}`;
        pill.title = `Last hardware sync: ${esp32State.last_sync}`;
    }

    updateServerStatus(available) {
        const pill = document.getElementById('server-status');
        if (!pill) return;
        pill.classList.toggle('is-online', !!available);
        pill.classList.toggle('is-offline', !available);
        pill.textContent = available ? 'Companion online' : 'Companion offline';
    }

    renderPresetTemplates() {
        const container = document.getElementById('preset-templates-list');
        if (!container) return;

        container.innerHTML = this.presetFiles.map((preset) =>
            `<button type="button" class="preset-template-btn" data-file="${preset.file}">${this.escapeHtml(preset.label)}</button>`
        ).join('');

        container.querySelectorAll('.preset-template-btn').forEach((button) => {
            button.addEventListener('click', () => this.importPresetTemplate(button.dataset.file));
        });
    }

    async importPresetTemplate(fileName) {
        try {
            const candidates = [
                `${this.actions.serverUrl}/presets/${fileName}`,
                `./presets/${fileName}`,
                `presets/${fileName}`
            ];

            let response = null;
            for (const url of candidates) {
                try {
                    const attempt = await fetch(url);
                    if (attempt.ok) {
                        response = attempt;
                        break;
                    }
                } catch (error) {
                    // Try next candidate
                }
            }

            if (!response) {
                throw new Error('Preset file not found');
            }

            const profile = await response.json();
            const imported = this.profiles.importProfile(JSON.stringify(profile));
            if (imported) {
                this.profiles.loadProfile(imported);
                this.refreshProfilesList();
                this.showToast(`Loaded template: ${imported.name}`);
            }
        } catch (error) {
            this.showToast(`Failed to load template: ${error.message}`, 3000);
        }
    }

    refreshActionHistory() {
        const history = this.actions.getHistory(20);

        const badge = document.getElementById('history-badge');
        if (badge) {
            const total = this.actions.actionHistory.length;
            badge.style.display = total ? 'inline-grid' : 'none';
            badge.textContent = total > 99 ? '99+' : total;
        }
        document.getElementById('btn-toggle-history')?.classList.toggle(
            'has-error', history.some((entry) => entry.status === 'error'));

        const list = document.getElementById('action-history-list');
        if (!list) return;

        if (!history.length) {
            list.innerHTML = '<p class="history-empty">No actions yet. Press a key to run one.</p>';
            return;
        }

        list.innerHTML = history.map((entry) => {
            const time = new Date(entry.timestamp).toLocaleTimeString();
            const statusClass = entry.status === 'success' ? 'is-success' : 'is-error';
            const label = this.escapeHtml(entry.buttonLabel || `Key ${entry.buttonIndex + 1}`);
            const type = this.escapeHtml(entry.actionType || 'unknown');
            const detail = entry.error ? `<span class="history-error">${this.escapeHtml(entry.error)}</span>` : '';
            return `
                <div class="history-item ${statusClass}">
                    <div class="history-row">
                        <strong>${label}</strong>
                        <span class="history-time">${time}</span>
                    </div>
                    <div class="history-meta">${type}${detail}</div>
                </div>
            `;
        }).join('');
    }

    applyTheme(theme) {
        this.theme = theme === 'light' ? 'light' : 'dark';
        document.body.classList.toggle('theme-light', this.theme === 'light');
        localStorage.setItem('streamdeck_theme', this.theme);

        const button = document.getElementById('btn-toggle-theme');
        if (button) {
            const lightMode = this.theme === 'light';
            button.title = lightMode ? 'Switch to dark mode' : 'Switch to light mode';
            button.setAttribute('aria-label', button.title);
            button.querySelector('.icon').textContent = lightMode ? '☾' : '☀';
            button.querySelector('.label').textContent = lightMode ? 'Night' : 'Day';
        }
    }

    toggleMode() {
        this.editorMode = this.editorMode === 'player' ? 'studio' : 'player';
        const studioContainer = document.getElementById('studio-container');
        const toggleBtn = document.getElementById('btn-toggle-mode');

        const isStudio = this.editorMode === 'studio';
        studioContainer.classList.toggle('studio-hidden', !isStudio);
        document.body.classList.toggle('studio-open', isStudio);
        toggleBtn.classList.toggle('active', isStudio);
        toggleBtn.querySelector('.label').textContent = isStudio ? 'Player' : 'Studio';
        const hint = document.querySelector('.footer-hint');
        if (hint) {
            hint.innerHTML = isStudio
                ? '<kbd>Esc</kbd> close inspector · click a key to edit'
                : '<kbd>Ctrl</kbd>+<kbd>E</kbd> studio · <kbd>Ctrl</kbd>+<kbd>S</kbd> save · click a key to run';
        }
        if (!isStudio) {
            this.closeInspector();
        }
    }

    openInspector(button) {
        this.selectedButton = button;
        const inspector = document.getElementById('inspector-panel');
        const emptyState = inspector.querySelector('.inspector-empty');
        const editor = inspector.querySelector('.inspector-editor');

        emptyState.style.display = 'none';
        editor.style.display = 'block';
        inspector.classList.add('open');

        // Populate fields
        document.getElementById('input-label').value = button.config.label || '';
        document.getElementById('input-icon').value = button.config.icon || '';
        document.getElementById('input-color').value = button.config.color || '#1a1a2e';
        document.getElementById('input-color-text').value = button.config.color || '#1a1a2e';

        const actionType = button.config.action?.type || '';
        document.getElementById('select-action-type').value = actionType;
        this.updateActionConfig(actionType, button.config.action);
        this.updateButtonPreview();

        // Highlight selected button
        this.deck.getAllButtons().forEach(btn => {
            btn.element.classList.remove('selected');
        });
        button.element.classList.add('selected');
    }

    closeInspector() {
        this.selectedButton = null;
        const inspector = document.getElementById('inspector-panel');
        inspector.classList.remove('open');

        this.deck.getAllButtons().forEach(btn => {
            btn.element.classList.remove('selected');
        });
    }

    attrValue(value) {
        return this.escapeHtml(value ?? '');
    }

    updateActionConfig(actionType, existingAction = null) {
        const configContainer = document.getElementById('action-config');
        configContainer.innerHTML = '';

        const configs = {
            'open_url': `
                <div class="form-group">
                    <label>URL</label>
                    <input type="text" id="action-url" class="form-control" placeholder="https://example.com" value="${this.attrValue(existingAction?.url)}">
                </div>
            `,
            'open_app': `
                <div class="form-group">
                    <label>Application</label>
                    <input type="text" id="action-app" class="form-control" placeholder="code, chrome.exe, notepad" value="${this.attrValue(existingAction?.app)}">
                </div>
            `,
            'run_command': `
                <div class="form-group">
                    <label>Command</label>
                    <textarea id="action-command" class="form-control" rows="3" placeholder="dir">${this.escapeHtml(existingAction?.command || '')}</textarea>
                </div>
                <div class="form-group">
                    <label>Shell</label>
                    <select id="action-shell" class="form-control">
                        <option value="powershell" ${existingAction?.shell === 'powershell' ? 'selected' : ''}>PowerShell</option>
                        <option value="cmd" ${existingAction?.shell === 'cmd' ? 'selected' : ''}>CMD</option>
                    </select>
                </div>
            `,
            'obs_control': `
                <div class="form-group">
                    <label>OBS Operation</label>
                    <select id="action-obs-operation" class="form-control">
                        <option value="set_scene">Change Scene</option>
                        <option value="start_recording">Start Recording</option>
                        <option value="stop_recording">Stop Recording</option>
                        <option value="toggle_recording">Start/Stop Recording (Toggle)</option>
                        <option value="set_source_visibility">Show/Hide Source</option>
                    </select>
                </div>
                <div class="form-group" id="obs-scene-group">
                    <label>Scene Name</label>
                    <input type="text" id="action-obs-scene" class="form-control" placeholder="e.g. Gaming Scene" value="${this.attrValue(existingAction?.scene)}">
                </div>
                <div class="form-group" id="obs-source-group">
                    <label>Source Name</label>
                    <input type="text" id="action-obs-source" class="form-control" placeholder="e.g. Camera" value="${this.attrValue(existingAction?.source)}">
                </div>
                <div class="form-group" id="obs-visible-group">
                    <label>Visibility</label>
                    <select id="action-obs-visible" class="form-control">
                        <option value="true" ${existingAction?.visible !== false ? 'selected' : ''}>Show</option>
                        <option value="false" ${existingAction?.visible === false ? 'selected' : ''}>Hide</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>OBS WebSocket Connection (optional)</label>
                    <input type="text" id="action-obs-host" class="form-control" placeholder="Host (default: 127.0.0.1)" value="${this.attrValue(existingAction?.host)}" style="margin-bottom: 6px;">
                    <input type="text" id="action-obs-port" class="form-control" placeholder="Port (default: 4455)" value="${this.attrValue(existingAction?.port)}" style="margin-bottom: 6px;">
                    <input type="password" id="action-obs-password" class="form-control" placeholder="Password (if set in OBS)" value="${this.attrValue(existingAction?.password)}">
                </div>
                <div class="form-group">
                    <button id="btn-obs-test" class="btn btn-secondary" type="button" style="width: 100%;">🔌 Test OBS Connection</button>
                    <div id="obs-test-result" style="margin-top: 6px; font-size: 12px; line-height: 1.4;"></div>
                    <small style="display: block; margin-top: 6px; opacity: 0.7;">Enable in OBS: Tools → WebSocket Server Settings (port 4455)</small>
                </div>
            `,
            'copy_text': `
                <div class="form-group">
                    <label>Text to Copy</label>
                    <textarea id="action-text" class="form-control" rows="3" placeholder="Text to copy...">${this.escapeHtml(existingAction?.text || '')}</textarea>
                </div>
            `,
            'keyboard_shortcut': `
                <div class="form-group">
                    <label>Keys (e.g. ctrl+c, win+l, media_play_pause)</label>
                    <input type="text" id="action-keys" class="form-control" placeholder="ctrl+shift+s" value="${this.attrValue(existingAction?.keys)}">
                </div>
                <small style="display: block; margin-top: 6px; opacity: 0.7;">Modifiers: ctrl, alt, shift, win + key (a-z, 0-9, f1-f24, enter, tab, esc, arrows, media_play_pause, volume_up...). Ctrl+Alt+Delete is blocked by Windows.</small>
            `,
            'home_assistant': `
                <div class="form-group" style="border: 1px solid var(--line); border-radius: 10px; padding: 10px;">
                    <label>HA Server</label>
                    <input type="text" id="action-ha-server-url" class="form-control" placeholder="http://192.168.1.50:8123" value="" style="margin-bottom: 6px;">
                    <input type="password" id="action-ha-server-token" class="form-control" placeholder="Long-lived token (saved on server only)" value="" style="margin-bottom: 6px;">
                    <button id="btn-ha-save-server" class="btn btn-secondary" type="button" style="width: 100%;">💾 Save server settings</button>
                    <div id="ha-server-result" style="margin-top: 6px; font-size: 12px; line-height: 1.4;"></div>
                </div>
                <div class="form-group">
                    <label>Devices (pick to fill Entity ID)</label>
                    <div style="display: flex; gap: 6px;">
                        <button id="btn-ha-load-devices" class="btn btn-secondary" type="button" style="flex: 1;">🔄 Load devices</button>
                    </div>
                    <select id="action-ha-entity-select" class="form-control" style="margin-top: 6px;">
                        <option value="">— load devices first —</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Domain</label>
                    <select id="action-ha-domain" class="form-control">
                        <option value="light" ${existingAction?.domain === 'light' ? 'selected' : ''}>Light</option>
                        <option value="switch" ${existingAction?.domain === 'switch' ? 'selected' : ''}>Switch</option>
                        <option value="script" ${existingAction?.domain === 'script' ? 'selected' : ''}>Script</option>
                        <option value="scene" ${existingAction?.domain === 'scene' ? 'selected' : ''}>Scene</option>
                        <option value="fan" ${existingAction?.domain === 'fan' ? 'selected' : ''}>Fan</option>
                        <option value="cover" ${existingAction?.domain === 'cover' ? 'selected' : ''}>Cover</option>
                        <option value="climate" ${existingAction?.domain === 'climate' ? 'selected' : ''}>Climate</option>
                        <option value="media_player" ${existingAction?.domain === 'media_player' ? 'selected' : ''}>Media Player</option>
                        <option value="automation" ${existingAction?.domain === 'automation' ? 'selected' : ''}>Automation</option>
                        <option value="input_boolean" ${existingAction?.domain === 'input_boolean' ? 'selected' : ''}>Input Boolean</option>
                        <option value="lock" ${existingAction?.domain === 'lock' ? 'selected' : ''}>Lock</option>
                        <option value="vacuum" ${existingAction?.domain === 'vacuum' ? 'selected' : ''}>Vacuum</option>
                        <option value="homeassistant" ${existingAction?.domain === 'homeassistant' ? 'selected' : ''}>Home Assistant</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Service</label>
                    <input type="text" id="action-ha-service" class="form-control" placeholder="turn_on" value="${this.attrValue(existingAction?.service)}">
                </div>
                <div class="form-group">
                    <label>Entity ID</label>
                    <input type="text" id="action-ha-entity" class="form-control" placeholder="light.bedroom" value="${this.attrValue(existingAction?.entity_id)}">
                </div>
                <div class="form-group">
                    <label>Service Data (JSON, optional)</label>
                    <input type="text" id="action-ha-data" class="form-control" placeholder='{"brightness": 128}' value="${this.attrValue(existingAction?.data)}">
                </div>
                <div class="form-group">
                    <label>HA URL override (optional)</label>
                    <input type="text" id="action-ha-url" class="form-control" placeholder="Default from server env" value="${this.attrValue(existingAction?.url)}">
                </div>
                <div class="form-group">
                    <button id="btn-ha-test" class="btn btn-secondary" type="button" style="width: 100%;">🔌 Test HA Connection</button>
                    <div id="ha-test-result" style="margin-top: 6px; font-size: 12px; line-height: 1.4;"></div>
                    <small style="display: block; margin-top: 6px; opacity: 0.7;">Set HA_URL and HA_TOKEN on the server PC (HA → profile → Security → Long-lived access tokens). The token is never stored in profiles.</small>
                </div>
            `,
            'http_check': `
                <div class="form-group">
                    <label>URL to Check</label>
                    <input type="text" id="action-url" class="form-control" placeholder="http://localhost:3000" value="${this.attrValue(existingAction?.url)}">
                </div>
            `,
            'ping': `
                <div class="form-group">
                    <label>Host</label>
                    <input type="text" id="action-host" class="form-control" placeholder="8.8.8.8 or google.com" value="${this.attrValue(existingAction?.host || '8.8.8.8')}">
                </div>
            `,
            'docker_command': `
                <div class="form-group">
                    <label>Action</label>
                    <select id="action-docker-action" class="form-control">
                        <option value="start" ${existingAction?.dockerAction === 'start' ? 'selected' : ''}>Start</option>
                        <option value="stop" ${existingAction?.dockerAction === 'stop' ? 'selected' : ''}>Stop</option>
                        <option value="restart" ${existingAction?.dockerAction === 'restart' ? 'selected' : ''}>Restart</option>
                        <option value="status" ${existingAction?.dockerAction === 'status' ? 'selected' : ''}>Status</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Container Name</label>
                    <input type="text" id="action-container" class="form-control" placeholder="container-name" value="${this.attrValue(existingAction?.container)}">
                </div>
            `,
            'widget': `
                <div class="form-group">
                    <label>Widget Type</label>
                    <select id="action-widget-type" class="form-control">
                        <option value="clock">Clock</option>
                        <option value="date">Date</option>
                        <option value="cpu_ram">CPU & RAM Monitor</option>
                        <option value="stopwatch">Stopwatch</option>
                        <option value="timer">Timer</option>
                        <option value="pomodoro">Pomodoro</option>
                        <option value="ping_monitor">Ping Monitor</option>
                        <option value="uptime">Uptime</option>
                    </select>
                </div>
                <div id="pomodoro-config" style="display: none; margin-top: 8px;">
                    <div class="form-group">
                        <label>Focus (minutes)</label>
                        <input type="number" id="action-pomo-work" class="form-control" min="1" max="180" value="25">
                    </div>
                    <div class="form-group">
                        <label>Short break (minutes)</label>
                        <input type="number" id="action-pomo-short" class="form-control" min="1" max="60" value="5">
                    </div>
                    <div class="form-group">
                        <label>Long break (minutes)</label>
                        <input type="number" id="action-pomo-long" class="form-control" min="1" max="90" value="15">
                    </div>
                    <div class="form-group">
                        <label>Sessions before long break</label>
                        <input type="number" id="action-pomo-sessions" class="form-control" min="1" max="12" value="4">
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="action-pomo-autostart"> Auto-start next phase</label>
                    </div>
                    <small style="display: block; margin-top: 6px; opacity: 0.7;">Press = start/pause · double-press = reset session</small>
                </div>
            `,
            'switch_profile': `
                <div class="form-group">
                    <label>Target Profile Name (Optional)</label>
                    <input type="text" id="action-profile-name" class="form-control" placeholder="Leave empty to cycle" value="${this.attrValue(existingAction?.name)}">
                </div>
                <p class="text-muted" style="font-size: 12px; color: #888;">If empty, it cycles through all profiles.</p>
            `,
            'macro': `
                <div class="form-group">
                    <label>Steps (run in order)</label>
                    <div id="macro-steps-list"></div>
                    <button id="btn-macro-add-step" class="btn btn-secondary" type="button" style="width: 100%; margin-top: 6px;">+ Add step</button>
                </div>
                <small style="display: block; margin-top: 6px; opacity: 0.7;">Each step: type + value (keys / app / url / command / text / ms for delay) + optional pause after it.</small>
            `
        };

        if (configs[actionType]) {
            configContainer.innerHTML = configs[actionType];
        }

        if (actionType === 'widget') {
            this.setupWidgetConfig();
        }

        if (actionType === 'macro') {
            this.setupMacroConfig(existingAction);
        }

        if (actionType === 'home_assistant') {
            this.setupHaConfig(existingAction);
        }

        if (actionType === 'obs_control') {
            this.setupObsConfig(existingAction);
        }
    }

    setupWidgetConfig() {
        const typeSelect = document.getElementById('action-widget-type');
        const pomoConfig = document.getElementById('pomodoro-config');
        if (!typeSelect || !pomoConfig) return;
        const syncPomoVisibility = () => {
            pomoConfig.style.display = typeSelect.value === 'pomodoro' ? 'block' : 'none';
        };
        typeSelect.addEventListener('change', syncPomoVisibility);
        syncPomoVisibility();
    }

    buildWidgetConfig(widgetType) {
        if (widgetType !== 'pomodoro') return {};
        const num = (id, fallback) => {
            const v = parseFloat(document.getElementById(id)?.value);
            return Number.isFinite(v) && v > 0 ? v : fallback;
        };
        return {
            workMinutes: num('action-pomo-work', 25),
            shortBreakMinutes: num('action-pomo-short', 5),
            longBreakMinutes: num('action-pomo-long', 15),
            sessionsBeforeLong: Math.max(1, Math.round(num('action-pomo-sessions', 4))),
            autoStart: document.getElementById('action-pomo-autostart')?.checked === true
        };
    }

    setupMacroConfig(existingAction = null) {
        const list = document.getElementById('macro-steps-list');
        const addBtn = document.getElementById('btn-macro-add-step');
        if (!list || !addBtn) return;

        const steps = Array.isArray(existingAction?.steps) ? existingAction.steps : [];
        list.innerHTML = '';
        steps.forEach((step) => this.addMacroStepRow(list, step));
        if (steps.length === 0) {
            this.addMacroStepRow(list, {});
        }

        addBtn.addEventListener('click', () => this.addMacroStepRow(list, {}));
    }

    addMacroStepRow(list, step = {}) {
        const row = document.createElement('div');
        row.className = 'macro-step';
        row.style.cssText = 'display: flex; gap: 6px; margin-bottom: 6px;';
        const type = step.type || 'keyboard_shortcut';
        row.innerHTML = `
            <select class="macro-step-type form-control">
                <option value="keyboard_shortcut" ${type === 'keyboard_shortcut' ? 'selected' : ''}>Keys</option>
                <option value="open_app" ${type === 'open_app' ? 'selected' : ''}>Open app</option>
                <option value="open_url" ${type === 'open_url' ? 'selected' : ''}>Open URL</option>
                <option value="run_command" ${type === 'run_command' ? 'selected' : ''}>Command</option>
                <option value="copy_text" ${type === 'copy_text' ? 'selected' : ''}>Copy text</option>
                <option value="home_assistant" ${type === 'home_assistant' ? 'selected' : ''}>Home Asst</option>
                <option value="delay" ${type === 'delay' ? 'selected' : ''}>Wait</option>
            </select>
            <input class="macro-step-value form-control" placeholder="keys / app / url… (HA: domain.service:entity)" value="${this.attrValue(step.keys ?? step.app ?? step.url ?? step.command ?? step.text ?? step.ms ?? (step.entity_id ? step.domain + '.' + step.service + ':' + step.entity_id : ''))}">
            <input class="macro-step-delay form-control" type="number" min="0" max="10000" placeholder="pause ms" style="max-width: 84px;" value="${this.attrValue(step.delay ?? '')}">
            <button class="macro-step-remove btn btn-secondary" type="button">✕</button>
        `;
        row.querySelector('.macro-step-remove').addEventListener('click', () => row.remove());
        list.appendChild(row);
    }

    setupObsConfig(existingAction = null) {
        const operationSelect = document.getElementById('action-obs-operation');
        const testButton = document.getElementById('btn-obs-test');
        if (!operationSelect) return;

        if (existingAction?.operation) {
            operationSelect.value = existingAction.operation;
        }

        if (this.obsHandlers.operationChange) {
            operationSelect.removeEventListener('change', this.obsHandlers.operationChange);
        }
        if (this.obsHandlers.testClick && testButton) {
            testButton.removeEventListener('click', this.obsHandlers.testClick);
        }

        this.obsHandlers.operationChange = () => this.updateObsFieldVisibility();
        this.obsHandlers.testClick = () => this.testObsConnection();

        operationSelect.addEventListener('change', this.obsHandlers.operationChange);
        testButton?.addEventListener('click', this.obsHandlers.testClick);

        this.updateObsFieldVisibility();
    }

    updateObsFieldVisibility() {
        const operation = document.getElementById('action-obs-operation')?.value;
        if (!operation) return;

        const sceneGroup = document.getElementById('obs-scene-group');
        const sourceGroup = document.getElementById('obs-source-group');
        const visibleGroup = document.getElementById('obs-visible-group');

        const needsScene = operation === 'set_scene' || operation === 'set_source_visibility';
        const needsSource = operation === 'set_source_visibility';

        if (sceneGroup) sceneGroup.style.display = needsScene ? 'block' : 'none';
        if (sourceGroup) sourceGroup.style.display = needsSource ? 'block' : 'none';
        if (visibleGroup) visibleGroup.style.display = needsSource ? 'block' : 'none';
    }

    async testObsConnection() {
        const resultBox = document.getElementById('obs-test-result');
        if (!resultBox) return;

        const payload = {
            host: document.getElementById('action-obs-host')?.value.trim() || '',
            port: document.getElementById('action-obs-port')?.value.trim() || '',
            password: document.getElementById('action-obs-password')?.value || ''
        };

        resultBox.style.color = '#999';
        resultBox.textContent = '⏳ Connecting to OBS...';

        try {
            const response = await fetch(`${this.actions.serverUrl}/api/obs-status`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const result = await response.json();

            if (result.success && result.connected) {
                const version = result.obs_version ? ` - OBS ${result.obs_version}` : '';
                const recording = result.recording ? ' (Recording)' : '';
                resultBox.style.color = '#28a745';
                resultBox.textContent = `✅ Connected${version}${recording}`;
            } else {
                resultBox.style.color = '#dc3545';
                resultBox.textContent = `❌ ${result.error || 'OBS not reachable'}`;
            }
        } catch (error) {
            resultBox.style.color = '#dc3545';
            resultBox.textContent = `❌ ${error.message}`;
        }
    }

    setupHaConfig() {
        const testButton = document.getElementById('btn-ha-test');
        if (testButton) {
            testButton.addEventListener('click', () => this.testHaConnection());
        }

        const urlInput = document.getElementById('action-ha-server-url');
        const tokenInput = document.getElementById('action-ha-server-token');
        const serverResult = document.getElementById('ha-server-result');
        const saveButton = document.getElementById('btn-ha-save-server');

        fetch(`${this.actions.serverUrl}/api/settings`)
            .then((response) => response.json())
            .then((result) => {
                const settings = result.settings || {};
                if (urlInput && settings.ha_url) urlInput.value = settings.ha_url;
                if (serverResult) {
                    serverResult.style.color = settings.ha_token_configured ? '#28a745' : '#999';
                    serverResult.textContent = settings.ha_token_configured
                        ? '✅ Token saved on server'
                        : 'No token saved yet';
                }
            })
            .catch(() => {});

        if (saveButton) {
            saveButton.addEventListener('click', async () => {
                const payload = {};
                if (urlInput) payload.ha_url = urlInput.value.trim();
                if (tokenInput && tokenInput.value) payload.ha_token = tokenInput.value;
                if (serverResult) {
                    serverResult.style.color = '#999';
                    serverResult.textContent = '⏳ Saving…';
                }
                try {
                    const response = await fetch(`${this.actions.serverUrl}/api/settings`, {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(payload)
                    });
                    const result = await response.json();
                    if (serverResult) {
                        if (result.success) {
                            if (tokenInput) tokenInput.value = '';
                            serverResult.style.color = '#28a745';
                            const saved = result.settings || {};
                            serverResult.textContent = saved.ha_token_configured
                                ? '✅ Saved — token stored on server'
                                : '✅ Saved';
                        } else {
                            serverResult.style.color = '#dc3545';
                            serverResult.textContent = `❌ ${result.error || 'Save failed'}`;
                        }
                    }
                } catch (error) {
                    if (serverResult) {
                        serverResult.style.color = '#dc3545';
                        serverResult.textContent = `❌ ${error.message}`;
                    }
                }
            });
        }

        const loadButton = document.getElementById('btn-ha-load-devices');
        const entitySelect = document.getElementById('action-ha-entity-select');
        if (loadButton && entitySelect) {
            const fillEntitySelect = (entities) => {
                entitySelect.innerHTML = '<option value="">— pick a device —</option>' + entities.map((item) =>
                    `<option value="${this.escapeHtml(item.entity_id)}">${this.escapeHtml(item.name)} (${this.escapeHtml(item.state)})</option>`
                ).join('');
            };

            const loadEntities = async (domain) => {
                const response = await fetch(
                    `${this.actions.serverUrl}/api/ha-entities?domain=${encodeURIComponent(domain)}`);
                return response.json();
            };

            loadButton.addEventListener('click', async () => {
                entitySelect.innerHTML = '<option value="">⏳ Loading…</option>';
                try {
                    const domain = document.getElementById('action-ha-domain')?.value || '';
                    let result = await loadEntities(domain);
                    if (!result.success) {
                        entitySelect.innerHTML = '<option value="">— failed to load —</option>';
                        this.showToast(result.error || 'Failed to load devices', 3000);
                        return;
                    }
                    let entities = result.entities || [];
                    if (!entities.length && domain) {
                        result = await loadEntities('');
                        if (result.success && (result.entities || []).length) {
                            entities = result.entities;
                            this.showToast(`No ${domain} devices — showing all (${entities.length})`, 3000);
                        }
                    }
                    if (!entities.length) {
                        entitySelect.innerHTML = '<option value="">— no devices found —</option>';
                        this.showToast('Home Assistant returned no entities', 3000);
                        return;
                    }
                    fillEntitySelect(entities);
                    this.showToast(`Loaded ${entities.length} devices`, 2000);
                } catch (error) {
                    entitySelect.innerHTML = '<option value="">— failed to load —</option>';
                    this.showToast(error.message, 3000);
                }
            });

            entitySelect.addEventListener('change', () => {
                const entityId = entitySelect.value;
                if (!entityId) return;
                const entityInput = document.getElementById('action-ha-entity');
                if (entityInput) entityInput.value = entityId;
                const domainSelect = document.getElementById('action-ha-domain');
                const prefix = entityId.split('.')[0];
                if (domainSelect && prefix) {
                    const match = Array.from(domainSelect.options).find((opt) => opt.value === prefix);
                    if (match) {
                        domainSelect.value = prefix;
                    } else {
                        const opt = document.createElement('option');
                        opt.value = prefix;
                        opt.textContent = prefix;
                        domainSelect.appendChild(opt);
                        domainSelect.value = prefix;
                    }
                }
            });
        }
    }

    async testHaConnection() {
        const resultBox = document.getElementById('ha-test-result');
        if (!resultBox) return;

        const url = document.getElementById('action-ha-url')?.value.trim() || '';

        resultBox.style.color = '#999';
        resultBox.textContent = '⏳ Connecting to Home Assistant...';

        try {
            const response = await fetch(`${this.actions.serverUrl}/api/ha-status`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ url })
            });
            const result = await response.json();

            if (result.success) {
                resultBox.style.color = '#28a745';
                resultBox.textContent = `✅ ${result.message || 'Connected'}`;
            } else {
                resultBox.style.color = '#dc3545';
                resultBox.textContent = `❌ ${result.error || 'Home Assistant not reachable'}`;
            }
        } catch (error) {
            resultBox.style.color = '#dc3545';
            resultBox.textContent = `❌ ${error.message}`;
        }
    }

    applyButtonChanges() {
        if (!this.selectedButton) return;

        const label = document.getElementById('input-label').value;
        const icon = document.getElementById('input-icon').value;
        const color = document.getElementById('input-color').value;
        const actionType = document.getElementById('select-action-type').value;

        let action = null;
        if (actionType) {
            try {
                action = this.buildActionFromInputs(actionType);
            } catch (error) {
                this.showToast(error.message, 3000);
                return;
            }
        }

        this.deck.updateButton(this.selectedButton.index, {
            label,
            icon,
            color,
            action
        });

        // Handle widgets
        if (actionType === 'widget') {
            const widgetType = document.getElementById('action-widget-type').value;
            this.liveKeys.registerWidget(this.selectedButton.index, widgetType, this.buildWidgetConfig(widgetType));
        } else {
            this.liveKeys.unregisterWidget(this.selectedButton.index);
        }

        this.updateButtonPreview();
        if (this.profiles) {
            this.profiles.saveCurrentProfile();
        }
        this.showToast('Changes applied & synced');
    }

    buildActionFromInputs(actionType) {
        const actions = {
            'open_url': () => ({ type: 'open_url', url: document.getElementById('action-url').value }),
            'open_app': () => ({ type: 'open_app', app: document.getElementById('action-app').value }),
            'run_command': () => ({
                type: 'run_command',
                command: document.getElementById('action-command').value,
                shell: document.getElementById('action-shell').value
            }),
            'obs_control': () => {
                const operation = document.getElementById('action-obs-operation').value;
                const action = {
                    type: 'obs_control',
                    operation,
                    host: document.getElementById('action-obs-host')?.value.trim() || '',
                    port: document.getElementById('action-obs-port')?.value.trim() || '',
                    password: document.getElementById('action-obs-password')?.value || ''
                };
                if (operation === 'set_scene' || operation === 'set_source_visibility') {
                    action.scene = document.getElementById('action-obs-scene')?.value.trim() || '';
                }
                if (operation === 'set_source_visibility') {
                    action.source = document.getElementById('action-obs-source')?.value.trim() || '';
                    action.visible = document.getElementById('action-obs-visible')?.value !== 'false';
                }
                return action;
            },
            'copy_text': () => ({ type: 'copy_text', text: document.getElementById('action-text').value }),
            'keyboard_shortcut': () => ({ type: 'keyboard_shortcut', keys: document.getElementById('action-keys').value.trim() }),
            'home_assistant': () => {
                const service = document.getElementById('action-ha-service')?.value.trim() || '';
                const entity_id = document.getElementById('action-ha-entity')?.value.trim() || '';
                if (!service || !entity_id) {
                    throw new Error('Home Assistant needs a Service (e.g. toggle) and an Entity ID — pick one from "Load devices"');
                }
                const action = {
                    type: 'home_assistant',
                    domain: document.getElementById('action-ha-domain')?.value || 'light',
                    service,
                    entity_id,
                    url: document.getElementById('action-ha-url')?.value.trim() || ''
                };
                const data = document.getElementById('action-ha-data')?.value.trim() || '';
                if (data) action.data = data;
                return action;
            },
            'http_check': () => ({ type: 'http_check', url: document.getElementById('action-url').value }),
            'ping': () => ({ type: 'ping', host: document.getElementById('action-host').value }),
            'docker_command': () => ({
                type: 'docker_command',
                dockerAction: document.getElementById('action-docker-action').value,
                container: document.getElementById('action-container').value
            }),
            'switch_profile': () => ({ type: 'switch_profile', name: document.getElementById('action-profile-name').value }),
            'macro': () => {
                const steps = [];
                document.querySelectorAll('#macro-steps-list .macro-step').forEach((row) => {
                    const type = row.querySelector('.macro-step-type').value;
                    const value = row.querySelector('.macro-step-value').value.trim();
                    const delay = parseInt(row.querySelector('.macro-step-delay').value, 10) || 0;
                    const step = { type };
                    if (type === 'keyboard_shortcut') step.keys = value;
                    else if (type === 'open_app') step.app = value;
                    else if (type === 'open_url') step.url = value;
                    else if (type === 'run_command') step.command = value;
                    else if (type === 'copy_text') step.text = value;
                    else if (type === 'home_assistant') {
                        const m = value.match(/^([a-z_]+)\.([a-z_0-9]+)\s*:\s*([a-z_]+\.[a-z0-9_]+)$/i);
                        if (!m) {
                            throw new Error('Home Assistant macro step must look like: domain.service:entity_id (e.g. light.toggle:light.bedroom)');
                        }
                        step.domain = m[1].toLowerCase();
                        step.service = m[2].toLowerCase();
                        step.entity_id = m[3].toLowerCase();
                    }
                    else if (type === 'delay') step.ms = parseInt(value, 10) || 0;
                    if (delay > 0 && type !== 'delay') step.delay = Math.min(delay, 10000);
                    if (type && (value || type === 'delay')) steps.push(step);
                });
                return { type: 'macro', steps };
            }
        };

        return actions[actionType] ? actions[actionType]() : null;
    }

    async testButtonAction() {
        if (!this.selectedButton) return;

        this.applyButtonChanges();
        await this.actions.executeAction(this.selectedButton);
    }

    refreshProfilesList() {
        const listContainer = document.getElementById('profiles-list');
        if (!listContainer) return;

        listContainer.innerHTML = '';

        const profiles = this.profiles.getAllProfiles();
        if (!profiles.length) {
            listContainer.innerHTML = '<p class="history-empty">No profiles yet. Click "+ New Profile".</p>';
            return;
        }
        profiles.forEach(profile => {
            const item = document.createElement('div');
            item.className = 'profile-item';
            if (this.profiles.currentProfile?.name === profile.name) {
                item.classList.add('active');
            }

            item.innerHTML = `
                <div class="profile-info">
                    <div class="profile-name">${this.escapeHtml(profile.name)}</div>
                    <div class="profile-meta">${profile.size} - ${profile.buttons?.length || 0} buttons</div>
                </div>
                <div class="profile-actions">
                    <button class="btn-icon btn-load" title="Load" data-name="${this.escapeHtml(profile.name)}">📂</button>
                    <button class="btn-icon btn-delete" title="Delete" data-name="${this.escapeHtml(profile.name)}">🗑️</button>
                </div>
            `;

            item.querySelector('.btn-load').addEventListener('click', () => {
                this.profiles.loadProfile(profile.name);
                this.refreshProfilesList();
            });

            item.querySelector('.btn-delete').addEventListener('click', () => {
                if (confirm(`Delete profile "${profile.name}"?`)) {
                    this.profiles.deleteProfile(profile.name);
                    this.refreshProfilesList();
                }
            });

            listContainer.appendChild(item);
        });
    }

    async loadHomeSettings() {
        try {
            const response = await fetch(`${this.actions.serverUrl}/api/settings`);
            const result = await response.json();
            const settings = result.settings || {};
            const setVal = (id, value) => {
                const el = document.getElementById(id);
                if (el) el.value = value ?? '';
            };
            setVal('home-city', settings.home_city || '');
            setVal('home-country', settings.home_country || '');
            setVal('home-lat', settings.home_lat ?? '');
            setVal('home-lon', settings.home_lon ?? '');
            const method = document.getElementById('home-prayer-method');
            if (method) method.value = String(settings.prayer_method ?? 5);
            const brightness = document.getElementById('home-brightness');
            const brightnessVal = document.getElementById('home-brightness-val');
            if (brightness) {
                brightness.value = settings.brightness ?? 100;
                if (brightnessVal) brightnessVal.textContent = brightness.value;
            }
            await this.loadHomeSlots(settings.home_slots || []);
        } catch (error) {
            console.warn('Failed to load home settings', error);
        }
        this.refreshHomePreview();
    }

    async loadHomeSlots(savedSlots = []) {
        const selects = [0, 1, 2, 3].map((i) => document.getElementById(`home-slot-${i}`));
        if (selects.some((el) => !el)) return;
        let names = [];
        try {
            const response = await fetch(`${this.actions.serverUrl}/api/profiles`);
            const result = await response.json();
            if (result.success && Array.isArray(result.profiles)) {
                names = result.profiles.filter((name) => name);
            }
        } catch (error) {
            console.warn('Failed to load profile names', error);
        }
        if (!names.length) {
            names = this.profiles.getAllProfiles().map((profile) => profile.name);
        }
        selects.forEach((select, i) => {
            const current = savedSlots[i] || names[i] || '';
            select.innerHTML = '<option value="">— auto (first profiles) —</option>' + names.map((name) =>
                `<option value="${this.escapeHtml(name)}"${name === current ? ' selected' : ''}>${this.escapeHtml(name)}</option>`
            ).join('');
            if (current && !names.includes(current)) {
                const extra = document.createElement('option');
                extra.value = current;
                extra.textContent = current;
                extra.selected = true;
                select.appendChild(extra);
            }
        });
    }

    async saveHomeSettings() {
        const val = (id) => document.getElementById(id)?.value.trim() || '';
        const payload = {
            home_city: val('home-city'),
            home_country: val('home-country'),
            home_lat: val('home-lat'),
            home_lon: val('home-lon'),
            prayer_method: parseInt(document.getElementById('home-prayer-method')?.value, 10) || 5,
            brightness: parseInt(document.getElementById('home-brightness')?.value, 10) || 100,
            home_slots: [0, 1, 2, 3].map((i) => document.getElementById(`home-slot-${i}`)?.value || '')
        };
        try {
            const response = await fetch(`${this.actions.serverUrl}/api/settings`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const result = await response.json();
            if (!result.success) {
                throw new Error(result.error || 'Save failed');
            }
            this.showToast('Home settings saved');
            this.refreshHomePreview();
        } catch (error) {
            this.showToast(error.message, 3000);
        }
    }

    async refreshHomePreview() {
        const preview = document.getElementById('home-preview');
        if (!preview) return;
        try {
            const response = await fetch(`${this.actions.serverUrl}/api/home-info`);
            const info = await response.json();
            if (!info.success) {
                throw new Error(info.error || 'Preview unavailable');
            }
            const temp = info.temp_c != null ? `${Math.round(info.temp_c)}C` : '--';
            const prayer = info.next_prayer ? `${info.next_prayer} ${info.next_prayer_time || ''}`.trim() : '--';
            preview.textContent = `Home preview: ${info.date || '--'} · ${temp} · ${prayer}`;
        } catch (error) {
            preview.textContent = 'Home preview: unavailable (is the server running?)';
        }
    }

    showToast(message, duration = 2000) {
        const toast = document.createElement('div');
        toast.className = 'toast-notification';
        toast.textContent = message;
        document.body.appendChild(toast);

        setTimeout(() => toast.classList.add('show'), 10);
        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => toast.remove(), 300);
        }, duration);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = StudioUI;
}
