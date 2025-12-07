using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using CameraMatrixCapture.Services;
using System;

namespace CameraMatrixCapture.Pages;

public sealed partial class CapturePage : Page
{
    private readonly DispatcherTimer _refreshTimer;
    private bool _isCapturing = false;
    private int _totalPositions = 36;
    private float _angleStep = 10f;
    private string _sessionName = "";

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

        // Get settings from previous page (SetupPage)
        // In a real app, this would be passed via a navigation parameter or view model
        // For now, use defaults
        _totalPositions = 36;
        _angleStep = 10f;

        ProgressDetailText.Text = $"0 / {_totalPositions} positions";
        CaptureProgressBar.Maximum = _totalPositions;
    }

    private void RefreshStatus(object? sender, object e)
    {
        if (_isCapturing)
        {
            int progress = CaptureService.CaptureProgress;
            int total = CaptureService.TotalPositions;
            float angle = CaptureService.CurrentAngle;
            int images = progress * CaptureService.ConnectedCameraCount;

            CaptureProgressBar.Value = progress;
            ProgressDetailText.Text = $"{progress} / {total} positions";
            AngleText.Text = $"{angle:F1}°";
            ImagesText.Text = images.ToString();

            int percent = total > 0 ? (progress * 100 / total) : 0;
            ProgressText.Text = $"Capturing... {percent}%";
        }
    }

    private void OnProgress(int current, int total)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            CaptureProgressBar.Value = current;
            ProgressDetailText.Text = $"{current} / {total} positions";
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
                ProgressText.Text = "Capture complete!";

                // Navigate to Done page
                if (this.Frame != null)
                {
                    this.Frame.Navigate(typeof(DonePage));
                }
            }
            else
            {
                ProgressText.Text = "Capture failed";
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
            // Start capture
            _isCapturing = true;
            UpdateButtonState();
            ProgressText.Text = "Starting capture...";

            CaptureService.StartCapture(_sessionName, _totalPositions, _angleStep);
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
}
