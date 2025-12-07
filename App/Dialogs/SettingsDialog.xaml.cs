using Microsoft.UI.Xaml.Controls;
using CameraMatrixCapture.Services;
using Windows.Storage.Pickers;
using System;

namespace CameraMatrixCapture.Dialogs;

public sealed partial class SettingsDialog : ContentDialog
{
    public SettingsDialog()
    {
        this.InitializeComponent();
        LoadSettings();

        // Update value text on slider change
        ExposureSlider.ValueChanged += (s, e) => ExposureValueText.Text = $"{(int)ExposureSlider.Value} μs";
        GainSlider.ValueChanged += (s, e) => GainValueText.Text = $"{GainSlider.Value:F1} dB";
    }

    private void LoadSettings()
    {
        ExposureSlider.Value = CaptureService.ExposureTime;
        GainSlider.Value = CaptureService.Gain;
        ParallelGroupsBox.Value = CaptureService.ParallelGroups;
        StaggerDelayBox.Value = CaptureService.StaggerDelay;
        OutputPathBox.Text = CaptureService.OutputPath;

        ExposureValueText.Text = $"{CaptureService.ExposureTime} μs";
        GainValueText.Text = $"{CaptureService.Gain:F1} dB";
    }

    private void SaveButton_Click(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        CaptureService.ExposureTime = (int)ExposureSlider.Value;
        CaptureService.Gain = (float)GainSlider.Value;
        CaptureService.ParallelGroups = (int)ParallelGroupsBox.Value;
        CaptureService.StaggerDelay = (int)StaggerDelayBox.Value;
        CaptureService.OutputPath = OutputPathBox.Text;

        CaptureService.SaveSettings();
    }

    private async void BrowseButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var folderPicker = new FolderPicker();
        folderPicker.SuggestedStartLocation = PickerLocationId.DocumentsLibrary;
        folderPicker.FileTypeFilter.Add("*");

        // Get the main window handle for the picker
        var mainWindow = App.MainWindow;
        if (mainWindow != null)
        {
            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(mainWindow);
            WinRT.Interop.InitializeWithWindow.Initialize(folderPicker, hwnd);
        }

        var folder = await folderPicker.PickSingleFolderAsync();
        if (folder != null)
        {
            OutputPathBox.Text = folder.Path;
        }
    }
}
