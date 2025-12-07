using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using CameraMatrixCapture.Services;

namespace CameraMatrixCapture.Pages;

public sealed partial class SetupPage : Page
{
    private bool _isInitialized = false;
    public int TotalPositions { get; private set; } = 36;
    public float AngleStep { get; private set; } = 10f;
    public string SessionName => SessionNameTextBox?.Text ?? "";

    public SetupPage()
    {
        this.InitializeComponent();
        _isInitialized = true;
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

    private void CustomSettings_Changed(NumberBox sender, NumberBoxValueChangedEventArgs args)
    {
        if (!_isInitialized) return;

        if (CustomPreset.IsChecked == true)
        {
            TotalPositions = (int)PositionsNumberBox.Value;
            AngleStep = (float)AngleStepNumberBox.Value;

            CustomPositionsText.Text = $"{TotalPositions} positions";
            CustomStepsText.Text = $"{AngleStep}° steps";

            UpdateSummary();
        }
    }

    private void UpdateSummary()
    {
        if (!_isInitialized || SummaryText == null) return;

        int cameraCount = CaptureService.ConnectedCameraCount;
        if (cameraCount == 0) cameraCount = 12; // Default assumption

        int totalImages = cameraCount * TotalPositions;

        // Rough time estimate: ~5 seconds per position
        int estimatedMinutes = TotalPositions * 5 / 60;
        string timeEstimate = estimatedMinutes > 0 ? $"~{estimatedMinutes} minutes" : "< 1 minute";

        SummaryText.Text = $"{TotalPositions} positions at {AngleStep}° intervals\n" +
                           $"{cameraCount} cameras × {TotalPositions} positions = {totalImages} images\n" +
                           $"Estimated time: {timeEstimate}";
    }
}
