using Microsoft.UI.Xaml;
using CameraMatrixCapture.Services;
using System;

namespace CameraMatrixCapture;

public partial class App : Application
{
    private Window? m_window;

    public static Window? MainWindow { get; private set; }

    public App()
    {
        this.InitializeComponent();

        // Register for unhandled exceptions to ensure cleanup
        this.UnhandledException += App_UnhandledException;
        AppDomain.CurrentDomain.ProcessExit += CurrentDomain_ProcessExit;
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        // Initialize backend
        CaptureService.Initialize();

        m_window = new MainWindow();
        MainWindow = m_window;

        // Handle window closed event for cleanup
        m_window.Closed += MainWindow_Closed;

        m_window.Activate();
    }

    private void MainWindow_Closed(object sender, WindowEventArgs args)
    {
        // Cleanup backend resources when window closes
        CaptureService.Shutdown();
    }

    private void CurrentDomain_ProcessExit(object? sender, EventArgs e)
    {
        // Ensure cleanup on process exit
        CaptureService.Shutdown();
    }

    private void App_UnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        // Try to cleanup even on crash
        try
        {
            CaptureService.Shutdown();
        }
        catch
        {
            // Ignore cleanup errors during crash
        }
    }
}
