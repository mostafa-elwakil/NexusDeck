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
        this.theme = localStorage.getItem('streamdeck_theme') || 'dark';

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
            <div id="server-status" class="status-pill is-offline" role="status">Companion offline</div>
        `;
        document.body.appendChild(footer);

        const studioContainer = document.createElement('div');
        studioContainer.id = 'studio-container';
        studioContainer.className = 'studio-hidden';
        studioContainer.appendChild(this.createInspector());
        studioContainer.appendChild(this.createProfilesPanel());
        document.body.appendChild(studioContainer);
    }

    createToolbar() {
        const toolbar = document.createElement('header');
        toolbar.className = 'studio-toolbar app-header';
        toolbar.innerHTML = `
            <div class="toolbar-section brand-section">
                <div class="app-brand">
                    <span class="brand-mark">OD</span>
                    <div class="brand-copy">
                        <strong>OpsDeck</strong>
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
                <button id="btn-toggle-theme" class="toolbar-btn" title="Switch to light mode" aria-label="Switch to light mode">
                    <span class="icon">☀</span>
                    <span class="label">Day</span>
                </button>
            </div>
            <div class="toolbar-section">
                <label for="select-deck-size">Size</label>
                <select id="select-deck-size" class="toolbar-select">
                    <option value="mini">Mini 3×2</option>
                    <option value="cyd" selected>CYD 4×3</option>
                    <option value="classic">Classic 5×3</option>
                    <option value="xl">XL 8×4</option>
                </select>
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
                            <option value="obs_control">OBS Control</option>
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

        this.deck.container.addEventListener('action:serverStatus', (e) => {
            this.updateServerStatus(e.detail.available);
        });

        this.refreshProfilesList();
    }

    updateServerStatus(available) {
        const pill = document.getElementById('server-status');
        if (!pill) return;
        pill.classList.toggle('is-online', !!available);
        pill.classList.toggle('is-offline', !available);
        pill.textContent = available ? 'Companion online' : 'Companion offline';
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
                    <input type="text" id="action-obs-scene" class="form-control" placeholder="e.g. Gaming Scene" value="${existingAction?.scene || ''}">
                </div>
                <div class="form-group" id="obs-source-group">
                    <label>Source Name</label>
                    <input type="text" id="action-obs-source" class="form-control" placeholder="e.g. Camera" value="${existingAction?.source || ''}">
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
                    <input type="text" id="action-obs-host" class="form-control" placeholder="Host (default: 127.0.0.1)" value="${existingAction?.host || ''}" style="margin-bottom: 6px;">
                    <input type="text" id="action-obs-port" class="form-control" placeholder="Port (default: 4455)" value="${existingAction?.port || ''}" style="margin-bottom: 6px;">
                    <input type="password" id="action-obs-password" class="form-control" placeholder="Password (if set in OBS)" value="${existingAction?.password || ''}">
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

        if (actionType === 'obs_control') {
            this.setupObsConfig(existingAction);
        }
    }

    setupObsConfig(existingAction = null) {
        const operationSelect = document.getElementById('action-obs-operation');
        if (!operationSelect) return;

        // Restore previously saved operation when editing an existing button
        if (existingAction?.operation) {
            operationSelect.value = existingAction.operation;
        }

        operationSelect.addEventListener('change', () => this.updateObsFieldVisibility());
        this.updateObsFieldVisibility();

        document.getElementById('btn-obs-test')?.addEventListener('click', () => {
            this.testObsConnection();
        });
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
