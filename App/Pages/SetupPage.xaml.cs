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
    public float TurntableSpeed { get; private set; } = 40f;  // Fast by default
    public bool IsManualMode => ManualMode?.IsChecked == true;

    // Static properties to pass settings between pages
    public static int CurrentTotalPositions { get; private set; } = 36;
    public static float CurrentAngleStep { get; private set; } = 10f;
    public static float CurrentTotalRotation { get; private set; } = 360f;
    public static float CurrentTurntableSpeed { get; private set; } = 40f;
    public static bool CurrentIsManualMode { get; private set; } = false;
    public static string CurrentOutputPath { get; private set; } = "";

    public SetupPage()
    {
        this.InitializeComponent();

        // Load saved settings from backend before marking initialized
        LoadSavedSettings();

        _isInitialized = true;
        UpdateSummary();
    }

    private void LoadSavedSettings()
    {
        // Load output path
        string savedPath = CaptureService.OutputPath;
        if (!string.IsNullOrEmpty(savedPath) && savedPath != "neural_dataset")
        {
            OutputPathTextBox.Text = savedPath;
            CurrentOutputPath = savedPath;
        }

        // Load capture settings
        TotalPositions = CaptureService.CaptureTotalPositions;
        AngleStep = CaptureService.CaptureAngleStep;
        TotalRotation = CaptureService.CaptureTotalRotation;
        TurntableSpeed = CaptureService.CaptureTurntableSpeed;

        // Load capture mode
        bool isManual = CaptureService.CaptureManualMode;
        if (isManual)
        {
            ManualMode.IsChecked = true;
            AutomatedSettingsPanel.Visibility = Visibility.Collapsed;
            ManualSettingsPanel.Visibility = Visibility.Visible;
        }
        else
        {
            AutomatedMode.IsChecked = true;
            AutomatedSettingsPanel.Visibility = Visibility.Visible;
            ManualSettingsPanel.Visibility = Visibility.Collapsed;
        }

        // Load preset and update UI
        int preset = CaptureService.CapturePreset;
        switch (preset)
        {
            case 1: // Detailed
                DetailedPreset.IsChecked = true;
                CustomSettingsPanel.Visibility = Visibility.Collapsed;
                break;
            case 2: // Custom
                CustomPreset.IsChecked = true;
                CustomSettingsPanel.Visibility = Visibility.Visible;
                // Update number boxes with saved values
                PositionsNumberBox.Value = TotalPositions;
                AngleStepNumberBox.Value = AngleStep;
                TotalRotationNumberBox.Value = TotalRotation;
                break;
            default: // 0 = Quick
                QuickPreset.IsChecked = true;
                CustomSettingsPanel.Visibility = Visibility.Collapsed;
                break;
        }

        // Load turntable speed selection
        int speedIndex = TurntableSpeed switch
        {
            40f => 0,  // Fast
            70f => 1,  // Medium
            100f => 2, // Slow
            _ => 0     // Default to Fast
        };
        TurntableSpeedComboBox.SelectedIndex = speedIndex;

        // Update custom preset text
        UpdateCustomPresetText();

        // Update static properties
        CurrentTotalPositions = TotalPositions;
        CurrentAngleStep = AngleStep;
        CurrentTotalRotation = TotalRotation;
        CurrentTurntableSpeed = TurntableSpeed;
        CurrentIsManualMode = isManual;
    }

    private void SaveSettings()
    {
        // Save all capture settings to backend
        CaptureService.CaptureTotalPositions = TotalPositions;
        CaptureService.CaptureAngleStep = AngleStep;
        CaptureService.CaptureTotalRotation = TotalRotation;
        CaptureService.CaptureTurntableSpeed = TurntableSpeed;
        CaptureService.CaptureManualMode = ManualMode?.IsChecked == true;

        // Determine preset index
        int preset = 0; // Quick
        if (DetailedPreset?.IsChecked == true) preset = 1;
        else if (CustomPreset?.IsChecked == true) preset = 2;
        CaptureService.CapturePreset = preset;

        // Trigger save to file
        CaptureService.SaveSettings();
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
            CaptureService.SaveSettings();
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
        SaveSettings();
    }

    private void Preset_Checked(object sender, RoutedEventArgs e)
    {
        if (!_isInitialized) return;

        if (QuickPreset.IsChecked == true)
        {
            TotalPositions = 36;
            AngleStep = 10f;
            TotalRotation = 360f;
            CustomSettingsPanel.Visibility = Visibility.Collapsed;
        }
        else if (DetailedPreset.IsChecked == true)
        {
            TotalPositions = 72;
            AngleStep = 5f;
            TotalRotation = 360f;
            CustomSettingsPanel.Visibility = Visibility.Collapsed;
        }
        else if (CustomPreset.IsChecked == true)
        {
            TotalPositions = (int)PositionsNumberBox.Value;
            AngleStep = (float)AngleStepNumberBox.Value;
            TotalRotation = (float)TotalRotationNumberBox.Value;
            CustomSettingsPanel.Visibility = Visibility.Visible;
        }

        UpdateSummary();
        SaveSettings();
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
            SaveSettings();
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
            SaveSettings();
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
            SaveSettings();
        }
        finally
        {
            _isUpdating = false;
        }
    }

    private void TurntableSpeed_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (!_isInitialized) return;

        if (TurntableSpeedComboBox.SelectedItem is ComboBoxItem selectedItem)
        {
            if (float.TryParse(selectedItem.Tag?.ToString(), out float speed))
            {
                TurntableSpeed = speed;
                CurrentTurntableSpeed = speed;
            }
        }

        UpdateSummary();
        SaveSettings();
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
        CurrentTurntableSpeed = TurntableSpeed;
        CurrentIsManualMode = isManual;
    }
}
