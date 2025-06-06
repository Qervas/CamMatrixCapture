// Controls component for camera parameter management
class Controls {
    constructor(api, ui, cameraGrid) {
        this.api = api;
        this.ui = ui;
        this.cameraGrid = cameraGrid;
        this.activityLog = document.getElementById('activityLog');
        this.captureCount = 0;
        
        this.setupEventListeners();
        this.initializeControls();
    }

    setupEventListeners() {
        // Capture button
        const captureBtn = document.getElementById('captureBtn');
        captureBtn.addEventListener('click', () => this.captureAllCameras());

        // Refresh button
        const refreshBtn = document.getElementById('refreshBtn');
        refreshBtn.addEventListener('click', () => this.refreshCameras());

        // Console button
        const consoleBtn = document.getElementById('consoleBtn');
        consoleBtn.addEventListener('click', () => this.showConsoleInfo());

        // Clear log button
        const clearLogBtn = document.getElementById('clearLog');
        clearLogBtn.addEventListener('click', () => this.clearActivityLog());

        // Folder selection
        const browseFolderBtn = document.getElementById('browseFolderBtn');
        browseFolderBtn.addEventListener('click', () => this.showFolderDialog());

        const createFolderBtn = document.getElementById('createFolderBtn');
        createFolderBtn.addEventListener('click', () => this.createNewFolder());

        // Parameter controls
        this.setupParameterControls();

        // Listen for API events
        this.api.on('captureComplete', (data) => {
            this.onCaptureComplete(data);
        });

        this.api.on('error', (error) => {
            this.logActivity('error', error);
        });
    }

    setupParameterControls() {
        // Exposure controls
        const exposureSlider = document.getElementById('exposureSlider');
        const exposureValue = document.getElementById('exposureValue');
        const applyExposure = document.getElementById('applyExposure');

        // Sync slider and input
        exposureSlider.addEventListener('input', () => {
            exposureValue.value = exposureSlider.value;
        });

        exposureValue.addEventListener('input', () => {
            exposureSlider.value = exposureValue.value;
        });

        applyExposure.addEventListener('click', () => {
            this.setExposureForAll(parseInt(exposureValue.value));
        });

        // Gain controls
        const gainSlider = document.getElementById('gainSlider');
        const gainValue = document.getElementById('gainValue');
        const applyGain = document.getElementById('applyGain');

        // Sync slider and input
        gainSlider.addEventListener('input', () => {
            gainValue.value = gainSlider.value;
        });

        gainValue.addEventListener('input', () => {
            gainSlider.value = gainValue.value;
        });

        applyGain.addEventListener('click', () => {
            this.setGainForAll(parseFloat(gainValue.value));
        });

        // Enter key support
        exposureValue.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.setExposureForAll(parseInt(exposureValue.value));
            }
        });

        gainValue.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.setGainForAll(parseFloat(gainValue.value));
            }
        });
    }

    initializeControls() {
        this.logActivity('info', 'Camera control system initialized');
        
        // Set initial values from your system defaults
        this.setControlValues({
            exposure: 15000,
            gain: 1.0
        });
    }

    setControlValues(values) {
        if (values.exposure) {
            const exposureSlider = document.getElementById('exposureSlider');
            const exposureValue = document.getElementById('exposureValue');
            exposureSlider.value = values.exposure;
            exposureValue.value = values.exposure;
        }

        if (values.gain) {
            const gainSlider = document.getElementById('gainSlider');
            const gainValue = document.getElementById('gainValue');
            gainSlider.value = values.gain;
            gainValue.value = values.gain;
        }
    }

    async captureAllCameras() {
        try {
            this.ui.setButtonLoading('captureBtn', true);
            this.ui.showLoading('Capturing from all cameras...');
            
            const outputFolder = this.getCurrentOutputFolder();
            console.log('🔍 Debug: Current output folder from UI:', outputFolder);
            this.logActivity('info', `Starting capture to folder: ${outputFolder}`);

            const captureOptions = {
                cameras: 'all',
                shots: 1,
                outputDir: outputFolder
            };
            
            console.log('🔍 Debug: Sending capture options:', captureOptions);

            const result = await this.api.captureImages(captureOptions);

            this.captureCount++;
            this.updateCaptureCount();

            this.ui.hideLoading();
            this.ui.setButtonLoading('captureBtn', false);
            
            if (result.status === 'success') {
                const successCount = result.total_images || 0;
                const message = `${successCount} image${successCount !== 1 ? 's' : ''} captured successfully!`;
                this.ui.showSuccess(message);
                this.logActivity('success', `Capture #${this.captureCount}: ${message}`);
                this.logActivity('info', `📁 Images saved to: ${result.output_directory}/`);
                
                // 🎉 NEW FEATURE: Show folder popup with option to open
                if (result.output_directory && successCount > 0) {
                    this.showFolderPopup(result.output_directory, successCount);
                }
            } else {
                this.ui.showWarning(`Capture failed: ${result.message || 'Unknown error'}`);
                this.logActivity('warning', `Capture #${this.captureCount}: ${result.message || 'Unknown error'}`);
            }
            
            // Update last capture time
            this.updateLastCaptureTime();
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.setButtonLoading('captureBtn', false);
            this.ui.showError(`Capture failed: ${error.message}`);
            
            this.logActivity('error', `Capture failed: ${error.message}`);
        }
    }

    async setExposureForAll(exposureTime) {
        // Validate exposure time
        if (exposureTime < 500 || exposureTime > 100000) {
            this.ui.showError('Exposure time must be between 500 and 100,000 μs');
            return;
        }

        try {
            this.ui.showLoading(`Setting exposure to ${exposureTime} μs...`);
            
            const result = await this.api.updateAllCamerasParameter('exposureTime', exposureTime);
            
            this.ui.hideLoading();
            
            if (result.failed > 0) {
                this.ui.showWarning(`Exposure updated for ${result.successful}/${result.total} cameras`);
            } else {
                this.ui.showSuccess(`Exposure set to ${exposureTime} μs for all cameras`);
            }
            
            this.logActivity('info', `Exposure updated: ${exposureTime} μs (${result.successful}/${result.total} cameras)`);
            
            // Refresh camera display
            this.cameraGrid.refresh();
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.showError(`Failed to set exposure: ${error.message}`);
            this.logActivity('error', `Exposure update failed: ${error.message}`);
        }
    }

    async setGainForAll(gain) {
        // Validate gain
        if (gain < 1.0 || gain > 4.0) {
            this.ui.showError('Gain must be between 1.0 and 4.0');
            return;
        }

        try {
            this.ui.showLoading(`Setting gain to ${gain}...`);
            
            const result = await this.api.updateAllCamerasParameter('gain', gain);
            
            this.ui.hideLoading();
            
            if (result.failed > 0) {
                this.ui.showWarning(`Gain updated for ${result.successful}/${result.total} cameras`);
            } else {
                this.ui.showSuccess(`Gain set to ${gain} for all cameras`);
            }
            
            this.logActivity('info', `Gain updated: ${gain} (${result.successful}/${result.total} cameras)`);
            
            // Refresh camera display
            this.cameraGrid.refresh();
            
        } catch (error) {
            this.ui.hideLoading();
            this.ui.showError(`Failed to set gain: ${error.message}`);
            this.logActivity('error', `Gain update failed: ${error.message}`);
        }
    }

    refreshCameras() {
        this.logActivity('info', 'Refreshing camera status...');
        this.cameraGrid.refresh();
    }

    showConsoleInfo() {
        const message = `
            Console Interface Available:
            
            The console interface is running in the terminal where you started the application.
            
            Available Commands:
            • 'c' - Single capture
            • '1-9' - Multiple captures  
            • 'p' - Print parameters
            • 'e' - Set exposure time
            • 'g' - Set gain
            • 'q' - Quit application
            
            Both web and console interfaces work simultaneously!
        `;
        
        this.ui.showInfo(message, 15000);
        this.logActivity('info', 'Console interface information displayed');
    }

    logActivity(level, message) {
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry';
        logEntry.innerHTML = `
            <span class="log-timestamp">[${timestamp}]</span>
            <span class="log-level-${level}">[${level.toUpperCase()}]</span>
            ${message}
        `;

        this.activityLog.appendChild(logEntry);
        
        // Auto-scroll to bottom
        this.activityLog.scrollTop = this.activityLog.scrollHeight;

        // Limit log entries (keep last 100)
        while (this.activityLog.children.length > 100) {
            this.activityLog.removeChild(this.activityLog.firstChild);
        }
    }

    clearActivityLog() {
        this.activityLog.innerHTML = '';
        this.logActivity('info', 'Activity log cleared');
    }

    updateCaptureCount() {
        const totalCapturesElement = document.getElementById('totalCaptures');
        if (totalCapturesElement) {
            totalCapturesElement.textContent = this.captureCount;
        }
    }

    updateLastCaptureTime() {
        const lastCaptureElement = document.getElementById('lastCaptureTime');
        if (lastCaptureElement) {
            const now = new Date();
            lastCaptureElement.textContent = now.toLocaleTimeString();
        }
    }

    onCaptureComplete(data) {
        this.updateLastCaptureTime();
        this.logActivity('success', `Capture completed: ${data.message || 'Unknown result'}`);
    }

    // Preset management
    saveCurrentSettings() {
        const exposureValue = document.getElementById('exposureValue').value;
        const gainValue = document.getElementById('gainValue').value;
        
        const settings = {
            exposure: parseInt(exposureValue),
            gain: parseFloat(gainValue),
            timestamp: new Date().toISOString()
        };

        // Save to localStorage
        localStorage.setItem('cameraSettings', JSON.stringify(settings));
        
        this.ui.showSuccess('Current settings saved');
        this.logActivity('info', `Settings saved: Exposure=${settings.exposure}μs, Gain=${settings.gain}`);
    }

    loadSavedSettings() {
        try {
            const saved = localStorage.getItem('cameraSettings');
            if (saved) {
                const settings = JSON.parse(saved);
                this.setControlValues(settings);
                
                this.ui.showInfo('Previous settings restored');
                this.logActivity('info', `Settings loaded: Exposure=${settings.exposure}μs, Gain=${settings.gain}`);
            }
        } catch (error) {
            console.error('Failed to load saved settings:', error);
        }
    }

    // Quick presets
    setPreset(presetName) {
        const presets = {
            'default': { exposure: 15000, gain: 1.0 },
            'bright': { exposure: 5000, gain: 2.0 },
            'dark': { exposure: 50000, gain: 1.5 },
            'fast': { exposure: 1000, gain: 3.0 }
        };

        const preset = presets[presetName];
        if (preset) {
            this.setControlValues(preset);
            this.ui.showInfo(`Preset applied: ${presetName}`);
            this.logActivity('info', `Preset applied: ${presetName} (Exposure=${preset.exposure}μs, Gain=${preset.gain})`);
        }
    }

    // Statistics and monitoring
    getSystemStats() {
        return {
            totalCaptures: this.captureCount,
            lastCaptureTime: document.getElementById('lastCaptureTime')?.textContent || 'Never',
            currentExposure: document.getElementById('exposureValue')?.value || 'Unknown',
            currentGain: document.getElementById('gainValue')?.value || 'Unknown',
            apiConnected: this.api.isConnected
        };
    }

    // Cleanup
    destroy() {
        // Save current settings before cleanup
        this.saveCurrentSettings();
        
        // Clear any intervals or timeouts
        this.logActivity('info', 'Camera controls shutting down...');
    }

    // Folder Management
    showFolderDialog() {
        const outputFolder = document.getElementById('outputFolder');
        const currentFolder = outputFolder.value || 'captured_images';
        
        // Create a simple folder selection dialog
        const folders = [
            'captured_images',
            'photos',
            'camera_captures',
            'images',
            'snapshots',
            'data/captures',
            'output',
            'temp_captures'
        ];
        
        const folderList = folders.map(folder => 
            `<div class="folder-option" data-folder="${folder}">📁 ${folder}</div>`
        ).join('');
        
        const dialogContent = `
            <div class="folder-dialog">
                <h4>📂 Select Output Folder</h4>
                <div class="current-folder">
                    <strong>Current:</strong> ${currentFolder}
                </div>
                <div class="folder-options">
                    ${folderList}
                </div>
                <div class="custom-folder">
                    <label>Custom Path:</label>
                    <input type="text" id="customFolderInput" value="${currentFolder}" placeholder="Enter custom folder path">
                </div>
                <div class="folder-actions">
                    <button id="selectCustomFolder" class="btn btn-primary">Use Custom Path</button>
                    <button id="cancelFolderDialog" class="btn btn-secondary">Cancel</button>
                </div>
            </div>
        `;
        
        // Show in a notification-style popup
        const dialog = this.ui.showInfo(dialogContent, 0); // 0 = no auto-close
        
        // Add click handlers for folder options
        setTimeout(() => {
            const folderOptions = document.querySelectorAll('.folder-option');
            folderOptions.forEach(option => {
                option.addEventListener('click', () => {
                    const folder = option.dataset.folder;
                    outputFolder.value = folder;
                    this.ui.removeNotification(dialog);
                    this.logActivity('info', `Output folder changed to: ${folder}`);
                });
            });
            
            const selectCustomBtn = document.getElementById('selectCustomFolder');
            if (selectCustomBtn) {
                selectCustomBtn.addEventListener('click', () => {
                    const customInput = document.getElementById('customFolderInput');
                    if (customInput && customInput.value.trim()) {
                        outputFolder.value = customInput.value.trim();
                        this.ui.removeNotification(dialog);
                        this.logActivity('info', `Output folder changed to: ${customInput.value.trim()}`);
                    }
                });
            }
            
            const cancelBtn = document.getElementById('cancelFolderDialog');
            if (cancelBtn) {
                cancelBtn.addEventListener('click', () => {
                    this.ui.removeNotification(dialog);
                });
            }
        }, 100);
    }

    createNewFolder() {
        const outputFolder = document.getElementById('outputFolder');
        const timestamp = new Date().toISOString().slice(0, 16).replace(/[-:]/g, '').replace('T', '_');
        const newFolder = `captures_${timestamp}`;
        
        outputFolder.value = newFolder;
        this.logActivity('info', `Created new folder name: ${newFolder}`);
        this.ui.showSuccess(`New folder name set: ${newFolder}`);
    }

    getCurrentOutputFolder() {
        const outputFolder = document.getElementById('outputFolder');
        const folderValue = outputFolder ? outputFolder.value.trim() || 'captured_images' : 'captured_images';
        console.log('🔍 Debug: getCurrentOutputFolder() - Element:', outputFolder);
        console.log('🔍 Debug: getCurrentOutputFolder() - Raw value:', outputFolder ? outputFolder.value : 'null');
        console.log('🔍 Debug: getCurrentOutputFolder() - Final value:', folderValue);
        return folderValue;
    }

    // 🎉 Show photo capture success with path hint
    showFolderPopup(folderPath, imageCount) {
        // Convert backslashes to Unix-style forward slashes
        const unixPath = folderPath.replace(/\\/g, '/');
        
        const popupContent = `
            <div class="folder-popup">
                <div class="popup-header">
                    <span class="popup-icon">📸✨</span>
                    <h4>Photos Captured Successfully!</h4>
                </div>
                <div class="popup-content">
                    <div class="capture-summary">
                        <strong>${imageCount}</strong> new photo${imageCount > 1 ? 's' : ''} saved
                    </div>
                    <div class="folder-path">
                        <span class="folder-icon">📁</span>
                        <span class="path-text">${unixPath}</span>
                    </div>
                    <div class="path-hint">
                        💡 Tip: Copy the path above to navigate to your photos
                    </div>
                </div>
                <div class="popup-actions">
                    <button id="copyPathBtn" class="btn btn-primary">
                        <span class="btn-icon">📋</span>
                        Copy Path
                    </button>
                    <button id="dismissPopupBtn" class="btn btn-secondary">
                        <span class="btn-icon">✕</span>
                        Dismiss
                    </button>
                </div>
            </div>
        `;

        // Show the popup with auto-dismiss after 10 seconds
        const popup = this.ui.showInfo(popupContent, 10000);

        // Add handlers after a brief delay to ensure DOM is ready
        setTimeout(() => {
            const copyPathBtn = document.getElementById('copyPathBtn');
            const dismissBtn = document.getElementById('dismissPopupBtn');

            if (copyPathBtn) {
                copyPathBtn.addEventListener('click', () => {
                    this.copyToClipboard(unixPath);
                    this.ui.showSuccess('📋 Folder path copied to clipboard!');
                });
            }

            if (dismissBtn) {
                dismissBtn.addEventListener('click', () => {
                    this.ui.removeNotification(popup);
                });
            }
        }, 100);

        // Log the folder popup event
        this.logActivity('info', `📸 Capture complete popup shown for ${unixPath}`);
    }

    // Helper function to open folder in system file explorer
    openFolder(folderPath) {
        try {
            // For Windows - open Explorer
            if (navigator.platform.toLowerCase().includes('win')) {
                // Try to open with Windows Explorer
                const fullPath = folderPath.replace(/\//g, '\\');
                window.open(`file:///${fullPath}`, '_blank');
            } else {
                // For other platforms
                window.open(`file://${folderPath}`, '_blank');
            }
            
            this.logActivity('success', `📂 Opened folder: ${folderPath}`);
        } catch (error) {
            console.error('Failed to open folder:', error);
            this.ui.showWarning('Unable to open folder directly. Please navigate manually.');
            this.logActivity('warning', `Failed to open folder: ${error.message}`);
        }
    }

    // Helper function to copy text to clipboard
    async copyToClipboard(text) {
        try {
            if (navigator.clipboard && window.isSecureContext) {
                await navigator.clipboard.writeText(text);
            } else {
                // Fallback for older browsers
                const textArea = document.createElement('textarea');
                textArea.value = text;
                textArea.style.position = 'fixed';
                textArea.style.opacity = '0';
                document.body.appendChild(textArea);
                textArea.focus();
                textArea.select();
                document.execCommand('copy');
                document.body.removeChild(textArea);
            }
            
            this.logActivity('info', `📋 Copied to clipboard: ${text}`);
        } catch (error) {
            console.error('Failed to copy to clipboard:', error);
            this.ui.showWarning('Failed to copy path to clipboard');
        }
    }
}

// Export for use in other modules
window.Controls = Controls; 