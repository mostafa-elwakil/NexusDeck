/**
 * Non-blocking notification manager with bounded history.
 */
class NotificationManager {
    constructor(options = {}) {
        this.maxHistory = options.maxHistory || 50;
        this.defaultDuration = options.defaultDuration || 3000;
        this.history = [];
        this.notifications = new Set();
        this.container = null;
    }

    ensureContainer() {
        if (this.container && document.body.contains(this.container)) {
            return this.container;
        }

        this.container = document.createElement('div');
        this.container.className = 'notification-stack';
        this.container.setAttribute('aria-live', 'polite');
        this.container.setAttribute('aria-atomic', 'false');
        document.body.appendChild(this.container);
        return this.container;
    }

    show(title, message, type = 'info', duration = this.defaultDuration) {
        const entry = {
            id: `${Date.now()}-${Math.random().toString(36).slice(2)}`,
            title: String(title || 'Notification'),
            message: String(message || ''),
            type: ['info', 'success', 'warning', 'error'].includes(type) ? type : 'info',
            timestamp: Date.now()
        };

        this.history.push(entry);
        if (this.history.length > this.maxHistory) {
            this.history.splice(0, this.history.length - this.maxHistory);
        }

        const notification = document.createElement('article');
        notification.className = `action-notification notification-${entry.type}`;
        notification.dataset.notificationId = entry.id;

        const content = document.createElement('div');
        content.className = 'notification-content';

        const titleElement = document.createElement('div');
        titleElement.className = 'notification-title';
        titleElement.textContent = entry.title;

        const messageElement = document.createElement('div');
        messageElement.className = 'notification-message';
        messageElement.textContent = entry.message;

        const closeButton = document.createElement('button');
        closeButton.type = 'button';
        closeButton.className = 'notification-close';
        closeButton.setAttribute('aria-label', 'Dismiss notification');
        closeButton.textContent = '×';
        closeButton.addEventListener('click', () => this.dismiss(notification));

        content.append(titleElement, messageElement);
        notification.append(content, closeButton);
        this.ensureContainer().appendChild(notification);
        this.notifications.add(notification);

        requestAnimationFrame(() => notification.classList.add('show'));

        if (duration > 0) {
            notification._dismissTimer = setTimeout(() => this.dismiss(notification), duration);
        }

        return entry.id;
    }

    dismiss(notification) {
        if (!notification || !this.notifications.has(notification)) {
            return;
        }

        clearTimeout(notification._dismissTimer);
        notification.classList.remove('show');
        notification.addEventListener('transitionend', () => this.remove(notification), { once: true });
        setTimeout(() => this.remove(notification), 350);
    }

    remove(notification) {
        if (!this.notifications.has(notification)) {
            return;
        }

        this.notifications.delete(notification);
        notification.remove();
    }

    getHistory(limit = this.maxHistory) {
        return this.history.slice(-Math.max(0, limit)).reverse();
    }

    clearHistory() {
        this.history = [];
    }

    clearVisible() {
        Array.from(this.notifications).forEach(notification => this.dismiss(notification));
    }

    destroy() {
        this.clearVisible();
        this.clearHistory();
        if (this.container) {
            this.container.remove();
            this.container = null;
        }
    }
}

const notificationManager = new NotificationManager();

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { NotificationManager, notificationManager };
}
