/**
 * StreamDeck Studio UI Editor
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

        this.init();
    }

    init() {
        this.createStudioUI();
        this.attachEventListeners();
    }

    createStudioUI() {
        // Create main container
        const studioContainer = document.createElement('div');
        studioContainer.id = 'studio-container';
        studioContainer.className = 'studio-hidden';

        // Create toolbar
        const toolbar = this.createToolbar();

        // Create inspector panel
        const inspector = this.createInspector();

        // Create profiles panel
        const profilesPanel = this.createProfilesPanel();

        studioContainer.appendChild(toolbar);
        studioContainer.appendChild(inspector);
        studioContainer.appendChild(profilesPanel);

        document.body.appendChild(studioContainer);
    }

    createToolbar() {
        const toolbar = document.createElement('div');
        toolbar.className = 'studio-toolbar';
        toolbar.innerHTML = `
            <div class="toolbar-section">
                <button id="btn-toggle-mode" class="toolbar-btn" title="Toggle Studio Mode">
                    <span class="icon">🎨</span>
                    <span class="label">Studio</span>
                </button>
                <button id="btn-save-profile" class="toolbar-btn" title="Save Current Profile">
                    <span class="icon">💾</span>
                    <span class="label">Save</span>
                </button>
                <button id="btn-clear-deck" class="toolbar-btn" title="Clear All Buttons">
                    <span class="icon">🗑️</span>
                    <span class="label">Clear</span>
                </button>
            </div>
            <div class="toolbar-section">
                <label>Size:</label>
                <select id="select-deck-size" class="toolbar-select">
                    <option value="mini">Mini (3×2)</option>
                    <option value="cyd" selected>CYD (4×3)</option>
                    <option value="classic">Classic (5×3)</option>
                    <option value="xl">XL (8×4)</option>
                </select>
            </div>
            <div class="toolbar-section">
                <button id="btn-export-profile" class="toolbar-btn" title="Export Profile">
                    <span class="icon">📤</span>
                    <span class="label">Export</span>
                </button>
                <button id="btn-import-profile" class="toolbar-btn" title="Import Profile">
                    <span class="icon">📥</span>
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
                <h3>Button Inspector</h3>
                <button id="btn-close-inspector" class="close-btn">×</button>
            </div>
            <div class="inspector-content">
                <div class="inspector-empty">
                    <p>Click a button to edit</p>
                </div>
                <div class="inspector-editor" style="display: none;">
                    <div class="form-group">
                        <label>Label</label>
                        <input type="text" id="input-label" class="form-control" placeholder="Button Label">
                    </div>

                    <div class="form-group">
                        <label>Icon (Emoji or HTML)</label>
                        <input type="text" id="input-icon" class="form-control" placeholder="🎯 or <svg>...</svg>">
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
                            <option value="copy_text">Copy Text</option>
                            <option value="http_check">HTTP Health Check</option>
                            <option value="ping">Ping Host</option>
                            <option value="docker_command">Docker Command</option>
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
                <h3>Profiles</h3>
            </div>
            <div class="panel-content">
                <div id="profiles-list" class="profiles-list">
                    <!-- Profiles will be populated here -->
                </div>
                <div class="panel-actions">
                    <button id="btn-new-profile" class="btn btn-block">+ New Profile</button>
                </div>
            </div>
        `;
        return panel;
    }

    attachEventListeners() {
        // Toggle mode
        document.getElementById('btn-toggle-mode')?.addEventListener('click', () => {
            this.toggleMode();
        });

        // Deck size change
        document.getElementById('select-deck-size')?.addEventListener('change', (e) => {
            this.deck.resize(e.target.value);
        });

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

        // Initial profiles list
        this.refreshProfilesList();
    }

    toggleMode() {
        this.editorMode = this.editorMode === 'player' ? 'studio' : 'player';
        const studioContainer = document.getElementById('studio-container');
        const toggleBtn = document.getElementById('btn-toggle-mode');

        if (this.editorMode === 'studio') {
            studioContainer.classList.remove('studio-hidden');
            toggleBtn.classList.add('active');
            toggleBtn.querySelector('.label').textContent = 'Player';
        } else {
            studioContainer.classList.add('studio-hidden');
            toggleBtn.classList.remove('active');
            toggleBtn.querySelector('.label').textContent = 'Studio';
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

    updateActionConfig(actionType, existingAction = null) {
        const configContainer = document.getElementById('action-config');
        configContainer.innerHTML = '';

        const configs = {
            'open_url': `
                <div class="form-group">
                    <label>URL</label>
                    <input type="text" id="action-url" class="form-control" placeholder="https://example.com" value="${existingAction?.url || ''}">
                </div>
            `,
            'open_app': `
                <div class="form-group">
                    <label>Application</label>
                    <input type="text" id="action-app" class="form-control" placeholder="code, chrome.exe, notepad" value="${existingAction?.app || ''}">
                </div>
            `,
            'run_command': `
                <div class="form-group">
                    <label>Command</label>
                    <textarea id="action-command" class="form-control" rows="3" placeholder="dir">${existingAction?.command || ''}</textarea>
                </div>
                <div class="form-group">
                    <label>Shell</label>
                    <select id="action-shell" class="form-control">
                        <option value="powershell" ${existingAction?.shell === 'powershell' ? 'selected' : ''}>PowerShell</option>
                        <option value="cmd" ${existingAction?.shell === 'cmd' ? 'selected' : ''}>CMD</option>
                    </select>
                </div>
            `,
            'copy_text': `
                <div class="form-group">
                    <label>Text to Copy</label>
                    <textarea id="action-text" class="form-control" rows="3" placeholder="Text to copy...">${existingAction?.text || ''}</textarea>
                </div>
            `,
            'http_check': `
                <div class="form-group">
                    <label>URL to Check</label>
                    <input type="text" id="action-url" class="form-control" placeholder="http://localhost:3000" value="${existingAction?.url || ''}">
                </div>
            `,
            'ping': `
                <div class="form-group">
                    <label>Host</label>
                    <input type="text" id="action-host" class="form-control" placeholder="8.8.8.8 or google.com" value="${existingAction?.host || '8.8.8.8'}">
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
                    <input type="text" id="action-container" class="form-control" placeholder="container-name" value="${existingAction?.container || ''}">
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
                        <option value="ping_monitor">Ping Monitor</option>
                        <option value="uptime">Uptime</option>
                    </select>
                </div>
            `
        };

        if (configs[actionType]) {
            configContainer.innerHTML = configs[actionType];
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
            action = this.buildActionFromInputs(actionType);
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
            this.liveKeys.registerWidget(this.selectedButton.index, widgetType);
        } else {
            this.liveKeys.unregisterWidget(this.selectedButton.index);
        }

        this.showToast('Changes applied');
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
            'copy_text': () => ({ type: 'copy_text', text: document.getElementById('action-text').value }),
            'http_check': () => ({ type: 'http_check', url: document.getElementById('action-url').value }),
            'ping': () => ({ type: 'ping', host: document.getElementById('action-host').value }),
            'docker_command': () => ({
                type: 'docker_command',
                dockerAction: document.getElementById('action-docker-action').value,
                container: document.getElementById('action-container').value
            })
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
