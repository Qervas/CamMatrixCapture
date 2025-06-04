// UI Utilities for Sapera Camera System
class UI {
    constructor() {
        this.notifications = document.getElementById('notifications');
        this.loadingOverlay = document.getElementById('loadingOverlay');
        this.setupModals();
    }

    // Notification System
    showNotification(message, type = 'info', duration = 5000) {
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        
        notification.innerHTML = `
            <div>${message}</div>
            <button class="notification-close">&times;</button>
        `;

        // Add to container
        this.notifications.appendChild(notification);

        // Handle close button
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            this.removeNotification(notification);
        });

        // Auto-remove after duration
        if (duration > 0) {
            setTimeout(() => {
                this.removeNotification(notification);
            }, duration);
        }

        return notification;
    }

    removeNotification(notification) {
        if (notification && notification.parentNode) {
            notification.style.animation = 'slideOutRight 0.3s ease';
            setTimeout(() => {
                if (notification.parentNode) {
                    notification.parentNode.removeChild(notification);
                }
            }, 300);
        }
    }

    // Success/Error shortcuts
    showSuccess(message, duration = 3000) {
        return this.showNotification(message, 'success', duration);
    }

    showError(message, duration = 7000) {
        return this.showNotification(message, 'error', duration);
    }

    showWarning(message, duration = 5000) {
        return this.showNotification(message, 'warning', duration);
    }

    showInfo(message, duration = 4000) {
        return this.showNotification(message, 'info', duration);
    }

    // Loading Overlay
    showLoading(message = 'Processing...') {
        const spinner = this.loadingOverlay.querySelector('.loading-spinner p');
        if (spinner) {
            spinner.textContent = message;
        }
        this.loadingOverlay.classList.add('show');
    }

    hideLoading() {
        this.loadingOverlay.classList.remove('show');
    }

    // Modal Management
    setupModals() {
        // Settings modal
        const settingsModal = document.getElementById('settingsModal');
        const settingsBtn = document.getElementById('settingsBtn');
        const closeBtn = settingsModal.querySelector('.modal-close');
        const cancelBtn = document.getElementById('cancelSettings');

        settingsBtn.addEventListener('click', () => {
            this.showModal('settingsModal');
        });

        closeBtn.addEventListener('click', () => {
            this.hideModal('settingsModal');
        });

        cancelBtn.addEventListener('click', () => {
            this.hideModal('settingsModal');
        });

        // Close modal on outside click
        settingsModal.addEventListener('click', (e) => {
            if (e.target === settingsModal) {
                this.hideModal('settingsModal');
            }
        });

        // Tab system for settings
        this.setupTabs();
    }

    showModal(modalId) {
        const modal = document.getElementById(modalId);
        if (modal) {
            modal.classList.add('show');
            document.body.style.overflow = 'hidden';
        }
    }

    hideModal(modalId) {
        const modal = document.getElementById(modalId);
        if (modal) {
            modal.classList.remove('show');
            document.body.style.overflow = 'auto';
        }
    }

    // Tab System
    setupTabs() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        const settingsContent = document.getElementById('settingsContent');

        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                // Remove active class from all tabs
                tabBtns.forEach(b => b.classList.remove('active'));
                
                // Add active class to clicked tab
                btn.classList.add('active');
                
                // Load tab content
                const tabName = btn.dataset.tab;
                this.loadTabContent(tabName, settingsContent);
            });
        });

        // Load default tab
        this.loadTabContent('global', settingsContent);
    }

    loadTabContent(tabName, container) {
        let content = '';
        
        switch (tabName) {
            case 'global':
                content = `
                    <div class="settings-section">
                        <h4>Global Camera Settings</h4>
                        <div class="form-group">
                            <label>Default Exposure Time (μs)</label>
                            <input type="number" id="globalExposure" min="500" max="100000" value="15000">
                        </div>
                        <div class="form-group">
                            <label>Default Gain</label>
                            <input type="number" id="globalGain" min="1.0" max="4.0" step="0.1" value="1.0">
                        </div>
                        <div class="form-group">
                            <label>Capture Format</label>
                            <select id="captureFormat">
                                <option value="tiff">TIFF</option>
                                <option value="png">PNG</option>
                                <option value="jpg">JPEG</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>
                                <input type="checkbox" id="autoRefresh" checked>
                                Auto-refresh camera status
                            </label>
                        </div>
                    </div>
                `;
                break;
                
            case 'individual':
                content = `
                    <div class="settings-section">
                        <h4>Individual Camera Settings</h4>
                        <div id="individualCameraSettings">
                            <p>Loading camera settings...</p>
                        </div>
                    </div>
                `;
                break;
                
            case 'advanced':
                content = `
                    <div class="settings-section">
                        <h4>Advanced Settings</h4>
                        <div class="form-group">
                            <label>API Refresh Interval (seconds)</label>
                            <input type="number" id="refreshInterval" min="1" max="60" value="10">
                        </div>
                        <div class="form-group">
                            <label>Capture Timeout (seconds)</label>
                            <input type="number" id="captureTimeout" min="5" max="120" value="30">
                        </div>
                        <div class="form-group">
                            <label>
                                <input type="checkbox" id="debugMode">
                                Enable debug logging
                            </label>
                        </div>
                        <div class="form-group">
                            <label>
                                <input type="checkbox" id="soundNotifications">
                                Sound notifications
                            </label>
                        </div>
                    </div>
                `;
                break;
        }
        
        container.innerHTML = content;
        
        // Load individual camera settings if needed
        if (tabName === 'individual') {
            this.loadIndividualCameraSettings();
        }
    }

    async loadIndividualCameraSettings() {
        const container = document.getElementById('individualCameraSettings');
        try {
            // This would be called when we have the API connected
            container.innerHTML = `
                <div class="camera-settings-grid">
                    <p>Connect to camera system to view individual settings</p>
                </div>
            `;
        } catch (error) {
            container.innerHTML = `<p class="error">Failed to load camera settings: ${error.message}</p>`;
        }
    }

    // Status Indicator
    updateConnectionStatus(connected) {
        const statusIndicator = document.getElementById('connectionStatus');
        const icon = statusIndicator.querySelector('i');
        
        statusIndicator.classList.remove('connected', 'error');
        
        if (connected) {
            statusIndicator.classList.add('connected');
            statusIndicator.innerHTML = '<i class="fas fa-circle"></i> Connected';
        } else {
            statusIndicator.classList.add('error');
            statusIndicator.innerHTML = '<i class="fas fa-circle"></i> Disconnected';
        }
    }

    // Button States
    setButtonLoading(buttonId, loading = true) {
        const button = document.getElementById(buttonId);
        if (!button) return;

        if (loading) {
            button.disabled = true;
            const icon = button.querySelector('i');
            if (icon) {
                icon.className = 'fas fa-spinner fa-spin';
            }
            button.dataset.originalText = button.textContent;
            const text = button.querySelector(':not(i)');
            if (text) {
                text.textContent = ' Processing...';
            }
        } else {
            button.disabled = false;
            const icon = button.querySelector('i');
            if (icon && button.dataset.originalIcon) {
                icon.className = button.dataset.originalIcon;
            }
            if (button.dataset.originalText) {
                button.textContent = button.dataset.originalText;
            }
        }
    }

    // Form Utilities
    getFormData(formId) {
        const form = document.getElementById(formId);
        if (!form) return {};

        const formData = new FormData(form);
        const data = {};
        
        for (let [key, value] of formData.entries()) {
            data[key] = value;
        }
        
        return data;
    }

    setFormData(formId, data) {
        const form = document.getElementById(formId);
        if (!form) return;

        Object.keys(data).forEach(key => {
            const element = form.querySelector(`[name="${key}"], #${key}`);
            if (element) {
                if (element.type === 'checkbox') {
                    element.checked = !!data[key];
                } else {
                    element.value = data[key];
                }
            }
        });
    }

    // Animation Helpers
    fadeIn(element, duration = 300) {
        element.style.opacity = '0';
        element.style.display = 'block';
        
        const start = performance.now();
        
        const animate = (currentTime) => {
            const elapsed = currentTime - start;
            const progress = Math.min(elapsed / duration, 1);
            
            element.style.opacity = progress;
            
            if (progress < 1) {
                requestAnimationFrame(animate);
            }
        };
        
        requestAnimationFrame(animate);
    }

    fadeOut(element, duration = 300, hideAfter = true) {
        const start = performance.now();
        const startOpacity = parseFloat(getComputedStyle(element).opacity);
        
        const animate = (currentTime) => {
            const elapsed = currentTime - start;
            const progress = Math.min(elapsed / duration, 1);
            
            element.style.opacity = startOpacity * (1 - progress);
            
            if (progress < 1) {
                requestAnimationFrame(animate);
            } else if (hideAfter) {
                element.style.display = 'none';
            }
        };
        
        requestAnimationFrame(animate);
    }
}

// Export for use in other modules
window.UI = UI; 