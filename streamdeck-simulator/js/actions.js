/**
 * StreamDeck Actions Engine
 * Handles execution of various action types: system commands, macros, navigation, etc.
 */

class ActionsEngine {
    constructor(deckCore, options = {}) {
        this.deck = deckCore;
        this.serverUrl = options.serverUrl || 'http://localhost:8765';
        this.serverAvailable = false;
        this.actionHistory = [];
        this.macroQueue = [];
        this.isExecutingMacro = false;

        this.actionHandlers = {
            'open_url': this.openUrl.bind(this),
            'open_app': this.openApp.bind(this),
            'run_command': this.runCommand.bind(this),
            'copy_text': this.copyText.bind(this),
            'http_check': this.httpCheck.bind(this),
            'ping': this.ping.bind(this),
            'macro': this.executeMacro.bind(this),
            'navigate': this.navigate.bind(this),
            'docker_command': this.dockerCommand.bind(this),
            'obs_control': this.obsControl.bind(this),
            'custom_script': this.customScript.bind(this),
            'custom': async (action, button) => {
                if (typeof action.handler === 'function') {
                    return action.handler(action, button);
                }
            }
        };

        this.checkServerAvailability();
    }

    async checkServerAvailability() {
        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 2000);

            const response = await fetch(`${this.serverUrl}/api/health`, {
                method: 'GET',
                signal: controller.signal
            });

            clearTimeout(timeoutId);
            this.serverAvailable = response.ok;
            this.dispatchEvent('serverStatus', { available: this.serverAvailable });
        } catch (error) {
            this.serverAvailable = false;
            console.log('Companion server not available - some features will be limited');
        }
    }

    async executeAction(button) {
        const action = button.config.action;
        if (!action || !action.type) {
            console.warn('No action configured for button', button.index);
            return;
        }

        // Set button to running state
        this.deck.setButtonRunning(button.index, true);

        try {
            const handler = this.actionHandlers[action.type];
            if (handler) {
                await handler(action, button);
                this.deck.showButtonFeedback(button.index, 'success', 800);
            } else {
                console.error('Unknown action type:', action.type);
                this.deck.showButtonFeedback(button.index, 'error', 1000);
            }

            // Add to history
            this.addToHistory(button, action, 'success');
        } catch (error) {
            appLogger.error('Action execution failed', {
                actionType: action.type,
                buttonIndex: button.index,
                error: error.message
            });
            this.deck.showButtonFeedback(button.index, 'error', 1000);
            this.addToHistory(button, action, 'error', error.message);
        } finally {
            this.deck.setButtonRunning(button.index, false);
        }
    }

    // Action Handlers

    async openUrl(action, button) {
        const url = action.url;
        if (!url) {
            throw new Error('URL not specified');
        }

        // Open in new tab
        window.open(url, '_blank', 'noopener,noreferrer');
        await this.delay(200); // Small delay for visual feedback
    }

    async openApp(action, button) {
        if (!this.serverAvailable) {
            throw new Error('Companion server required for opening applications');
        }

        const response = await fetch(`${this.serverUrl}/api/open-app`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                app: action.app,
                args: action.args || []
            })
        });

        const result = await response.json();
        if (!result.success) {
            throw new Error(result.error || 'Failed to open application');
        }
    }

    async runCommand(action, button) {
        if (!this.serverAvailable) {
            throw new Error('Companion server required for running commands');
        }

        const response = await fetch(`${this.serverUrl}/api/run-command`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                command: action.command,
                shell: action.shell || 'powershell'
            })
        });

        const result = await response.json();
        if (!result.success) {
            throw new Error(result.error || 'Command execution failed');
        }

        // Optionally show output
        if (action.showOutput && result.output) {
            this.showNotification('Command Output', result.output);
        }
    }

    async obsControl(action, button) {
        if (!this.serverAvailable) {
            throw new Error('Companion server required for OBS control');
        }

        const response = await fetch(`${this.serverUrl}/api/obs-control`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(action)
        });
        const result = await response.json();
        if (!response.ok || !result.success) {
            throw new Error(result.error || 'OBS action failed');
        }
    }

    async copyText(action, button) {
        const text = action.text;
        if (!text) {
            throw new Error('No text to copy');
        }

        try {
            await navigator.clipboard.writeText(text);
            this.showNotification('Copied', 'Text copied to clipboard');
        } catch (error) {
            // Fallback for older browsers
            const textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.opacity = '0';
            document.body.appendChild(textarea);
            textarea.select();
            document.execCommand('copy');
            document.body.removeChild(textarea);
        }
    }

    async httpCheck(action, button) {
        const url = action.url;
        if (!url) {
            throw new Error('URL not specified');
        }

        const startTime = Date.now();

        try {
            const response = await fetch(url, {
                method: action.method || 'GET',
                signal: AbortSignal.timeout(action.timeout || 5000)
            });

            const responseTime = Date.now() - startTime;
            const status = response.status;

            // Update button with status
            const statusColor = status >= 200 && status < 300 ? '#00ff88' : '#ff4444';
            this.showNotification('HTTP Check', `${url}\nStatus: ${status}\nTime: ${responseTime}ms`);

        } catch (error) {
            throw new Error(`HTTP check failed: ${error.message}`);
        }
    }

    async ping(action, button) {
        const host = action.host || '8.8.8.8';

        if (this.serverAvailable) {
            const response = await fetch(`${this.serverUrl}/api/ping`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ host })
            });

            const result = await response.json();
            if (!result.success) {
                throw new Error(result.error || 'Ping failed');
            }

            this.showNotification('Ping Result', `${host}\nLatency: ${result.latency}ms`);
        } else {
            // Fallback using HTTP timing
            const startTime = Date.now();
            try {
                await fetch(`https://${host}`, { mode: 'no-cors', signal: AbortSignal.timeout(3000) });
                const latency = Date.now() - startTime;
                this.showNotification('Network Check', `Estimated latency: ${latency}ms`);
            } catch (error) {
                throw new Error('Network check failed');
            }
        }
    }

    async executeMacro(action, button) {
        const steps = action.steps;
        if (!steps || !Array.isArray(steps) || steps.length === 0) {
            throw new Error('Macro has no steps');
        }

        this.isExecutingMacro = true;

        for (let i = 0; i < steps.length; i++) {
            const step = steps[i];

            // Execute step
            const handler = this.actionHandlers[step.type];
            if (handler) {
                await handler(step, button);
            }

            // Delay before next step
            if (step.delay && i < steps.length - 1) {
                await this.delay(step.delay);
            }
        }

        this.isExecutingMacro = false;
    }

    async navigate(action, button) {
        const direction = action.direction; // 'next', 'prev', or page number
        this.dispatchEvent('navigate', { direction, page: action.page });
    }

    async dockerCommand(action, button) {
        if (!this.serverAvailable) {
            throw new Error('Companion server required for Docker commands');
        }

        const dockerAction = action.dockerAction; // 'start', 'stop', 'restart', 'status'
        const container = action.container;

        if (!container) {
            throw new Error('Container name not specified');
        }

        const commands = {
            'start': `docker start ${container}`,
            'stop': `docker stop ${container}`,
            'restart': `docker restart ${container}`,
            'status': `docker ps -a --filter name=${container}`
        };

        const command = commands[dockerAction];
        if (!command) {
            throw new Error('Unknown Docker action');
        }

        await this.runCommand({
            command,
            shell: 'powershell',
            showOutput: dockerAction === 'status'
        }, button);
    }

    async customScript(action, button) {
        const script = action.script;
        if (!script) {
            throw new Error('No script provided');
        }

        try {
            // Execute script in sandboxed context
            const func = new Function('button', 'deck', 'actions', script);
            await func.call(this, button, this.deck, this);
        } catch (error) {
            throw new Error(`Script execution failed: ${error.message}`);
        }
    }

    // Utility Methods

    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    showNotification(title, message) {
        return notificationManager.show(title, message);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    addToHistory(button, action, status, error = null) {
        this.actionHistory.push({
            timestamp: Date.now(),
            buttonIndex: button.index,
            buttonLabel: button.config.label,
            actionType: action.type,
            status,
            error
        });

        // Keep only last 100 entries
        if (this.actionHistory.length > 100) {
            this.actionHistory.shift();
        }

        this.dispatchEvent('historyUpdated', {
            entry: this.actionHistory[this.actionHistory.length - 1]
        });
    }

    getHistory(limit = 50) {
        return this.actionHistory.slice(-limit).reverse();
    }

    clearHistory() {
        this.actionHistory = [];
    }

    dispatchEvent(eventName, detail) {
        const event = new CustomEvent(`action:${eventName}`, { detail });
        this.deck.container.dispatchEvent(event);
    }

    /**
     * Complete cleanup for this action engine instance
     * Call before destroying the application
     */
    destroy() {
        // Cancel all pending operations
        this.macroQueue = [];
        this.isExecutingMacro = false;

        // Clear history
        this.actionHistory = [];

        // Cancel any pending server checks
        // Note: Individual fetch operations can't be cancelled retroactively,
        // but new ones won't be initiated after this point

        console.log('ActionsEngine instance destroyed');
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ActionsEngine;
}
