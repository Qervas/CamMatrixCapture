using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using CameraMatrixCapture.Services;
using System;
using System.Diagnostics;

namespace CameraMatrixCapture.Pages;

public sealed partial class CapturePage : Page
{
    private readonly DispatcherTimer _refreshTimer;
    private bool _isCapturing = false;
    private bool _isManualMode = false;
    private int _totalPositions = 36;
    private float _angleStep = 10f;
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
        _sessionName = SetupPage.CurrentSessionName;
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
            ProgressText.Text = $"Capturing... {percent}%";
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
                    CaptureButton.IsEnabled = false;

                    int totalImages = _totalPositions * CaptureService.ConnectedCameraCount;
                    CompleteText.Text = $"Captured {_totalPositions} positions ({totalImages} images)";
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
        }
        else
        {
            // Start automated capture
            _isCapturing = true;
            _sessionStarted = true;
            UpdateButtonState();
            ProgressText.Text = "Starting capture...";
            SessionCompletePanel.Visibility = Visibility.Collapsed;

            CaptureService.StartCapture(_sessionName, _totalPositions, _angleStep);
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
        // Allow user to continue capturing in the same session
        SessionCompletePanel.Visibility = Visibility.Collapsed;
        CaptureButton.IsEnabled = true;

        // Reset for another round but keep session
        CaptureProgressBar.Value = 0;
        ProgressDetailText.Text = $"0 / {_totalPositions} positions";
        ProgressText.Text = "Ready to continue";
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
        if (!string.IsNullOrEmpty(_sessionPath))
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "explorer.exe",
                    Arguments = _sessionPath,
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
}
