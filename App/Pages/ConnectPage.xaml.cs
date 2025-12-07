using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI;
using CameraMatrixCapture.Services;
using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;

namespace CameraMatrixCapture.Pages;

public class BluetoothDeviceItem
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
}

public sealed partial class ConnectPage : Page
{
    private readonly ObservableCollection<BluetoothDeviceItem> _devices = new();
    private readonly DispatcherTimer _refreshTimer;

    public ConnectPage()
    {
        this.InitializeComponent();
        DeviceListView.ItemsSource = _devices;

        // Set up device discovered callback
        CaptureService.OnDeviceDiscovered += OnDeviceDiscovered;

        // Timer to refresh status
        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
        _refreshTimer.Tick += RefreshStatus;
        _refreshTimer.Start();

        // Initial status update
        UpdateCameraStatus();
        UpdateTurntableStatus();
    }

    private void RefreshStatus(object? sender, object e)
    {
        UpdateCameraStatus();
        UpdateTurntableStatus();
        UpdateNextButtonState();
    }

    private void UpdateCameraStatus()
    {
        int discovered = CaptureService.DiscoveredCameraCount;
        int connected = CaptureService.ConnectedCameraCount;

        CameraCountText.Text = $"{connected} / {discovered} cameras";

        if (CaptureService.IsDiscovering)
        {
            CameraStatusDot.Fill = new SolidColorBrush(Colors.Orange);
            CameraStatusText.Text = "Discovering...";
            CameraProgressBar.Visibility = Visibility.Visible;
            CameraProgressBar.IsIndeterminate = true;
        }
        else if (CaptureService.IsConnecting)
        {
            CameraStatusDot.Fill = new SolidColorBrush(Colors.Orange);
            CameraStatusText.Text = "Connecting...";
            CameraProgressBar.Visibility = Visibility.Visible;
            CameraProgressBar.IsIndeterminate = true;
        }
        else if (connected > 0)
        {
            CameraStatusDot.Fill = new SolidColorBrush(Colors.Green);
            CameraStatusText.Text = "Connected";
            CameraProgressBar.Visibility = Visibility.Collapsed;
        }
        else if (discovered > 0)
        {
            CameraStatusDot.Fill = new SolidColorBrush(Colors.Orange);
            CameraStatusText.Text = "Discovered, not connected";
            CameraProgressBar.Visibility = Visibility.Collapsed;
        }
        else
        {
            CameraStatusDot.Fill = new SolidColorBrush(Colors.Gray);
            CameraStatusText.Text = "Not connected";
            CameraProgressBar.Visibility = Visibility.Collapsed;
        }

        ConnectCamerasButton.IsEnabled = discovered > 0 && !CaptureService.IsConnecting;
        DiscoverButton.IsEnabled = !CaptureService.IsDiscovering;
    }

    private void UpdateTurntableStatus()
    {
        if (CaptureService.IsBluetoothConnected)
        {
            TurntableStatusDot.Fill = new SolidColorBrush(Colors.Green);
            TurntableStatusText.Text = "Connected";
            TurntableNameText.Text = "Turntable ready";
        }
        else if (CaptureService.IsBluetoothScanning)
        {
            TurntableStatusDot.Fill = new SolidColorBrush(Colors.Orange);
            TurntableStatusText.Text = "Scanning...";
            TurntableProgressBar.Visibility = Visibility.Visible;
            TurntableProgressBar.IsIndeterminate = true;
        }
        else
        {
            TurntableStatusDot.Fill = new SolidColorBrush(Colors.Gray);
            TurntableStatusText.Text = "Not connected";
            TurntableProgressBar.Visibility = Visibility.Collapsed;
        }

        ScanButton.IsEnabled = !CaptureService.IsBluetoothScanning;
    }

    private void UpdateNextButtonState()
    {
        // TODO: For production, require both cameras and turntable connected:
        // bool canProceed = CaptureService.ConnectedCameraCount > 0 && CaptureService.IsBluetoothConnected;

        // For testing/preview: always allow Next
        bool canProceed = true;

        // Use the static MainWindow reference
        if (App.MainWindow is MainWindow mainWindow)
        {
            mainWindow.SetNextEnabled(canProceed);
        }
    }

    private async void DiscoverButton_Click(object sender, RoutedEventArgs e)
    {
        await CaptureService.DiscoverCamerasAsync();
    }

    private async void ConnectCamerasButton_Click(object sender, RoutedEventArgs e)
    {
        await CaptureService.ConnectAllCamerasAsync();
    }

    private void ScanButton_Click(object sender, RoutedEventArgs e)
    {
        _devices.Clear();
        CaptureService.StartBluetoothScan();
    }

    private void OnDeviceDiscovered(string deviceId, string deviceName)
    {
        // Must run on UI thread
        DispatcherQueue.TryEnqueue(() =>
        {
            // Avoid duplicates
            foreach (var dev in _devices)
            {
                if (dev.Id == deviceId) return;
            }
            _devices.Add(new BluetoothDeviceItem { Id = deviceId, Name = deviceName });
        });
    }

    private void DeviceListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        ConnectTurntableButton.IsEnabled = DeviceListView.SelectedItem != null;
    }

    private void ConnectTurntableButton_Click(object sender, RoutedEventArgs e)
    {
        if (DeviceListView.SelectedItem is BluetoothDeviceItem device)
        {
            CaptureService.StopBluetoothScan();
            bool success = CaptureService.ConnectBluetooth(device.Id);
            if (success)
            {
                TurntableNameText.Text = device.Name;
            }
        }
    }
}
