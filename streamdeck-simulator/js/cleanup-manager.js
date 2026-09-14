/**
 * Cleanup Manager
 * Handles graceful shutdown and resource cleanup for StreamDeck Application
 */

class CleanupManager {
    constructor() {
        this.cleanupTasks = [];
        this.isShuttingDown = false;
    }

    /**
     * Register a cleanup task
     * @param {Function} callback - Function to call during cleanup
     * @param {string} name - Optional name for debugging
     */
    register(callback, name = 'Unknown') {
        this.cleanupTasks.push({
            callback,
            name
        });
    }

    /**
     * Execute all registered cleanup tasks
     */
    async cleanup() {
        if (this.isShuttingDown) {
            return; // Prevent multiple simultaneous cleanup attempts
        }

        this.isShuttingDown = true;
        console.log('🧹 Starting application cleanup...');

        const failedTasks = [];

        for (const task of this.cleanupTasks) {
            try {
                console.log(`  ↳ Cleaning up: ${task.name}`);
                await Promise.resolve(task.callback());
            } catch (error) {
                console.error(`  ✗ Cleanup failed for ${task.name}:`, error);
                failedTasks.push(task.name);
            }
        }

        if (failedTasks.length > 0) {
            console.warn(`⚠️  ${failedTasks.length} cleanup task(s) failed:`, failedTasks.join(', '));
        } else {
            console.log('✅ All cleanup tasks completed successfully');
        }

        // Clear tasks
        this.cleanupTasks = [];
        this.isShuttingDown = false;
    }

    /**
     * Setup automatic cleanup on page unload
     */
    setupAutoCleanup() {
        // Cleanup on page close/reload
        window.addEventListener('beforeunload', () => {
            this.cleanup();
        });

        // Cleanup on page visibility change (tab switch)
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                console.log('📴 Page hidden - pausing resource-heavy operations');
                // Can trigger partial cleanup here if needed
            }
        });

        // Cleanup on Ctrl+C in console (for dev purposes)
        window.addEventListener('error', (event) => {
            if (event.message && event.message.includes('Uncaught')) {
                console.error('💥 Uncaught error detected, triggering cleanup');
                this.cleanup();
            }
        });
    }

    /**
     * Get cleanup status
     */
    getStatus() {
        return {
            isShuttingDown: this.isShuttingDown,
            taskCount: this.cleanupTasks.length,
            tasks: this.cleanupTasks.map(t => t.name)
        };
    }
}

// Global cleanup manager instance
const globalCleanupManager = new CleanupManager();

// Auto-setup on load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        globalCleanupManager.setupAutoCleanup();
    });
} else {
    // If script loads after DOMContentLoaded
    globalCleanupManager.setupAutoCleanup();
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { CleanupManager, globalCleanupManager };
}
