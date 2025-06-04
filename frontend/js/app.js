// Main Application - Sapera Camera System Web Interface
class CameraApp {
    constructor() {
        this.api = null;
        this.ui = null;
        this.cameraGrid = null;
        this.controls = null;
        
        this.isInitialized = false;
        this.connectionRetryCount = 0;
        this.maxRetries = 5;
        
        this.init();
    }

    async init() {
        console.log('🚀 Initializing Sapera Camera System Web Interface...');
        
        try {
            // Initialize UI first
            this.ui = new UI();
            this.ui.showLoading('Initializing camera system...');
            
            // Initialize API
            this.api = new CameraAPI();
            
            // Initialize camera grid
            this.cameraGrid = new CameraGrid(this.api, this.ui);
            
            // Initialize controls
            this.controls = new Controls(this.api, this.ui, this.cameraGrid);
            
            // Make controls globally available for individual camera capture
            window.controls = this.controls;
            window.cameraGrid = this.cameraGrid;
            
            // Setup global event listeners
            this.setupGlobalEventListeners();
            
            // Try to connect to the backend
            await this.connectToBackend();
            
            this.isInitialized = true;
            this.ui.hideLoading();
            
            console.log('✅ Camera system initialized successfully');
            this.showWelcomeMessage();
            
        } catch (error) {
            console.error('❌ Failed to initialize camera system:', error);
            this.ui.hideLoading();
            this.ui.showError(`Initialization failed: ${error.message}`);
            this.showOfflineMode();
        }
    }

    async connectToBackend() {
        try {
            // Test connection with health check
            const health = await this.api.healthCheck();
            
            if (health) {
                console.log('🔗 Connected to camera backend');
                this.connectionRetryCount = 0;
                
                // Load initial data
                await this.loadInitialData();
                
                return true;
            }
        } catch (error) {
            console.warn('⚠️ Backend connection failed:', error.message);
            
            if (this.connectionRetryCount < this.maxRetries) {
                this.connectionRetryCount++;
                this.ui.showWarning(`Connection attempt ${this.connectionRetryCount}/${this.maxRetries} failed. Retrying...`);
                
                // Retry after delay
                setTimeout(() => {
                    this.connectToBackend();
                }, 3000);
            } else {
                throw new Error(`Failed to connect after ${this.maxRetries} attempts`);
            }
        }
        
        return false;
    }

    async loadInitialData() {
        try {
            // Load cameras
            await this.cameraGrid.loadCameras();
            
            // Load any saved settings
            this.controls.loadSavedSettings();
            
            console.log('📊 Initial data loaded');
            
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.ui.showWarning('Some data failed to load. System may have limited functionality.');
        }
    }

    setupGlobalEventListeners() {
        // Connection status changes
        this.api.on('connectionChange', (connected) => {
            this.onConnectionChange(connected);
        });

        // Error handling
        this.api.on('error', (error) => {
            console.error('API Error:', error);
        });

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            this.handleKeyboardShortcuts(e);
        });

        // Page visibility changes
        document.addEventListener('visibilitychange', () => {
            this.handleVisibilityChange();
        });

        // Window beforeunload
        window.addEventListener('beforeunload', () => {
            this.cleanup();
        });

        // Custom events
        document.addEventListener('cameraSelected', (e) => {
            console.log('Camera selected:', e.detail.serialNumber);
        });
    }

    onConnectionChange(connected) {
        this.ui.updateConnectionStatus(connected);
        
        if (connected) {
            this.ui.showSuccess('Connected to camera system');
            this.connectionRetryCount = 0;
            
            // Reload data when reconnected
            this.loadInitialData();
        } else {
            this.ui.showError('Lost connection to camera system');
            
            // Attempt to reconnect
            setTimeout(() => {
                if (!this.api.isConnected) {
                    this.connectToBackend();
                }
            }, 5000);
        }
    }

    handleKeyboardShortcuts(e) {
        // Only handle shortcuts when not typing in inputs
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
            return;
        }

        switch (e.key.toLowerCase()) {
            case 'c':
                if (e.ctrlKey || e.metaKey) return; // Don't interfere with Ctrl+C
                e.preventDefault();
                this.controls.captureAllCameras();
                break;
                
            case 'r':
                if (e.ctrlKey || e.metaKey) return; // Don't interfere with Ctrl+R
                e.preventDefault();
                this.cameraGrid.refresh();
                break;
                
            case 's':
                if (e.ctrlKey || e.metaKey) {
                    e.preventDefault();
                    this.controls.saveCurrentSettings();
                }
                break;
                
            case 'escape':
                // Close any open modals
                this.ui.hideModal('settingsModal');
                break;
        }
    }

    handleVisibilityChange() {
        if (document.hidden) {
            // Page is hidden - pause auto-refresh
            console.log('🔕 Page hidden - pausing auto-refresh');
            this.cameraGrid.setAutoRefresh(false);
        } else {
            // Page is visible - resume auto-refresh
            console.log('👁️ Page visible - resuming auto-refresh');
            this.cameraGrid.setAutoRefresh(true);
            
            // Refresh data immediately
            if (this.api.isConnected) {
                this.cameraGrid.refresh();
            }
        }
    }

    showWelcomeMessage() {
        const welcomeMessage = `
            🎉 Welcome to Sapera Camera System v2.0!
            
            ✅ Web interface ready
            📱 Console interface available
            🎮 All controls operational
            
            Use keyboard shortcuts:
            • 'C' - Capture
            • 'R' - Refresh
            • 'Ctrl+S' - Save settings
        `;
        
        this.ui.showSuccess(welcomeMessage, 8000);
        this.controls.logActivity('info', 'Web interface ready - system operational');
    }

    showOfflineMode() {
        const offlineMessage = `
            🔌 Offline Mode
            
            Cannot connect to camera backend.
            Please ensure:
            • Camera system is running
            • Web server is started
            • Network connection is available
            
            The interface will continue trying to connect automatically.
        `;
        
        this.ui.showWarning(offlineMessage, 10000);
    }

    // Public API methods for external access
    getSystemStatus() {
        return {
            initialized: this.isInitialized,
            connected: this.api?.isConnected || false,
            cameras: this.cameraGrid?.cameras?.length || 0,
            controls: this.controls?.getSystemStats() || {},
            version: '2.0'
        };
    }

    // Manual refresh for all components
    async refreshAll() {
        try {
            this.ui.showLoading('Refreshing all data...');
            
            await Promise.all([
                this.cameraGrid.refresh(),
                this.api.healthCheck()
            ]);
            
            this.ui.hideLoading();
            this.ui.showSuccess('All data refreshed');
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.showError(`Refresh failed: ${error.message}`);
        }
    }

    // Export system configuration
    exportConfig() {
        const config = {
            timestamp: new Date().toISOString(),
            cameras: this.cameraGrid.cameras,
            settings: this.controls.getSystemStats(),
            system: this.getSystemStatus()
        };
        
        const blob = new Blob([JSON.stringify(config, null, 2)], {
            type: 'application/json'
        });
        
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `sapera-config-${new Date().toISOString().split('T')[0]}.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        this.ui.showSuccess('Configuration exported');
    }

    // Cleanup
    cleanup() {
        console.log('🧹 Cleaning up camera system...');
        
        if (this.controls) {
            this.controls.destroy();
        }
        
        if (this.cameraGrid) {
            this.cameraGrid.destroy();
        }
        
        if (this.api) {
            this.api.destroy();
        }
    }
}

// Global application instance
let cameraApp;

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('📄 DOM loaded - starting camera application...');
    cameraApp = new CameraApp();
});

// Global functions for HTML onclick handlers
window.cameraGrid = {
    captureFromCamera: (serial) => cameraApp?.cameraGrid?.captureFromCamera(serial),
    showCameraDetails: (serial) => cameraApp?.cameraGrid?.showCameraDetails(serial),
    refresh: () => cameraApp?.cameraGrid?.refresh()
};

window.cameraApp = {
    getStatus: () => cameraApp?.getSystemStatus(),
    refresh: () => cameraApp?.refreshAll(),
    export: () => cameraApp?.exportConfig()
};

// Export for debugging
window.app = cameraApp; 