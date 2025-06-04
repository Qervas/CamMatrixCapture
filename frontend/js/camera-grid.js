// Camera Grid Component for displaying camera status
class CameraGrid {
    constructor(api, ui) {
        this.api = api;
        this.ui = ui;
        this.cameras = [];
        this.container = document.getElementById('cameraGrid');
        this.refreshInterval = null;
        this.autoRefresh = true;
        
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Listen for API connection changes
        this.api.on('connectionChange', (connected) => {
            if (connected) {
                this.loadCameras();
            } else {
                this.showDisconnectedState();
            }
        });

        // Listen for camera updates
        this.api.on('cameraUpdate', (cameras) => {
            this.updateCameraDisplay(cameras);
        });
    }

    async loadCameras() {
        try {
            this.ui.showLoading('Loading camera information...');
            
            const response = await this.api.getCameras();
            this.cameras = response.cameras || [];
            
            this.renderCameras();
            this.updateStatistics();
            
            this.ui.hideLoading();
            this.ui.showSuccess(`Loaded ${this.cameras.length} cameras`);
            
            // Start auto-refresh if enabled
            if (this.autoRefresh) {
                this.startAutoRefresh();
            }
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.showError(`Failed to load cameras: ${error.message}`);
            this.showErrorState(error.message);
        }
    }

    renderCameras() {
        if (!this.cameras || this.cameras.length === 0) {
            this.showEmptyState();
            return;
        }

        // Sort cameras by position
        const sortedCameras = [...this.cameras].sort((a, b) => {
            return (a.position || 99) - (b.position || 99);
        });

        this.container.innerHTML = '';

        sortedCameras.forEach(camera => {
            const cameraCard = this.createCameraCard(camera);
            this.container.appendChild(cameraCard);
        });
    }

    createCameraCard(camera) {
        const card = document.createElement('div');
        card.className = `camera-card ${this.getCameraStatusClass(camera)}`;
        card.dataset.serial = camera.serialNumber;

        const statusIcon = this.getStatusIcon(camera);
        const statusText = this.getStatusText(camera);
        const parameters = camera.parameters || {};

        card.innerHTML = `
            <div class="camera-header">
                <span>Position ${camera.position || 'N/A'}</span>
                <span class="camera-serial">${camera.serialNumber || 'Unknown'}</span>
            </div>
            
            <div class="camera-status ${this.getCameraStatusClass(camera)}">
                ${statusIcon} ${statusText}
            </div>
            
            <div class="camera-body">
                <div class="camera-info">
                    <strong>Model:</strong> ${camera.modelName || 'Unknown'}
                </div>
                
                <div class="camera-params">
                    <div class="param-item">
                        <span class="param-label">Exposure:</span>
                        <span class="param-value">${parameters.exposureTime || 'N/A'} μs</span>
                    </div>
                    <div class="param-item">
                        <span class="param-label">Gain:</span>
                        <span class="param-value">${parameters.gain || 'N/A'}</span>
                    </div>
                    <div class="param-item">
                        <span class="param-label">Black Level:</span>
                        <span class="param-value">${parameters.blackLevel || 'N/A'}</span>
                    </div>
                    <div class="param-item">
                        <span class="param-label">Auto Exposure:</span>
                        <span class="param-value">${parameters.autoExposure ? 'On' : 'Off'}</span>
                    </div>
                </div>

                <div class="camera-actions">
                    <button class="btn btn-small" onclick="cameraGrid.captureFromCamera('${camera.serialNumber}')">
                        <i class="fas fa-camera"></i> Capture
                    </button>
                    <button class="btn btn-small btn-secondary" onclick="cameraGrid.showCameraDetails('${camera.serialNumber}')">
                        <i class="fas fa-info-circle"></i> Details
                    </button>
                </div>
            </div>
        `;

        // Add hover effects and animations
        this.addCardInteractions(card, camera);

        return card;
    }

    addCardInteractions(card, camera) {
        // Hover effects
        card.addEventListener('mouseenter', () => {
            if (camera.isConnected) {
                card.style.transform = 'translateY(-4px)';
                card.style.boxShadow = '0 8px 25px rgba(0,0,0,0.15)';
            }
        });

        card.addEventListener('mouseleave', () => {
            card.style.transform = 'translateY(0)';
            card.style.boxShadow = '';
        });

        // Click to select
        card.addEventListener('click', (e) => {
            if (e.target.tagName !== 'BUTTON') {
                this.selectCamera(camera.serialNumber);
            }
        });
    }

    getCameraStatusClass(camera) {
        if (!camera) return 'offline';
        
        if (camera.isConnected === true || camera.connected === true) {
            return 'connected';
        } else if (camera.isConnected === false || camera.connected === false) {
            return 'offline';
        } else {
            return 'loading';
        }
    }

    getStatusIcon(camera) {
        switch (this.getCameraStatusClass(camera)) {
            case 'connected':
                return '<i class="fas fa-check-circle"></i>';
            case 'offline':
                return '<i class="fas fa-times-circle"></i>';
            case 'loading':
                return '<i class="fas fa-spinner fa-spin"></i>';
            default:
                return '<i class="fas fa-question-circle"></i>';
        }
    }

    getStatusText(camera) {
        switch (this.getCameraStatusClass(camera)) {
            case 'connected':
                return 'Connected';
            case 'offline':
                return 'Offline';
            case 'loading':
                return 'Connecting...';
            default:
                return 'Unknown';
        }
    }

    selectCamera(serialNumber) {
        // Remove previous selections
        document.querySelectorAll('.camera-card').forEach(card => {
            card.classList.remove('selected');
        });

        // Add selection to clicked card
        const card = document.querySelector(`[data-serial="${serialNumber}"]`);
        if (card) {
            card.classList.add('selected');
            // You could emit an event here for other components to listen to
            document.dispatchEvent(new CustomEvent('cameraSelected', {
                detail: { serialNumber }
            }));
        }
    }

    async captureFromCamera(serialNumber) {
        try {
            this.ui.showLoading(`Capturing from camera ${serialNumber}...`);
            
            // Get the current output folder from controls
            const outputFolder = window.controls ? window.controls.getCurrentOutputFolder() : 'captured_images';
            
            // Use the individual camera capture endpoint
            const result = await this.api.request(`/api/cameras/${serialNumber}/capture`, {
                method: 'POST',
                body: {
                    outputDir: outputFolder,
                    format: 'tiff'
                }
            });
            
            this.ui.hideLoading();
            
            if (result.success) {
                this.ui.showSuccess(`📸 Capture completed for camera ${serialNumber}!`);
                
                // Log to activity if controls are available
                if (window.controls) {
                    window.controls.logActivity('success', `Individual capture: Camera ${serialNumber} (Position ${this.getCameraPosition(serialNumber)})`);
                    if (result.filename) {
                        window.controls.logActivity('info', `📁 Image saved: ${result.filename}`);
                    }
                }
            } else {
                this.ui.showWarning(`Capture completed with issues for camera ${serialNumber}`);
            }
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.showError(`❌ Capture failed for camera ${serialNumber}: ${error.message}`);
            
            // Log error if controls are available
            if (window.controls) {
                window.controls.logActivity('error', `Individual capture failed: Camera ${serialNumber} - ${error.message}`);
            }
        }
    }
    
    getCameraPosition(serialNumber) {
        const camera = this.cameras.find(c => c.serialNumber === serialNumber);
        return camera ? camera.position : 'Unknown';
    }

    async showCameraDetails(serialNumber) {
        try {
            const camera = this.cameras.find(c => c.serialNumber === serialNumber);
            if (!camera) {
                this.ui.showError('Camera not found');
                return;
            }

            // Create a simple details modal
            const details = this.formatCameraDetails(camera);
            
            // For now, show in notification - you could create a proper modal
            this.ui.showInfo(details, 10000);
            
        } catch (error) {
            this.ui.showError(`Failed to get camera details: ${error.message}`);
        }
    }

    formatCameraDetails(camera) {
        const params = camera.parameters || {};
        return `
            Camera Details:
            • Serial: ${camera.serialNumber}
            • Position: ${camera.position || 'N/A'}
            • Model: ${camera.modelName || 'Unknown'}
            • Status: ${this.getStatusText(camera)}
            • Exposure: ${params.exposureTime || 'N/A'} μs
            • Gain: ${params.gain || 'N/A'}
            • Black Level: ${params.blackLevel || 'N/A'}
        `;
    }

    updateStatistics() {
        const total = this.cameras.length;
        const connected = this.cameras.filter(c => 
            c.isConnected === true || c.connected === true
        ).length;

        // Update stat cards
        const totalElement = document.getElementById('totalCameras');
        const connectedElement = document.getElementById('connectedCameras');

        if (totalElement) totalElement.textContent = total;
        if (connectedElement) connectedElement.textContent = connected;

        // Update last capture time if available
        const lastCaptureElement = document.getElementById('lastCaptureTime');
        if (lastCaptureElement) {
            // This would come from your API
            lastCaptureElement.textContent = 'Just now';
        }
    }

    showEmptyState() {
        this.container.innerHTML = `
            <div class="camera-card">
                <div class="camera-header">
                    <i class="fas fa-search"></i> No Cameras Found
                </div>
                <div class="camera-body">
                    <p>No cameras are currently detected by the system.</p>
                    <p>Please check:</p>
                    <ul>
                        <li>Camera connections</li>
                        <li>Power supply</li>
                        <li>System configuration</li>
                    </ul>
                    <button class="btn btn-primary" onclick="cameraGrid.loadCameras()">
                        <i class="fas fa-sync-alt"></i> Refresh
                    </button>
                </div>
            </div>
        `;
    }

    showErrorState(errorMessage) {
        this.container.innerHTML = `
            <div class="camera-card offline">
                <div class="camera-header">
                    <i class="fas fa-exclamation-triangle"></i> Connection Error
                </div>
                <div class="camera-body">
                    <p>Failed to connect to the camera system:</p>
                    <p class="error">${errorMessage}</p>
                    <button class="btn btn-primary" onclick="cameraGrid.loadCameras()">
                        <i class="fas fa-sync-alt"></i> Retry
                    </button>
                </div>
            </div>
        `;
    }

    showDisconnectedState() {
        this.container.innerHTML = `
            <div class="camera-card loading">
                <div class="camera-header">
                    <i class="fas fa-wifi"></i> Disconnected
                </div>
                <div class="camera-body">
                    <p>Lost connection to camera system.</p>
                    <p>Attempting to reconnect...</p>
                    <div class="loading-dots">
                        <span></span><span></span><span></span>
                    </div>
                </div>
            </div>
        `;
    }

    startAutoRefresh(interval = 30000) {
        this.stopAutoRefresh();
        this.refreshInterval = setInterval(() => {
            if (this.api.isConnected) {
                this.loadCameras();
            }
        }, interval);
    }

    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }

    setAutoRefresh(enabled) {
        this.autoRefresh = enabled;
        if (enabled) {
            this.startAutoRefresh();
        } else {
            this.stopAutoRefresh();
        }
    }

    refresh() {
        this.loadCameras();
    }

    // Update individual camera
    updateCamera(serialNumber, updatedData) {
        const cameraIndex = this.cameras.findIndex(c => c.serialNumber === serialNumber);
        if (cameraIndex !== -1) {
            this.cameras[cameraIndex] = { ...this.cameras[cameraIndex], ...updatedData };
            this.renderCameras();
            this.updateStatistics();
        }
    }

    // Cleanup
    destroy() {
        this.stopAutoRefresh();
    }
}

// Export for use in other modules
window.CameraGrid = CameraGrid; 