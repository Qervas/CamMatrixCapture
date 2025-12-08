using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using CameraMatrixCapture.Services;
using System;
using System.Diagnostics;

namespace CameraMatrixCapture.Pages;

public sealed partial class DonePage : Page
{
    public DonePage()
    {
        this.InitializeComponent();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);

        // Get session info from service
        string sessionPath = CaptureService.LastSessionPath;
        int imageCount = CaptureService.LastSessionImageCount;

        ImageCountText.Text = $"{imageCount} images saved";
        PathText.Text = sessionPath;
    }

    private void OpenFolderButton_Click(object sender, RoutedEventArgs e)
    {
        string path = CaptureService.LastSessionPath;

        // Convert to absolute path using C++ backend's working directory
        path = CaptureService.GetAbsolutePath(path);

        if (!string.IsNullOrEmpty(path))
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "explorer.exe",
                    Arguments = $"\"{path}\"",
                    UseShellExecute = true
                });
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to open folder: {ex.Message}");
            }
        }
    }

    private void NewCaptureButton_Click(object sender, RoutedEventArgs e)
    {
        // Navigate back to Connect page (Step 1)
        // The MainWindow handles this via the "New Capture" button
        // But we can also navigate directly
        if (this.Frame != null)
        {
            this.Frame.Navigate(typeof(ConnectPage));
        }
    }
}
