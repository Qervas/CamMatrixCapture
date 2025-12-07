using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using CameraMatrixCapture.Pages;
using CameraMatrixCapture.Dialogs;
using CameraMatrixCapture.Services;
using System;

namespace CameraMatrixCapture;

public sealed partial class MainWindow : Window
{
    private int _currentStep = 1;
    private const int TotalSteps = 4;

    public MainWindow()
    {
        this.InitializeComponent();
        this.Title = "Camera Matrix Capture";

        // Navigate to first page
        NavigateToStep(1);
    }

    private void NavigateToStep(int step)
    {
        _currentStep = step;
        UpdateStepIndicators();
        UpdateNavigationButtons();

        // Navigate to appropriate page
        switch (step)
        {
            case 1:
                ContentFrame.Navigate(typeof(ConnectPage));
                break;
            case 2:
                ContentFrame.Navigate(typeof(SetupPage));
                break;
            case 3:
                ContentFrame.Navigate(typeof(CapturePage));
                break;
            case 4:
                ContentFrame.Navigate(typeof(DonePage));
                break;
        }
    }

    private void UpdateStepIndicators()
    {
        var accentBrush = (SolidColorBrush)Application.Current.Resources["AccentBrush"];
        var inactiveBrush = (SolidColorBrush)Application.Current.Resources["SystemControlBackgroundBaseMediumBrush"]
            ?? new SolidColorBrush(Microsoft.UI.Colors.Gray);

        Step1Circle.Fill = _currentStep >= 1 ? accentBrush : inactiveBrush;
        Step2Circle.Fill = _currentStep >= 2 ? accentBrush : inactiveBrush;
        Step3Circle.Fill = _currentStep >= 3 ? accentBrush : inactiveBrush;
        Step4Circle.Fill = _currentStep >= 4 ? accentBrush : inactiveBrush;
    }

    private void UpdateNavigationButtons()
    {
        BackButton.IsEnabled = _currentStep > 1;

        if (_currentStep == TotalSteps)
        {
            NextButton.Content = "New Capture";
        }
        else
        {
            NextButton.Content = "Next";
        }

        // Enable/disable Next based on current page state
        // This will be controlled by the pages themselves via a public method
    }

    private void BackButton_Click(object sender, RoutedEventArgs e)
    {
        if (_currentStep > 1)
        {
            NavigateToStep(_currentStep - 1);
        }
    }

    private void NextButton_Click(object sender, RoutedEventArgs e)
    {
        if (_currentStep == TotalSteps)
        {
            // "New Capture" - go back to step 1
            NavigateToStep(1);
        }
        else if (_currentStep < TotalSteps)
        {
            NavigateToStep(_currentStep + 1);
        }
    }

    private async void SettingsButton_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new SettingsDialog
        {
            XamlRoot = this.Content.XamlRoot
        };
        await dialog.ShowAsync();
    }

    // Public method for pages to enable/disable Next button
    public void SetNextEnabled(bool enabled)
    {
        NextButton.IsEnabled = enabled;
    }
}
