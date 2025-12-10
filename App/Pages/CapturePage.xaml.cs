using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using CameraMatrixCapture.Services;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace CameraMatrixCapture.Pages;

public sealed partial class CapturePage : Page
{
    // P/Invoke for system sound notification
    [DllImport("winmm.dll", SetLastError = true)]
    private static extern bool PlaySound(string pszSound, IntPtr hmod, uint fdwSound);

    private const uint SND_ALIAS = 0x00010000;
    private const uint SND_ASYNC = 0x0001;

    private readonly DispatcherTimer _refreshTimer;
    private bool _isCapturing = false;
    private bool _isManualMode = false;
    private int _totalPositions = 36;
    private float _angleStep = 10f;
    private float _turntableSpeed = 40f;
    private string _sessionName = "";
    private string _sessionPath = "";
    private int _captureCount = 0;
    private int _totalImages = 0;
    private bool _sessionStarted = false;

    public CapturePage()
    {
        this.InitializeComponent();

        // Subscribe to events
        CaptureService.OnProgress += OnProgress;
        CaptureService.OnCaptureComplete += OnCaptureComplete;

        // Timer to refresh status
        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
        _refreshTimer.Tick += RefreshStatus;
        _refreshTimer.Start();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);

        // Get settings from SetupPage
        _totalPositions = SetupPage.CurrentTotalPositions;
        _angleStep = SetupPage.CurrentAngleStep;
        _turntableSpeed = SetupPage.CurrentTurntableSpeed;
        _isManualMode = SetupPage.CurrentIsManualMode;

        // Setup UI based on mode
        if (_isManualMode)
        {
            ModeTitleText.Text = "Manual Capture";
            AutomatedModePanel.Visibility = Visibility.Collapsed;
            ManualModePanel.Visibility = Visibility.Visible;
        }
        else
        {
            ModeTitleText.Text = "Automated 360° Capture";
            AutomatedModePanel.Visibility = Visibility.Visible;
            ManualModePanel.Visibility = Visibility.Collapsed;
            ProgressDetailText.Text = $"0 / {_totalPositions} positions";
            CaptureProgressBar.Maximum = _totalPositions;
        }

        // Reset state
        _captureCount = 0;
        _totalImages = 0;
        _sessionStarted = false;
        SessionCompletePanel.Visibility = Visibility.Collapsed;
        UpdateStats();

        // Show output folder path from settings
        string outputPath = SetupPage.CurrentOutputPath;
        if (!string.IsNullOrEmpty(outputPath))
        {
            SessionPathText.Text = $"Output: {outputPath}";
            OpenFolderButton.IsEnabled = true;
        }
        else
        {
            SessionPathText.Text = "Output folder not set";
            OpenFolderButton.IsEnabled = false;
        }
    }

    private void RefreshStatus(object? sender, object e)
    {
        if (_isCapturing && !_isManualMode)
        {
            int progress = CaptureService.CaptureProgress;
            int total = CaptureService.TotalPositions;
            float angle = CaptureService.CurrentAngle;
            int images = progress * CaptureService.ConnectedCameraCount;

            CaptureProgressBar.Value = progress;
            ProgressDetailText.Text = $"{progress} / {total} positions";
            _captureCount = progress;
            _totalImages = images;
            UpdateStats();

            int percent = total > 0 ? (progress * 100 / total) : 0;
            int state = CaptureService.CaptureState;
            string stateText = state switch
            {
                1 => "Capturing",
                2 => "Rotating",
                3 => "Settling",
                _ => "Working"
            };
            ProgressText.Text = $"{stateText}... {percent}%";

            // Update timing display
            UpdateTimingDisplay();
        }

        // Update session path if available
        if (_sessionStarted)
        {
            string path = CaptureService.LastSessionPath;
            if (!string.IsNullOrEmpty(path) && path != _sessionPath)
            {
                _sessionPath = path;
                SessionPathText.Text = path;
                OpenFolderButton.IsEnabled = true;
            }
        }

        // Update current angle display in manual mode
        if (_isManualMode)
        {
            UpdateCurrentAngleDisplay();
        }
    }

    private void UpdateTimingDisplay()
    {
        int state = CaptureService.CaptureState;
        int captureElapsed = CaptureService.CaptureElapsedMs;
        int rotateElapsed = CaptureService.RotateElapsedMs;
        int totalCaptureMs = CaptureService.TotalCaptureTimeMs;
        int totalRotateMs = CaptureService.TotalRotateTimeMs;

        // Update state text and current phase time
        switch (state)
        {
            case 1:
                StateText.Text = "Capturing";
                StateTimeText.Text = $"{captureElapsed / 1000.0:F1}s";
                break;
            case 2:
                StateText.Text = "Rotating";
                StateTimeText.Text = $"{rotateElapsed / 1000.0:F1}s";
                break;
            case 3:
                StateText.Text = "Settling";
                StateTimeText.Text = "";
                break;
            default:
                StateText.Text = "Idle";
                StateTimeText.Text = "";
                break;
        }

        // Update total times
        TotalCaptureTimeText.Text = $"{totalCaptureMs / 1000.0:F1}s";
        TotalRotateTimeText.Text = $"{totalRotateMs / 1000.0:F1}s";
    }

    private void OnProgress(int current, int total)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            CaptureProgressBar.Value = current;
            ProgressDetailText.Text = $"{current} / {total} positions";
            _captureCount = current;
            _totalImages = current * CaptureService.ConnectedCameraCount;
            UpdateStats();
        });
    }

    private void OnCaptureComplete(bool success, string sessionPath)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            _isCapturing = false;
            UpdateButtonState();

            // Play notification sound
            try
            {
                // Play system notification sound (SystemAsterisk for success, SystemHand for error)
                PlaySound(success ? "SystemAsterisk" : "SystemHand", IntPtr.Zero, SND_ALIAS | SND_ASYNC);
            }
            catch { /* Ignore sound errors */ }

            // Update final timing
            int totalCaptureMs = CaptureService.TotalCaptureTimeMs;
            int totalRotateMs = CaptureService.TotalRotateTimeMs;
            StateText.Text = "Complete";
            StateTimeText.Text = "";
            TotalCaptureTimeText.Text = $"{totalCaptureMs / 1000.0:F1}s";
            TotalRotateTimeText.Text = $"{totalRotateMs / 1000.0:F1}s";

            if (success)
            {
                _sessionPath = sessionPath;
                SessionPathText.Text = sessionPath;
                OpenFolderButton.IsEnabled = true;

                if (_isManualMode)
                {
                    // In manual mode, just update status - user can keep capturing
                    ManualStatusText.Text = "Captured! Click again to take more";
                }
                else
                {
                    // In automated mode, show completion panel with options
                    ProgressText.Text = "Capture complete!";
                    SessionCompletePanel.Visibility = Visibility.Visible;
                    SessionNameTextBox.IsEnabled = false;  // Keep visible but disabled
                    CaptureButton.IsEnabled = false;

                    int totalImages = _totalPositions * CaptureService.ConnectedCameraCount;
                    int totalTimeMs = totalCaptureMs + totalRotateMs;
                    CompleteText.Text = $"Captured {_totalPositions} positions ({totalImages} images) in {totalTimeMs / 1000.0:F1}s";
                }
            }
            else
            {
                if (_isManualMode)
                {
                    ManualStatusText.Text = "Capture failed - try again";
                }
                else
                {
                    ProgressText.Text = "Capture failed";
                }
            }
        });
    }

    private void CaptureButton_Click(object sender, RoutedEventArgs e)
    {
        if (_isCapturing)
        {
            // Stop capture
            CaptureService.StopCapture();
            _isCapturing = false;
            UpdateButtonState();
            ProgressText.Text = "Capture stopped";
            TimingPanel.Visibility = Visibility.Collapsed;
            SessionNameTextBox.IsEnabled = true;  // Re-enable editing
        }
        else
        {
            // Get session name from textbox
            _sessionName = SessionNameTextBox.Text?.Trim() ?? "";

            // Start automated capture
            _isCapturing = true;
            _sessionStarted = true;
            UpdateButtonState();
            ProgressText.Text = "Starting capture...";
            SessionCompletePanel.Visibility = Visibility.Collapsed;
            TimingPanel.Visibility = Visibility.Visible;
            SessionNameTextBox.IsEnabled = false;  // Keep visible but disable editing

            // Reset timing display
            StateText.Text = "Starting...";
            StateTimeText.Text = "";
            TotalCaptureTimeText.Text = "0.0s";
            TotalRotateTimeText.Text = "0.0s";

            CaptureService.StartCapture(_sessionName, _totalPositions, _angleStep, _turntableSpeed);
        }
    }

    private void ManualCaptureButton_Click(object sender, RoutedEventArgs e)
    {
        if (!_sessionStarted)
        {
            // First capture - create session
            _sessionStarted = true;
            CaptureService.CreateSession(_sessionName);
        }

        ManualStatusText.Text = "Capturing...";
        ManualCaptureButton.IsEnabled = false;

        // Capture with all cameras
        CaptureService.CaptureOnce(() =>
        {
            DispatcherQueue.TryEnqueue(() =>
            {
                _captureCount++;
                int cameraCount = CaptureService.ConnectedCameraCount;
                if (cameraCount == 0) cameraCount = 12;
                _totalImages = _captureCount * cameraCount;
                UpdateStats();

                ManualStatusText.Text = "Captured! Click again to take more";
                ManualCaptureButton.IsEnabled = true;

                // Update session path
                _sessionPath = CaptureService.LastSessionPath;
                if (!string.IsNullOrEmpty(_sessionPath))
                {
                    SessionPathText.Text = _sessionPath;
                    OpenFolderButton.IsEnabled = true;
                }
            });
        });
    }

    private void ContinueCaptureButton_Click(object sender, RoutedEventArgs e)
    {
        // Prepare for a new capture session
        SessionCompletePanel.Visibility = Visibility.Collapsed;
        CaptureButton.IsEnabled = true;
        SessionNameTextBox.IsEnabled = true;  // Re-enable editing

        // Clear session name for new input
        SessionNameTextBox.Text = "";

        // Reset for another round
        CaptureProgressBar.Value = 0;
        ProgressDetailText.Text = $"0 / {_totalPositions} positions";
        ProgressText.Text = "Enter session name and click Start";
        TimingPanel.Visibility = Visibility.Collapsed;

        // Reset session state
        _sessionStarted = false;
        _sessionPath = "";

        // Show output folder path
        string outputPath = SetupPage.CurrentOutputPath;
        SessionPathText.Text = !string.IsNullOrEmpty(outputPath)
            ? $"Output: {outputPath}"
            : "Output folder not set";
    }

    private void NewSessionButton_Click(object sender, RoutedEventArgs e)
    {
        // Go back to Setup page to start a new session
        if (App.MainWindow is MainWindow mainWindow)
        {
            mainWindow.NavigateToStep(2); // Step 2 is Setup
        }
    }

    private void OpenFolderButton_Click(object sender, RoutedEventArgs e)
    {
        // Get the current output path from CaptureService
        string pathToOpen = _sessionPath;

        // If no session path, try to get the base output folder
        if (string.IsNullOrEmpty(pathToOpen))
        {
            pathToOpen = CaptureService.OutputPath;
        }

        // Convert to absolute path using C++ backend's working directory
        pathToOpen = CaptureService.GetAbsolutePath(pathToOpen);

        if (!string.IsNullOrEmpty(pathToOpen))
        {
            try
            {
                // Make sure the path exists
                if (!System.IO.Directory.Exists(pathToOpen))
                {
                    System.IO.Directory.CreateDirectory(pathToOpen);
                }

                Debug.WriteLine($"Opening folder: {pathToOpen}");
                Process.Start(new ProcessStartInfo
                {
                    FileName = "explorer.exe",
                    Arguments = $"\"{pathToOpen}\"",
                    UseShellExecute = true
                });
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to open folder: {ex.Message}");
            }
        }
    }

    private void UpdateButtonState()
    {
        if (_isCapturing)
        {
            CaptureIcon.Glyph = "\uE71A"; // Stop icon
            CaptureButtonText.Text = "Stop";
        }
        else
        {
            CaptureIcon.Glyph = "\uE768"; // Play icon
            CaptureButtonText.Text = "Start";
        }
    }

    private void UpdateStats()
    {
        CaptureCountText.Text = _captureCount.ToString();
        ImagesText.Text = _totalImages.ToString();
    }

    private void SessionNameTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        // Optional: Update UI based on session name input
        // For now, just store the value
        _sessionName = SessionNameTextBox.Text?.Trim() ?? "";
    }

    #region Turntable Controls

    private float GetSelectedRotationStep()
    {
        if (RotationStepComboBox.SelectedItem is ComboBoxItem item &&
            float.TryParse(item.Tag?.ToString(), out float step))
        {
            return step;
        }
        return 10f; // Default 10°
    }

    private void RotateLeftButton_Click(object sender, RoutedEventArgs e)
    {
        float step = GetSelectedRotationStep();
        SetTurntableButtonsEnabled(false);
        ManualStatusText.Text = $"Rotating -{step}°...";

        // Rotate counter-clockwise (negative angle)
        bool success = CaptureService.RotateTurntable(-step);

        if (!success)
        {
            ManualStatusText.Text = "Rotation failed - check turntable connection";
            SetTurntableButtonsEnabled(true);
            return;
        }

        // Re-enable after delay (estimate based on rotation)
        _ = ReenableTurntableButtonsAfterDelay((int)(step * 100)); // ~100ms per degree
    }

    private void RotateRightButton_Click(object sender, RoutedEventArgs e)
    {
        float step = GetSelectedRotationStep();
        SetTurntableButtonsEnabled(false);
        ManualStatusText.Text = $"Rotating +{step}°...";

        // Rotate clockwise (positive angle)
        bool success = CaptureService.RotateTurntable(step);

        if (!success)
        {
            ManualStatusText.Text = "Rotation failed - check turntable connection";
            SetTurntableButtonsEnabled(true);
            return;
        }

        // Re-enable after delay
        _ = ReenableTurntableButtonsAfterDelay((int)(step * 100));
    }

    private void ReturnToZeroButton_Click(object sender, RoutedEventArgs e)
    {
        SetTurntableButtonsEnabled(false);
        ManualStatusText.Text = "Returning to zero...";

        bool success = CaptureService.ReturnToZero();

        if (!success)
        {
            ManualStatusText.Text = "Return to zero failed - check turntable connection";
            SetTurntableButtonsEnabled(true);
            return;
        }

        // Re-enable after delay (allow time for full rotation back)
        _ = ReenableTurntableButtonsAfterDelay(5000);
    }

    private void SetTurntableButtonsEnabled(bool enabled)
    {
        RotateLeftButton.IsEnabled = enabled;
        RotateRightButton.IsEnabled = enabled;
        ReturnToZeroButton.IsEnabled = enabled;
    }

    private async System.Threading.Tasks.Task ReenableTurntableButtonsAfterDelay(int delayMs)
    {
        await System.Threading.Tasks.Task.Delay(delayMs);

        DispatcherQueue.TryEnqueue(() =>
        {
            SetTurntableButtonsEnabled(true);
            ManualStatusText.Text = "Ready - Click to capture";
            UpdateCurrentAngleDisplay();
        });
    }

    private void UpdateCurrentAngleDisplay()
    {
        float angle = CaptureService.CurrentAngle;
        if (angle >= 0)
        {
            CurrentAngleText.Text = $"Current: {angle:F1}°";
        }
    }

    #endregion
}
