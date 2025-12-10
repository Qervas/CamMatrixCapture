using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using CameraMatrixCapture.Services;
using Windows.Storage.Pickers;
using Windows.Storage;
using System;

namespace CameraMatrixCapture.Pages;

public sealed partial class SetupPage : Page
{
    private bool _isInitialized = false;
    private bool _isUpdating = false;  // Prevent recursive updates

    // Properties accessible from CapturePage
    public int TotalPositions { get; private set; } = 36;
    public float AngleStep { get; private set; } = 10f;
    public float TotalRotation { get; private set; } = 360f;
    public bool IsManualMode => ManualMode?.IsChecked == true;

    // Static properties to pass settings between pages
    public static int CurrentTotalPositions { get; private set; } = 36;
    public static float CurrentAngleStep { get; private set; } = 10f;
    public static float CurrentTotalRotation { get; private set; } = 360f;
    public static bool CurrentIsManualMode { get; private set; } = false;
    public static string CurrentOutputPath { get; private set; } = "";

    public SetupPage()
    {
        this.InitializeComponent();
        _isInitialized = true;

        // Load previously selected path if any (but don't set a default)
        string savedPath = CaptureService.OutputPath;
        if (!string.IsNullOrEmpty(savedPath) && savedPath != "neural_dataset")
        {
            OutputPathTextBox.Text = savedPath;
            CurrentOutputPath = savedPath;
        }

        UpdateSummary();
    }

    private async void BrowseButton_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FolderPicker();
        picker.SuggestedStartLocation = PickerLocationId.Desktop;
        picker.FileTypeFilter.Add("*");

        // Get the window handle for the picker
        var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(App.MainWindow);
        WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);

        StorageFolder folder = await picker.PickSingleFolderAsync();
        if (folder != null)
        {
            OutputPathTextBox.Text = folder.Path;
            CurrentOutputPath = folder.Path;
            CaptureService.OutputPath = folder.Path;
            OutputPathError.Visibility = Visibility.Collapsed;
            UpdateSummary();
        }
    }

    public bool ValidateOutputPath()
    {
        if (string.IsNullOrEmpty(CurrentOutputPath))
        {
            OutputPathError.Visibility = Visibility.Visible;
            return false;
        }
        OutputPathError.Visibility = Visibility.Collapsed;
        return true;
    }

    private void CaptureMode_Checked(object sender, RoutedEventArgs e)
    {
        if (!_isInitialized) return;

        bool isManual = ManualMode.IsChecked == true;

        AutomatedSettingsPanel.Visibility = isManual ? Visibility.Collapsed : Visibility.Visible;
        ManualSettingsPanel.Visibility = isManual ? Visibility.Visible : Visibility.Collapsed;

        UpdateSummary();
    }

    private void Preset_Checked(object sender, RoutedEventArgs e)
    {
        if (!_isInitialized) return;

        if (QuickPreset.IsChecked == true)
        {
            TotalPositions = 36;
            AngleStep = 10f;
            CustomSettingsPanel.Visibility = Visibility.Collapsed;
        }
        else if (DetailedPreset.IsChecked == true)
        {
            TotalPositions = 72;
            AngleStep = 5f;
            CustomSettingsPanel.Visibility = Visibility.Collapsed;
        }
        else if (CustomPreset.IsChecked == true)
        {
            TotalPositions = (int)PositionsNumberBox.Value;
            AngleStep = (float)AngleStepNumberBox.Value;
            CustomSettingsPanel.Visibility = Visibility.Visible;
        }

        UpdateSummary();
    }

    private void TotalRotation_Changed(NumberBox sender, NumberBoxValueChangedEventArgs args)
    {
        if (!_isInitialized || _isUpdating) return;
        if (double.IsNaN(args.NewValue)) return;

        _isUpdating = true;
        try
        {
            TotalRotation = (float)args.NewValue;

            // Recalculate angle step based on current positions
            if (TotalPositions > 0)
            {
                AngleStep = TotalRotation / TotalPositions;
                AngleStepNumberBox.Value = Math.Round(AngleStep, 2);
            }

            UpdateCustomPresetText();
            UpdateSummary();
        }
        finally
        {
            _isUpdating = false;
        }
    }

    private void Positions_Changed(NumberBox sender, NumberBoxValueChangedEventArgs args)
    {
        if (!_isInitialized || _isUpdating) return;
        if (double.IsNaN(args.NewValue)) return;

        _isUpdating = true;
        try
        {
            TotalPositions = (int)args.NewValue;

            // Recalculate angle step: total rotation / positions
            if (TotalPositions > 0)
            {
                TotalRotation = (float)TotalRotationNumberBox.Value;
                AngleStep = TotalRotation / TotalPositions;
                AngleStepNumberBox.Value = Math.Round(AngleStep, 2);
            }

            UpdateCustomPresetText();
            UpdateSummary();
        }
        finally
        {
            _isUpdating = false;
        }
    }

    private void AngleStep_Changed(NumberBox sender, NumberBoxValueChangedEventArgs args)
    {
        if (!_isInitialized || _isUpdating) return;
        if (double.IsNaN(args.NewValue)) return;

        _isUpdating = true;
        try
        {
            AngleStep = (float)args.NewValue;

            // Recalculate positions: total rotation / angle step
            if (AngleStep > 0)
            {
                TotalRotation = (float)TotalRotationNumberBox.Value;
                TotalPositions = (int)Math.Round(TotalRotation / AngleStep);
                if (TotalPositions < 2) TotalPositions = 2;
                PositionsNumberBox.Value = TotalPositions;
            }

            UpdateCustomPresetText();
            UpdateSummary();
        }
        finally
        {
            _isUpdating = false;
        }
    }

    private void UpdateCustomPresetText()
    {
        if (CustomPositionsText != null)
            CustomPositionsText.Text = $"{TotalPositions} positions";
        if (CustomStepsText != null)
            CustomStepsText.Text = $"{AngleStep:F1}° steps";
    }

    private void UpdateSummary()
    {
        if (!_isInitialized || SummaryText == null) return;

        int cameraCount = CaptureService.ConnectedCameraCount;
        if (cameraCount == 0) cameraCount = 12; // Default assumption

        bool isManual = ManualMode?.IsChecked == true;

        string outputInfo = string.IsNullOrEmpty(CurrentOutputPath)
            ? "Output: Not selected"
            : $"Output: {CurrentOutputPath}";

        if (isManual)
        {
            SummaryText.Text = $"Manual capture mode\n" +
                               $"{cameraCount} cameras connected\n" +
                               $"Capture images on demand\n" +
                               outputInfo;
        }
        else
        {
            int totalImages = cameraCount * TotalPositions;

            // Rough time estimate: ~5 seconds per position
            int estimatedMinutes = TotalPositions * 5 / 60;
            string timeEstimate = estimatedMinutes > 0 ? $"~{estimatedMinutes} minutes" : "< 1 minute";

            // Show total rotation info for custom preset
            string rotationInfo = CustomPreset?.IsChecked == true
                ? $"{TotalRotation:F0}° rotation, "
                : "";

            SummaryText.Text = $"{rotationInfo}{TotalPositions} positions at {AngleStep:F1}° intervals\n" +
                               $"{cameraCount} cameras × {TotalPositions} positions = {totalImages} images\n" +
                               $"Estimated time: {timeEstimate}\n" +
                               outputInfo;
        }

        // Update static properties for CapturePage to access
        CurrentTotalPositions = TotalPositions;
        CurrentAngleStep = AngleStep;
        CurrentTotalRotation = TotalRotation;
        CurrentIsManualMode = isManual;
    }
}
