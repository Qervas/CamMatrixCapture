using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using CameraMatrixCapture.Services;
using System;
using System.Text;

namespace CameraMatrixCapture.Dialogs;

public sealed partial class DebugConsoleDialog : ContentDialog
{
    private readonly DispatcherTimer _refreshTimer;
    private readonly StringBuilder _logBuilder = new();
    private int _lineCount = 0;

    public DebugConsoleDialog()
    {
        this.InitializeComponent();

        // Set up auto-refresh timer
        _refreshTimer = new DispatcherTimer();
        _refreshTimer.Interval = TimeSpan.FromMilliseconds(500);
        _refreshTimer.Tick += RefreshTimer_Tick;

        // Load initial logs
        RefreshLogs();

        // Start timer when dialog opens
        this.Opened += (s, e) => _refreshTimer.Start();
        this.Closed += (s, e) => _refreshTimer.Stop();
    }

    private void RefreshTimer_Tick(object? sender, object e)
    {
        RefreshLogs();
    }

    private void RefreshLogs()
    {
        try
        {
            // Get logs from CaptureService
            string newLogs = CaptureService.GetDebugLogs();

            if (!string.IsNullOrEmpty(newLogs) && newLogs != _logBuilder.ToString())
            {
                _logBuilder.Clear();
                _logBuilder.Append(newLogs);
                LogTextBlock.Text = newLogs;

                _lineCount = newLogs.Split('\n').Length;
                LineCountText.Text = $"{_lineCount} lines";
                StatusText.Text = $"Last updated: {DateTime.Now:HH:mm:ss}";

                // Auto-scroll if enabled
                if (AutoScrollCheck.IsChecked == true)
                {
                    LogScrollViewer.ChangeView(null, LogScrollViewer.ScrollableHeight, null);
                }
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Error: {ex.Message}";
        }
    }

    private void ClearButton_Click(object sender, RoutedEventArgs e)
    {
        CaptureService.ClearDebugLogs();
        _logBuilder.Clear();
        LogTextBlock.Text = "";
        _lineCount = 0;
        LineCountText.Text = "0 lines";
        StatusText.Text = "Logs cleared";
    }

    private void RefreshButton_Click(object sender, RoutedEventArgs e)
    {
        RefreshLogs();
    }
}
