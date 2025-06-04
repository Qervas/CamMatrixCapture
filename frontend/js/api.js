// API Layer for Sapera Camera System
class CameraAPI {
    constructor(baseUrl = '') {
        this.baseUrl = baseUrl;
        this.isConnected = false;
        this.reconnectInterval = null;
        this.healthCheckInterval = null;
        this.callbacks = {
            connectionChange: [],
            cameraUpdate: [],
            captureComplete: [],
            error: []
        };
        
        this.startHealthCheck();
    }

    // Connection Management
    async startHealthCheck() {
        this.healthCheckInterval = setInterval(async () => {
            try {
                const response = await this.healthCheck();
                if (response && !this.isConnected) {
                    this.setConnectionStatus(true);
                }
            } catch (error) {
                if (this.isConnected) {
                    this.setConnectionStatus(false);
                    this.emit('error', 'Lost connection to camera system');
                }
            }
        }, 5000); // Check every 5 seconds
    }

    setConnectionStatus(connected) {
        if (this.isConnected !== connected) {
            this.isConnected = connected;
            this.emit('connectionChange', connected);
        }
    }

    // Event System
    on(event, callback) {
        if (this.callbacks[event]) {
            this.callbacks[event].push(callback);
        }
    }

    emit(event, data) {
        if (this.callbacks[event]) {
            this.callbacks[event].forEach(callback => callback(data));
        }
    }

    // HTTP Helper
    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        const defaultOptions = {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            }
        };

        const finalOptions = { ...defaultOptions, ...options };

        if (finalOptions.body && typeof finalOptions.body === 'object') {
            finalOptions.body = JSON.stringify(finalOptions.body);
        }

        try {
            const response = await fetch(url, finalOptions);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            } else {
                return await response.text();
            }
        } catch (error) {
            console.error(`API Request failed: ${endpoint}`, error);
            this.emit('error', `API Error: ${error.message}`);
            throw error;
        }
    }

    // API Endpoints
    async healthCheck() {
        return await this.request('/health');
    }

    async getCameras() {
        return await this.request('/api/cameras');
    }

    async getCamera(serialNumber) {
        return await this.request(`/api/cameras/${serialNumber}`);
    }

    async updateCameraParameter(serialNumber, parameter, value) {
        return await this.request(`/api/cameras/${serialNumber}/parameters/${parameter}`, {
            method: 'PUT',
            body: { value: value }
        });
    }

    async captureImages(options = {}) {
        const body = {
            cameras: options.cameras || 'all',
            shots: options.shots || 1,
            format: options.format || 'tiff',
            outputDir: options.outputDir || 'captured_images'
        };

        return await this.request('/api/capture', {
            method: 'POST',
            body: body
        });
    }

    async getDefaults() {
        return await this.request('/api/defaults');
    }

    async getSystemStatus() {
        try {
            const [cameras, health] = await Promise.all([
                this.getCameras(),
                this.healthCheck()
            ]);

            return {
                cameras: cameras,
                health: health,
                timestamp: new Date().toISOString()
            };
        } catch (error) {
            console.error('Failed to get system status:', error);
            throw error;
        }
    }

    // Batch Operations
    async updateAllCamerasParameter(parameter, value) {
        try {
            const cameras = await this.getCameras();
            const updates = cameras.cameras.map(camera => 
                this.updateCameraParameter(camera.serialNumber, parameter, value)
            );
            
            const results = await Promise.allSettled(updates);
            
            const successful = results.filter(r => r.status === 'fulfilled').length;
            const failed = results.filter(r => r.status === 'rejected').length;
            
            return {
                total: cameras.cameras.length,
                successful,
                failed,
                results
            };
        } catch (error) {
            console.error('Batch update failed:', error);
            throw error;
        }
    }

    // Utility Methods
    formatError(error) {
        if (error.response && error.response.data) {
            return error.response.data.message || error.response.data.error || 'Unknown API error';
        }
        return error.message || 'Unknown error occurred';
    }

    // Cleanup
    destroy() {
        if (this.healthCheckInterval) {
            clearInterval(this.healthCheckInterval);
        }
        if (this.reconnectInterval) {
            clearInterval(this.reconnectInterval);
        }
    }
}

// Export for use in other modules
window.CameraAPI = CameraAPI; 