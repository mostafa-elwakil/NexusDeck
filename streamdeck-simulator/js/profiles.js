/**
 * StreamDeck Profiles Manager
 * Handles profile loading, saving, import/export, and LocalStorage persistence
 */

class ProfilesManager {
    constructor(deckCore, actionsEngine, liveKeysManager) {
        this.deck = deckCore;
        this.actions = actionsEngine;
        this.liveKeys = liveKeysManager;
        this.currentProfile = null;
        this.profiles = [];
        this.storageKey = 'streamdeck_profiles';
        this.currentProfileKey = 'streamdeck_current_profile';

        this.loadProfiles();
    }

    loadProfiles() {
        try {
            const defaults = this.getDefaultProfiles();
            const stored = localStorage.getItem(this.storageKey);
            if (stored) {
                const parsed = JSON.parse(stored);
                this.profiles = defaults.map(def => {
                    const existing = parsed.find(p => p.name === def.name);
                    return existing ? { ...existing, backgroundColor: existing.backgroundColor || '#000000', size: 'cyd', rows: 3, cols: 4 } : def;
                });
                parsed.forEach(p => {
                    if (!defaults.some(d => d.name === p.name)) {
                        this.profiles.push(p);
                    }
                });
                this.saveProfiles();
            } else {
                this.profiles = defaults;
                this.saveProfiles();
            }
            this.migrateCpuRamWidgets();

            // Load last active profile
            const currentProfileName = localStorage.getItem(this.currentProfileKey);
            if (currentProfileName) {
                const profile = this.getProfile(currentProfileName);
                if (profile) {
                    this.loadProfile(profile);
                }
            } else if (this.profiles.length > 0) {
                this.loadProfile(this.profiles[0]);
            }
        } catch (error) {
            console.error('Failed to load profiles:', error);
            this.profiles = this.getDefaultProfiles();
        }
    }

    saveProfiles() {
        try {
            localStorage.setItem(this.storageKey, JSON.stringify(this.profiles));
        } catch (error) {
            console.error('Failed to save profiles:', error);
        }
    }

    migrateCpuRamWidgets() {
        let changed = false;

        this.profiles.forEach((profile) => {
            if (!Array.isArray(profile.buttons)) return;

            const cpuRamIndex = profile.buttons.findIndex(
                (button) => button?.widget?.type === 'cpu_ram'
            );
            if (cpuRamIndex < 0) return;

            const oldButtons = profile.buttons.slice();
            const cpuButton = {
                label: '', icon: '🔥', color: '#8a3d20', widget: { type: 'cpu' }
            };
            const ramButton = {
                label: '', icon: '💾', color: '#245c52', widget: { type: 'ram' }
            };

            profile.buttons[cpuRamIndex] = cpuButton;
            if (cpuRamIndex + 1 < profile.buttons.length) {
                profile.buttons[cpuRamIndex + 1] = ramButton;
            }

            for (let index = cpuRamIndex + 2; index < profile.buttons.length; index++) {
                const sourceIndex = index - 1;
                if (sourceIndex !== cpuRamIndex) {
                    profile.buttons[index] = oldButtons[sourceIndex];
                }
            }

            changed = true;
        });

        if (changed) this.saveProfiles();
    }

    loadProfile(profile) {
        if (typeof profile === 'string') {
            profile = this.getProfile(profile);
        }

        if (!profile) {
            console.error('Profile not found');
            return false;
        }

        this.currentProfile = profile;

        // Stop all live widgets
        this.liveKeys.stopAll();

        // Apply deck size
        if (profile.size && profile.size !== this.deck.config.size) {
            this.deck.resize(profile.size);
        }

        // Load buttons
        if (profile.buttons && Array.isArray(profile.buttons)) {
            profile.buttons.forEach((btnConfig, index) => {
                if (index < this.deck.getButtonCount()) {
                    this.deck.updateButton(index, {
                        label: btnConfig.label || '',
                        icon: btnConfig.icon || '',
                        color: btnConfig.color || '#1a1a2e',
                        action: btnConfig.action || null
                    });

                    // Register live widget if specified
                    if (btnConfig.widget) {
                        this.liveKeys.registerWidget(index, btnConfig.widget.type, btnConfig.widget.config || {});
                    }
                }
            });
        }

        // Save as current profile
        localStorage.setItem(this.currentProfileKey, profile.name);

        this.dispatchEvent('profileLoaded', { profile });
        this.syncProfileToServer(profile);
        return true;
    }

    async syncProfileToServer(profile = this.currentProfile) {
        if (!profile || !this.actions?.serverUrl) return;

        try {
            const response = await fetch(`${this.actions.serverUrl}/api/set-profile`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(profile)
            });

            if (!response.ok) {
                throw new Error(`Profile sync failed with HTTP ${response.status}`);
            }
        } catch (error) {
            console.error('Failed to sync profile with ESP32 server:', error);
        }
    }

    saveCurrentProfile() {
        if (!this.currentProfile) {
            console.error('No active profile to save');
            return false;
        }

        // Export current deck state
        const deckState = this.deck.exportProfile();

        // Update profile with current state
        this.currentProfile.size = deckState.size;
        this.currentProfile.rows = deckState.rows;
        this.currentProfile.cols = deckState.cols;
        this.currentProfile.backgroundColor = this.currentProfile.backgroundColor || '#000000';
        this.currentProfile.buttons = deckState.buttons;

        // Add live widget info (including widget config, e.g. pomodoro durations)
        const activeWidgets = this.liveKeys.getActiveWidgets();
        activeWidgets.forEach(({ buttonIndex, type, config }) => {
            if (this.currentProfile.buttons[buttonIndex]) {
                this.currentProfile.buttons[buttonIndex].widget = config ? { type, config } : { type };
            }
        });

        // Update in profiles list
        const index = this.profiles.findIndex(p => p.name === this.currentProfile.name);
        if (index >= 0) {
            this.profiles[index] = this.currentProfile;
        }

        this.saveProfiles();
        this.syncProfileToServer(this.currentProfile);
        this.dispatchEvent('profileSaved', { profile: this.currentProfile });
        return true;
    }

    createProfile(name, basedOn = null) {
        if (this.getProfile(name)) {
            console.error('Profile already exists:', name);
            return null;
        }

        let profile;
        if (basedOn) {
            const baseProfile = this.getProfile(basedOn);
            if (baseProfile) {
                profile = JSON.parse(JSON.stringify(baseProfile));
                profile.name = name;
            }
        }

        if (!profile) {
            profile = {
                name: name,
                size: this.deck.config.size,
                rows: this.deck.config.rows,
                cols: this.deck.config.cols,
                backgroundColor: '#000000',
                buttons: Array(this.deck.getButtonCount()).fill(null).map(() => ({
                    label: '',
                    icon: '',
                    color: '#1a1a2e',
                    action: null
                }))
            };
        }

        this.profiles.push(profile);
        this.saveProfiles();
        this.dispatchEvent('profileCreated', { profile });
        return profile;
    }

    deleteProfile(name) {
        const index = this.profiles.findIndex(p => p.name === name);
        if (index < 0) {
            console.error('Profile not found:', name);
            return false;
        }

        this.profiles.splice(index, 1);
        this.saveProfiles();

        // If deleting current profile, load another
        if (this.currentProfile && this.currentProfile.name === name) {
            if (this.profiles.length > 0) {
                this.loadProfile(this.profiles[0]);
            } else {
                this.currentProfile = null;
            }
        }

        this.dispatchEvent('profileDeleted', { name });
        return true;
    }

    renameProfile(oldName, newName) {
        const profile = this.getProfile(oldName);
        if (!profile) {
            console.error('Profile not found:', oldName);
            return false;
        }

        if (this.getProfile(newName)) {
            console.error('Profile name already exists:', newName);
            return false;
        }

        profile.name = newName;
        this.saveProfiles();

        if (this.currentProfile && this.currentProfile.name === oldName) {
            this.currentProfile.name = newName;
            localStorage.setItem(this.currentProfileKey, newName);
        }

        this.dispatchEvent('profileRenamed', { oldName, newName });
        return true;
    }

    getProfile(name) {
        return this.profiles.find(p => p.name === name);
    }

    getAllProfiles() {
        return [...this.profiles];
    }

    exportProfile(name) {
        const profile = name ? this.getProfile(name) : this.currentProfile;
        if (!profile) {
            console.error('Profile not found');
            return null;
        }

        const json = JSON.stringify(profile, null, 2);
        return json;
    }

    exportProfileToFile(name) {
        const json = this.exportProfile(name);
        if (!json) return;

        const profile = name ? this.getProfile(name) : this.currentProfile;
        const blob = new Blob([json], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${profile.name.replace(/\s+/g, '_')}.json`;
        a.click();
        URL.revokeObjectURL(url);
    }

    importProfile(jsonString) {
        try {
            const profile = JSON.parse(jsonString);

            // Validate profile structure
            if (!profile.name || !profile.buttons) {
                throw new Error('Invalid profile format');
            }

            // Check if name exists
            let finalName = profile.name;
            let counter = 1;
            while (this.getProfile(finalName)) {
                finalName = `${profile.name} (${counter})`;
                counter++;
            }
            profile.name = finalName;

            this.profiles.push(profile);
            this.saveProfiles();
            this.dispatchEvent('profileImported', { profile });
            return profile;
        } catch (error) {
            console.error('Failed to import profile:', error);
            return null;
        }
    }

    importProfileFromFile(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => {
                const profile = this.importProfile(e.target.result);
                if (profile) {
                    resolve(profile);
                } else {
                    reject(new Error('Failed to import profile'));
                }
            };
            reader.onerror = () => reject(new Error('Failed to read file'));
            reader.readAsText(file);
        });
    }

    getDefaultProfiles() {
        return [
            {
                name: 'DevOps Dashboard',
                size: 'cyd',
                rows: 3,
                cols: 4,
                backgroundColor: '#000000',
                buttons: [
                    { label: 'VS Code', icon: '💻', color: '#007acc', action: { type: 'open_app', app: 'code' } },
                    { label: 'Terminal', icon: '⚡', color: '#1a1a2e', action: { type: 'open_app', app: 'wt.exe' } },
                    { label: 'Docker', icon: '🐳', color: '#2496ed', action: { type: 'open_url', url: 'http://localhost:9000' } },
                    { label: 'GitHub', icon: '🐙', color: '#24292e', action: { type: 'open_url', url: 'https://github.com' } },

                    { label: 'Localhost', icon: '🌐', color: '#2d5f4d', action: { type: 'http_check', url: 'http://localhost:3000' } },
                    { label: 'Ping 8.8.8.8', icon: '📡', color: '#1a3e2e', action: { type: 'ping', host: '8.8.8.8' } },
                    { label: '', icon: '🕐', color: '#1a1a3e', widget: { type: 'clock' } },
                    { label: '', icon: '🔥', color: '#8a3d20', widget: { type: 'cpu' } },

                    { label: '', icon: '💾', color: '#245c52', widget: { type: 'ram' } },
                    { label: 'Docker PS', icon: '📋', color: '#1a2e3e', action: { type: 'docker_command', dockerAction: 'status', container: '*' } },
                    { label: 'Clear Logs', icon: '🗑️', color: '#3e1a1a', action: { type: 'run_command', command: 'Remove-Item logs\\*.log', shell: 'powershell' } },
                    { label: 'Uptime', icon: '⏱️', color: '#2d4d6f', widget: { type: 'uptime' } }
                ]
            },
            {
                name: 'Media Control',
                size: 'cyd',
                rows: 3,
                cols: 4,
                backgroundColor: '#000000',
                buttons: [
                    { label: 'Spotify', icon: '🎵', color: '#1db954', action: { type: 'open_url', url: 'https://open.spotify.com' } },
                    { label: 'YouTube', icon: '📺', color: '#ff0000', action: { type: 'open_url', url: 'https://youtube.com' } },
                    { label: 'VLC', icon: '🎥', color: '#ff8800', action: { type: 'open_app', app: 'vlc.exe' } },
                    { label: 'Rec Toggle', icon: '⏺️', color: '#ea4335', action: { type: 'obs_control', operation: 'toggle_recording' } },

                    { label: 'Scene: Game', icon: '🎮', color: '#2496ed', action: { type: 'obs_control', operation: 'set_scene', scene: 'Game' } },
                    { label: '', icon: '🕐', color: '#1a1a3e', widget: { type: 'clock' } },
                    { label: '', icon: '⏱️', color: '#1a2e3e', widget: { type: 'stopwatch' } },
                    { label: '', icon: '⏲️', color: '#2d2d6f', widget: { type: 'timer', config: { duration: 300 } } },

                    { label: 'Browser', icon: '🌐', color: '#4285f4', action: { type: 'open_url', url: 'https://google.com' } },
                    { label: 'Discord', icon: '💬', color: '#7289da', action: { type: 'open_app', app: 'Discord.exe' } },
                    { label: 'Notepad', icon: '📝', color: '#333333', action: { type: 'open_app', app: 'notepad.exe' } },
                    { label: 'Switch Profile', icon: '🔄', color: '#5cb5f0', action: { type: 'switch_profile' } }
                ]
            },
            {
                name: 'OBS Studio',
                size: 'cyd',
                rows: 3,
                cols: 4,
                backgroundColor: '#000000',
                buttons: [
                    { label: 'OBS', icon: '🎥', color: '#302e31', action: { type: 'open_app', app: 'obs64.exe' } },
                    { label: 'Scene: Game', icon: '🎮', color: '#2496ed', action: { type: 'obs_control', operation: 'set_scene', scene: 'Game' } },
                    { label: 'Scene: Chat', icon: '💬', color: '#6264a7', action: { type: 'obs_control', operation: 'set_scene', scene: 'Chatting' } },
                    { label: 'Scene: Desktop', icon: '🖥️', color: '#007acc', action: { type: 'obs_control', operation: 'set_scene', scene: 'Desktop' } },

                    { label: 'Camera On', icon: '📷', color: '#1db954', action: { type: 'obs_control', operation: 'set_source_visibility', scene: 'Game', source: 'Camera', visible: true } },
                    { label: 'Camera Off', icon: '🚫', color: '#3e1a1a', action: { type: 'obs_control', operation: 'set_source_visibility', scene: 'Game', source: 'Camera', visible: false } },
                    { label: 'Start Rec', icon: '⏺️', color: '#ff0000', action: { type: 'obs_control', operation: 'start_recording' } },
                    { label: 'Stop Rec', icon: '⏹️', color: '#8a1a1a', action: { type: 'obs_control', operation: 'stop_recording' } },

                    { label: 'Rec Toggle', icon: '⏯️', color: '#ea4335', action: { type: 'obs_control', operation: 'toggle_recording' } },
                    { label: '', icon: '🕐', color: '#1a1a3e', widget: { type: 'clock' } },
                    { label: '', icon: '⏱️', color: '#1a2e3e', widget: { type: 'stopwatch' } },
                    { label: 'Timer 5m', icon: '⏲️', color: '#2d2d6f', widget: { type: 'timer', config: { duration: 300 } } }
                ]
            },
            {
                name: 'Productivity',
                size: 'cyd',
                rows: 3,
                cols: 4,
                backgroundColor: '#000000',
                buttons: [
                    { label: 'Gmail', icon: '📧', color: '#ea4335', action: { type: 'open_url', url: 'https://mail.google.com' } },
                    { label: 'Calendar', icon: '📅', color: '#4285f4', action: { type: 'open_url', url: 'https://calendar.google.com' } },
                    { label: 'Notion', icon: '📝', color: '#000000', action: { type: 'open_url', url: 'https://notion.so' } },
                    { label: 'Slack', icon: '💬', color: '#4a154b', action: { type: 'open_url', url: 'https://slack.com' } },

                    { label: 'Chrome', icon: '🌐', color: '#4285f4', action: { type: 'open_app', app: 'chrome.exe' } },
                    { label: 'Excel', icon: '📊', color: '#217346', action: { type: 'open_app', app: 'excel.exe' } },
                    { label: 'Word', icon: '📄', color: '#2b579a', action: { type: 'open_app', app: 'winword.exe' } },
                    { label: 'Teams', icon: '👥', color: '#6264a7', action: { type: 'open_app', app: 'teams.exe' } },

                    { label: 'Screenshot', icon: '📸', color: '#1a1a2e', action: { type: 'run_command', command: 'snippingtool', shell: 'cmd' } },
                    { label: 'Calculator', icon: '🔢', color: '#2d2d5f', action: { type: 'open_app', app: 'calc.exe' } },
                    { label: 'Notepad', icon: '📝', color: '#1a2e1a', action: { type: 'open_app', app: 'notepad.exe' } },
                    { label: 'Switch Profile', icon: '🔄', color: '#5cb5f0', action: { type: 'switch_profile' } }
                ]
            }
        ];
    }

    dispatchEvent(eventName, detail) {
        const event = new CustomEvent(`profile:${eventName}`, { detail });
        this.deck.container.dispatchEvent(event);
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ProfilesManager;
}
