/**
 * StreamDeck Live Widgets
 * Dynamic buttons that update automatically: CPU/RAM monitor, clock, stopwatch, ping, etc.
 */

class LiveKeysManager {
    constructor(deckCore, actionsEngine) {
        this.deck = deckCore;
        this.actions = actionsEngine;
        this.activeWidgets = new Map();
        this.updateIntervals = new Map();

        this.widgetTypes = {
            'clock': this.createClockWidget.bind(this),
            'cpu_ram': this.createCpuRamWidget.bind(this),
            'stopwatch': this.createStopwatchWidget.bind(this),
            'timer': this.createTimerWidget.bind(this),
            'pomodoro': this.createPomodoroWidget.bind(this),
            'ping_monitor': this.createPingWidget.bind(this),
            'date': this.createDateWidget.bind(this),
            'uptime': this.createUptimeWidget.bind(this)
        };
    }

    registerWidget(buttonIndex, widgetType, config = {}) {
        // Stop existing widget if any
        this.unregisterWidget(buttonIndex);

        const creator = this.widgetTypes[widgetType];
        if (!creator) {
            console.error('Unknown widget type:', widgetType);
            return false;
        }

        const widget = creator(buttonIndex, config);
        this.activeWidgets.set(buttonIndex, widget);

        // Start widget updates
        if (widget.updateInterval) {
            const intervalId = setInterval(() => {
                widget.update();
            }, widget.updateInterval);
            this.updateIntervals.set(buttonIndex, intervalId);
        }

        // Initial update
        if (widget.update) {
            widget.update();
        }

        return true;
    }

    unregisterWidget(buttonIndex) {
        const widget = this.activeWidgets.get(buttonIndex);
        if (widget && widget.cleanup) {
            widget.cleanup();
        }

        const intervalId = this.updateIntervals.get(buttonIndex);
        if (intervalId) {
            clearInterval(intervalId);
            this.updateIntervals.delete(buttonIndex);
        }

        this.activeWidgets.delete(buttonIndex);
    }

    // Clock Widget (HH:MM:SS)
    createClockWidget(buttonIndex, config) {
        const format24h = config.format24h !== false;
        const showSeconds = config.showSeconds !== false;

        return {
            type: 'clock',
            updateInterval: 1000,
            update: () => {
                const now = new Date();
                let hours = now.getHours();
                const minutes = now.getMinutes().toString().padStart(2, '0');
                const seconds = now.getSeconds().toString().padStart(2, '0');

                let timeStr;
                if (format24h) {
                    timeStr = `${hours.toString().padStart(2, '0')}:${minutes}`;
                } else {
                    const ampm = hours >= 12 ? 'PM' : 'AM';
                    hours = hours % 12 || 12;
                    timeStr = `${hours}:${minutes} ${ampm}`;
                }

                if (showSeconds) {
                    timeStr += format24h ? `:${seconds}` : `:${seconds}`;
                }

                this.deck.updateButton(buttonIndex, {
                    label: timeStr,
                    icon: '🕐',
                    color: this.getGradient('#1a1a3e', '#2d2d5f')
                });
            }
        };
    }

    // Date Widget
    createDateWidget(buttonIndex, config) {
        const format = config.format || 'short'; // 'short', 'long', 'numeric'

        return {
            type: 'date',
            updateInterval: 60000, // Update every minute
            update: () => {
                const now = new Date();
                let dateStr;

                switch (format) {
                    case 'long':
                        dateStr = now.toLocaleDateString('en-US', {
                            weekday: 'short',
                            month: 'short',
                            day: 'numeric'
                        });
                        break;
                    case 'numeric':
                        dateStr = now.toLocaleDateString('en-US');
                        break;
                    default:
                        dateStr = now.toLocaleDateString('en-US', {
                            month: 'short',
                            day: 'numeric'
                        });
                }

                this.deck.updateButton(buttonIndex, {
                    label: dateStr,
                    icon: '📅',
                    color: this.getGradient('#1a3e2e', '#2d5f4d')
                });
            }
        };
    }

    // CPU & RAM Monitor
    createCpuRamWidget(buttonIndex, config) {
        let cpuUsage = 0;
        let ramUsage = 0;

        return {
            type: 'cpu_ram',
            updateInterval: 2000,
            update: async () => {
                try {
                    if (this.actions.serverAvailable) {
                        const controller = new AbortController();
                        const timeoutId = setTimeout(() => controller.abort(), 3000);

                        const response = await fetch(`${this.actions.serverUrl}/api/system-stats`, {
                            signal: controller.signal
                        });

                        clearTimeout(timeoutId);
                        const stats = await response.json();
                        cpuUsage = stats.cpu || 0;
                        ramUsage = stats.ram || 0;
                    } else {
                        // Fallback: simulate with random values for demo
                        cpuUsage = Math.random() * 100;
                        ramUsage = Math.random() * 100;
                    }

                    const cpuBar = this.createProgressBar(cpuUsage, 10);
                    const ramBar = this.createProgressBar(ramUsage, 10);

                    const label = `CPU ${cpuUsage.toFixed(0)}%\n${cpuBar}\nRAM ${ramUsage.toFixed(0)}%\n${ramBar}`;

                    // Color based on usage
                    let color;
                    const maxUsage = Math.max(cpuUsage, ramUsage);
                    if (maxUsage < 50) {
                        color = this.getGradient('#1a3e2e', '#2d6f4d');
                    } else if (maxUsage < 80) {
                        color = this.getGradient('#3e3e1a', '#6f6f2d');
                    } else {
                        color = this.getGradient('#3e1a1a', '#6f2d2d');
                    }

                    this.deck.updateButton(buttonIndex, {
                        label: label,
                        icon: '📊',
                        color: color
                    });
                } catch (error) {
                    console.error('Failed to update CPU/RAM widget:', error);
                }
            }
        };
    }

    // Stopwatch Widget
    createStopwatchWidget(buttonIndex, config) {
        let startTime = null;
        let elapsedTime = 0;
        let running = false;

        const widget = {
            type: 'stopwatch',
            updateInterval: 100,
            update: () => {
                let displayTime = elapsedTime;
                if (running && startTime) {
                    displayTime = Date.now() - startTime + elapsedTime;
                }

                const seconds = Math.floor(displayTime / 1000);
                const minutes = Math.floor(seconds / 60);
                const hours = Math.floor(minutes / 60);

                const timeStr = `${hours.toString().padStart(2, '0')}:${(minutes % 60).toString().padStart(2, '0')}:${(seconds % 60).toString().padStart(2, '0')}`;

                this.deck.updateButton(buttonIndex, {
                    label: timeStr,
                    icon: running ? '⏸️' : '▶️',
                    color: running ? this.getGradient('#1a2e3e', '#2d4d6f') : this.getGradient('#1a1a2e', '#2d2d5f')
                });
            },
            toggle: () => {
                if (running) {
                    elapsedTime += Date.now() - startTime;
                    running = false;
                    startTime = null;
                } else {
                    running = true;
                    startTime = Date.now();
                }
                widget.update();
            },
            reset: () => {
                running = false;
                startTime = null;
                elapsedTime = 0;
                widget.update();
            }
        };

        // Override button action to toggle stopwatch
        const button = this.deck.getButton(buttonIndex);
        if (button) {
            button.config.action = {
                type: 'custom',
                handler: () => widget.toggle()
            };
        }

        return widget;
    }

    // Timer Widget (Countdown)
    createTimerWidget(buttonIndex, config) {
        const duration = config.duration || 300; // Default 5 minutes in seconds
        let remainingTime = duration;
        let running = false;
        let startTime = null;

        const widget = {
            type: 'timer',
            updateInterval: 100,
            update: () => {
                if (running && startTime) {
                    const elapsed = Math.floor((Date.now() - startTime) / 1000);
                    remainingTime = Math.max(0, duration - elapsed);

                    if (remainingTime === 0) {
                        running = false;
                        this.playTimerAlert();
                    }
                }

                const minutes = Math.floor(remainingTime / 60);
                const seconds = remainingTime % 60;
                const timeStr = `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;

                let color;
                if (remainingTime === 0) {
                    color = this.getGradient('#3e1a1a', '#6f2d2d');
                } else if (remainingTime < 60) {
                    color = this.getGradient('#3e3e1a', '#6f6f2d');
                } else {
                    color = this.getGradient('#1a1a3e', '#2d2d6f');
                }

                this.deck.updateButton(buttonIndex, {
                    label: timeStr,
                    icon: running ? '⏸️' : '▶️',
                    color: color
                });
            },
            toggle: () => {
                if (running) {
                    running = false;
                } else {
                    if (remainingTime === 0) {
                        remainingTime = duration;
                    }
                    running = true;
                    startTime = Date.now();
                }
                widget.update();
            }
        };

        const button = this.deck.getButton(buttonIndex);
        if (button) {
            button.config.action = {
                type: 'custom',
                handler: () => widget.toggle()
            };
        }

        return widget;
    }

    // Pomodoro Widget (Focus / Short Break / Long Break cycles)
    // Controls: single press = start/pause, double press = reset session
    createPomodoroWidget(buttonIndex, config) {
        const workSec = Math.max(60, Math.round((config.workMinutes || 25) * 60));
        const shortSec = Math.max(60, Math.round((config.shortBreakMinutes || 5) * 60));
        const longSec = Math.max(60, Math.round((config.longBreakMinutes || 15) * 60));
        const sessionsBeforeLong = Math.max(1, Math.round(config.sessionsBeforeLong || 4));
        const autoStart = config.autoStart === true;

        let phase = 'work'; // 'work' | 'short' | 'long'
        let completedSessions = 0;
        let remaining = workSec;
        let running = false;
        let endAt = null;
        let lastClickAt = 0;
        let alertUntil = 0;

        const phaseDuration = (p) => p === 'work' ? workSec : (p === 'short' ? shortSec : longSec);
        const phaseName = (p) => p === 'work' ? 'FOCUS' : (p === 'short' ? 'SHORT' : 'LONG');
        const fmt = (s) => `${Math.floor(s / 60).toString().padStart(2, '0')}:${(s % 60).toString().padStart(2, '0')}`;
        const sessionDots = () => {
            let dots = '';
            for (let i = 0; i < sessionsBeforeLong; i++) {
                dots += i < (completedSessions % sessionsBeforeLong) ? '●' : '○';
            }
            return dots;
        };

        const widget = {
            type: 'pomodoro',
            config: {
                workMinutes: workSec / 60,
                shortBreakMinutes: shortSec / 60,
                longBreakMinutes: longSec / 60,
                sessionsBeforeLong,
                autoStart
            },
            updateInterval: 250,
            update: () => {
                if (running && endAt) {
                    remaining = Math.max(0, Math.ceil((endAt - Date.now()) / 1000));
                    if (remaining === 0) {
                        running = false;
                        endAt = null;
                        widget.advancePhase();
                        return;
                    }
                }

                const alerting = Date.now() < alertUntil;
                let color;
                if (alerting) {
                    color = this.getGradient('#3e1a1a', '#8a2d2d');
                } else if (phase === 'work') {
                    color = this.getGradient('#3e1a1a', '#6f2d2d');
                } else if (phase === 'short') {
                    color = this.getGradient('#1a3e2e', '#2d6f4d');
                } else {
                    color = this.getGradient('#1a2e3e', '#2d4d6f');
                }

                this.deck.updateButton(buttonIndex, {
                    label: `${phaseName(phase)} ${fmt(remaining)}\n${sessionDots()}`,
                    icon: running ? '⏸️' : (alerting ? '🔔' : '🍅'),
                    color: color
                });
            },
            advancePhase: () => {
                let message;
                if (phase === 'work') {
                    completedSessions++;
                    const isLong = completedSessions % sessionsBeforeLong === 0;
                    phase = isLong ? 'long' : 'short';
                    message = `Pomodoro ${completedSessions} done! Time for a ${isLong ? 'long' : 'short'} break.`;
                } else {
                    phase = 'work';
                    message = 'Break over! Back to focus.';
                }
                remaining = phaseDuration(phase);
                alertUntil = Date.now() + 5000;
                this.playTimerAlert();
                if (typeof notificationManager !== 'undefined' && notificationManager.show) {
                    notificationManager.show('Pomodoro', message, 'success');
                }
                if (this.deck.showButtonFeedback) {
                    this.deck.showButtonFeedback(buttonIndex, 'success', 1200);
                }
                if (autoStart) {
                    running = true;
                    endAt = Date.now() + remaining * 1000;
                }
                widget.update();
            },
            toggle: () => {
                const now = Date.now();
                if (now - lastClickAt < 450) {
                    lastClickAt = 0;
                    widget.reset();
                    return;
                }
                lastClickAt = now;
                if (running) {
                    remaining = Math.max(0, Math.ceil((endAt - Date.now()) / 1000));
                    running = false;
                    endAt = null;
                } else {
                    if (remaining <= 0) {
                        remaining = phaseDuration(phase);
                    }
                    running = true;
                    endAt = Date.now() + remaining * 1000;
                }
                widget.update();
            },
            reset: () => {
                phase = 'work';
                completedSessions = 0;
                remaining = workSec;
                running = false;
                endAt = null;
                alertUntil = 0;
                widget.update();
            }
        };

        const button = this.deck.getButton(buttonIndex);
        if (button) {
            button.config.action = {
                type: 'custom',
                handler: () => widget.toggle()
            };
        }

        return widget;
    }

    // Ping Monitor Widget
    createPingWidget(buttonIndex, config) {
        const host = config.host || '8.8.8.8';
        let latency = 0;
        let status = 'checking';

        return {
            type: 'ping_monitor',
            updateInterval: 5000,
            update: async () => {
                try {
                    const startTime = Date.now();

                    if (this.actions.serverAvailable) {
                        const controller = new AbortController();
                        const timeoutId = setTimeout(() => controller.abort(), 3000);

                        const response = await fetch(`${this.actions.serverUrl}/api/ping`, {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ host }),
                            signal: controller.signal
                        });

                        clearTimeout(timeoutId);
                        const result = await response.json();
                        latency = result.latency || 0;
                        status = result.success ? 'online' : 'offline';
                    } else {
                        // Fallback: HTTP timing with proper timeout
                        const controller = new AbortController();
                        const timeoutId = setTimeout(() => controller.abort(), 3000);

                        await fetch(`https://${host}`, {
                            mode: 'no-cors',
                            signal: controller.signal
                        });

                        clearTimeout(timeoutId);
                        latency = Date.now() - startTime;
                        status = 'online';
                    }

                    const label = `${host}\n${latency}ms`;
                    const color = status === 'online'
                        ? this.getGradient('#1a3e2e', '#2d6f4d')
                        : this.getGradient('#3e1a1a', '#6f2d2d');

                    this.deck.updateButton(buttonIndex, {
                        label: label,
                        icon: status === 'online' ? '🟢' : '🔴',
                        color: color
                    });
                } catch (error) {
                    status = 'offline';
                    this.deck.updateButton(buttonIndex, {
                        label: `${host}\nOffline`,
                        icon: '🔴',
                        color: this.getGradient('#3e1a1a', '#6f2d2d')
                    });
                }
            }
        };
    }

    // Uptime Widget (Session uptime)
    createUptimeWidget(buttonIndex, config) {
        const sessionStart = Date.now();

        return {
            type: 'uptime',
            updateInterval: 1000,
            update: () => {
                const uptime = Date.now() - sessionStart;
                const seconds = Math.floor(uptime / 1000);
                const minutes = Math.floor(seconds / 60);
                const hours = Math.floor(minutes / 60);
                const days = Math.floor(hours / 24);

                let uptimeStr;
                if (days > 0) {
                    uptimeStr = `${days}d ${hours % 24}h`;
                } else if (hours > 0) {
                    uptimeStr = `${hours}h ${minutes % 60}m`;
                } else {
                    uptimeStr = `${minutes}m ${seconds % 60}s`;
                }

                this.deck.updateButton(buttonIndex, {
                    label: `Uptime\n${uptimeStr}`,
                    icon: '⏱️',
                    color: this.getGradient('#1a2e3e', '#2d4d6f')
                });
            }
        };
    }

    // Utility Methods

    createProgressBar(percentage, length = 10) {
        const filled = Math.round((percentage / 100) * length);
        const empty = length - filled;
        return '█'.repeat(filled) + '░'.repeat(empty);
    }

    getGradient(color1, color2) {
        return `linear-gradient(135deg, ${color1} 0%, ${color2} 100%)`;
    }

    playTimerAlert() {
        if (this.deck.config.soundEnabled && this.deck.audioContext) {
            const audioContext = this.deck.audioContext;

            for (let i = 0; i < 3; i++) {
                setTimeout(() => {
                    const oscillator = audioContext.createOscillator();
                    const gainNode = audioContext.createGain();

                    oscillator.connect(gainNode);
                    gainNode.connect(audioContext.destination);

                    oscillator.frequency.value = 800;
                    gainNode.gain.setValueAtTime(0.2, audioContext.currentTime);
                    gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + 0.2);

                    oscillator.start(audioContext.currentTime);
                    oscillator.stop(audioContext.currentTime + 0.2);
                }, i * 300);
            }
        }
    }

    stopAll() {
        this.updateIntervals.forEach((intervalId) => {
            clearInterval(intervalId);
        });
        this.updateIntervals.clear();

        this.activeWidgets.forEach((widget) => {
            if (widget.cleanup) {
                widget.cleanup();
            }
        });
        this.activeWidgets.clear();
    }

    getActiveWidgets() {
        return Array.from(this.activeWidgets.entries()).map(([index, widget]) => ({
            buttonIndex: index,
            type: widget.type,
            config: widget.config || null
        }));
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = LiveKeysManager;
}
