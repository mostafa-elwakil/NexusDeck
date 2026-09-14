/**
 * Performance Utilities
 * Debouncing, throttling, and other performance optimizations
 */

class PerformanceUtils {
    /**
     * Debounce a function - delay execution until calls stop
     * @param {Function} func - Function to debounce
     * @param {number} wait - Milliseconds to wait
     * @returns {Function} Debounced function
     */
    static debounce(func, wait = 300) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }

    /**
     * Throttle a function - limit execution frequency
     * @param {Function} func - Function to throttle
     * @param {number} limit - Milliseconds between executions
     * @returns {Function} Throttled function
     */
    static throttle(func, limit = 300) {
        let inThrottle;
        return function executedFunction(...args) {
            if (!inThrottle) {
                func(...args);
                inThrottle = true;
                setTimeout(() => inThrottle = false, limit);
            }
        };
    }

    /**
     * Batch DOM updates to avoid layout thrashing
     * @param {Function} updateFn - Function containing DOM updates
     */
    static batchDOMUpdate(updateFn) {
        if (window.requestAnimationFrame) {
            window.requestAnimationFrame(updateFn);
        } else {
            setTimeout(updateFn, 0);
        }
    }

    /**
     * Debounced window resize listener
     * @param {Function} callback - Function to call on resize
     * @param {number} delay - Debounce delay
     */
    static onWindowResize(callback, delay = 300) {
        const debouncedCallback = this.debounce(callback, delay);
        window.addEventListener('resize', debouncedCallback);
        
        // Return unsubscribe function
        return () => {
            window.removeEventListener('resize', debouncedCallback);
        };
    }

    /**
     * Detect if element is in viewport
     * @param {HTMLElement} element - Element to check
     * @returns {boolean} True if in viewport
     */
    static isInViewport(element) {
        if (!element || !(element instanceof HTMLElement)) {
            return false;
        }
        const rect = element.getBoundingClientRect();
        return (
            rect.top >= 0 &&
            rect.left >= 0 &&
            rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
            rect.right <= (window.innerWidth || document.documentElement.clientWidth)
        );
    }

    /**
     * Safely query DOM element with validation
     * @param {string} selector - CSS selector
     * @param {HTMLElement} context - Context element (default: document)
     * @returns {HTMLElement|null} Element or null
     */
    static safeQuerySelector(selector, context = document) {
        try {
            if (!selector || typeof selector !== 'string') {
                console.warn('Invalid selector:', selector);
                return null;
            }
            return context.querySelector(selector);
        } catch (error) {
            console.error('Invalid CSS selector:', selector, error);
            return null;
        }
    }

    /**
     * Safely query multiple DOM elements with validation
     * @param {string} selector - CSS selector
     * @param {HTMLElement} context - Context element (default: document)
     * @returns {NodeList|[]} Elements or empty array
     */
    static safeQuerySelectorAll(selector, context = document) {
        try {
            if (!selector || typeof selector !== 'string') {
                console.warn('Invalid selector:', selector);
                return [];
            }
            return context.querySelectorAll(selector);
        } catch (error) {
            console.error('Invalid CSS selector:', selector, error);
            return [];
        }
    }

    /**
     * Monitor performance of a function
     * @param {string} name - Name for logging
     * @param {Function} func - Function to monitor
     * @returns {*} Result of function
     */
    static measure(name, func) {
        const start = performance.now();
        try {
            const result = func();
            const duration = performance.now() - start;
            if (duration > 16.67) { // > 1 frame at 60fps
                console.warn(`⚠️  ${name} took ${duration.toFixed(2)}ms (slow)`);
            } else {
                console.log(`✓ ${name} took ${duration.toFixed(2)}ms`);
            }
            return result;
        } catch (error) {
            const duration = performance.now() - start;
            console.error(`✗ ${name} failed after ${duration.toFixed(2)}ms:`, error);
            throw error;
        }
    }

    /**
     * Memory info (Chrome only)
     * @returns {Object} Memory info or null
     */
    static getMemoryInfo() {
        if (performance.memory) {
            return {
                usedJSHeapSize: (performance.memory.usedJSHeapSize / 1048576).toFixed(2) + ' MB',
                totalJSHeapSize: (performance.memory.totalJSHeapSize / 1048576).toFixed(2) + ' MB',
                jsHeapSizeLimit: (performance.memory.jsHeapSizeLimit / 1048576).toFixed(2) + ' MB'
            };
        }
        return null;
    }

    /**
     * Check if browser supports a feature
     * @param {string} feature - Feature name
     * @returns {boolean} True if supported
     */
    static supportsFeature(feature) {
        const features = {
            'abortController': typeof AbortController !== 'undefined',
            'fetch': typeof fetch !== 'undefined',
            'localStorage': typeof localStorage !== 'undefined',
            'serviceWorker': 'serviceWorker' in navigator,
            'clipboard': navigator.clipboard !== undefined,
            'requestAnimationFrame': typeof requestAnimationFrame !== 'undefined',
            'webGL': (() => {
                try {
                    const canvas = document.createElement('canvas');
                    return !!(window.WebGLRenderingContext && 
                        (canvas.getContext('webgl') || canvas.getContext('experimental-webgl')));
                } catch (e) {
                    return false;
                }
            })(),
            'audioContext': typeof (window.AudioContext || window.webkitAudioContext) !== 'undefined'
        };

        return features[feature] || false;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PerformanceUtils;
}
