using Microsoft.UI.Xaml;
using CameraMatrixCapture.Services;

namespace CameraMatrixCapture;

public partial class App : Application
{
    private Window? m_window;

    public static Window? MainWindow { get; private set; }

    public App()
    {
        this.InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        // Initialize backend
        CaptureService.Initialize();

        m_window = new MainWindow();
        MainWindow = m_window;
        m_window.Activate();
    }
}
