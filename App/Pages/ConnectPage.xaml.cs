using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI;
using CameraMatrixCapture.Services;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;

namespace CameraMatrixCapture.Pages;

public class BluetoothDeviceItem
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
}

public class CameraItem
{
    public int Index { get; set; }
    public string DisplayName { get; set; } = "";
    public string SerialNumber { get; set; } = "";
    public bool IsConnected { get; set; }
    public SolidColorBrush StatusColor => IsConnected
        ? new SolidColorBrush(Colors.Green)
        : new SolidColorBrush(Colors.Gray);
}

public sealed partial class ConnectPage : Page
{
    private readonly ObservableCollection<BluetoothDeviceItem> _devices = new();
    private readonly ObservableCollection<CameraItem> _cameras = new();
    private readonly DispatcherTimer _refreshTimer;

    public ConnectPage()
    {
        this.InitializeComponent();
        DeviceListView.ItemsSource = _devices;
        CameraListView.ItemsSource = _cameras;

        // Set up callbacks
        CaptureService.OnDeviceDiscovered += OnDeviceDiscovered;
        CaptureService.OnLog += OnLog;

        // Timer to refresh status
        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
        _refreshTimer.Tick += RefreshStatus;
        _refreshTimer.Start();

        // Initial status update
        UpdateCameraStatus();
        UpdateTurntableStatus();
    }

    private void OnLog(string message)
    {
        Debug.WriteLine($"[CaptureCore] {message}");
    }

    private void RefreshStatus(object? sender, object e)
    {
        UpdateCameraStatus();
        UpdateCameraList();
        UpdateTurntableStatus();
        UpdateNextButtonState();
    }

    private void UpdateCameraList()
    {
        int discovered = CaptureService.DiscoveredCameraCount;

        // Only update if count changed (don't refresh during drag)
        if (_cameras.Count != discovered)
        {
            _cameras.Clear();
            for (int i = 0; i < discovered; i++)
            {
                string name = CaptureService.GetCameraName(i);
                string serial = CaptureService.GetCameraSerial(i);
                bool connected = CaptureService.IsCameraConnected(i);

                _cameras.Add(new CameraItem
                {
                    Index = i,
                    DisplayName = string.IsNullOrEmpty(name) ? $"Camera {i + 1}" : name,
                    SerialNumber = serial,
                    IsConnected = connected
                });
            }

            // Re-attach collection changed handler for reorder detection
            _cameras.CollectionChanged -= OnCamerasCollectionChanged;
            _cameras.CollectionChanged += OnCamerasCollectionChanged;
        }
        else
        {
            // Just update connection status and names (in case of reorder)
            for (int i = 0; i < _cameras.Count; i++)
            {
                string name = CaptureService.GetCameraName(i);
                bool connected = CaptureService.IsCameraConnected(i);
                if (_cameras[i].IsConnected != connected || _cameras[i].DisplayName != name)
                {
                    _cameras[i] = new CameraItem
                    {
                        Index = i,
                        DisplayName = string.IsNullOrEmpty(name) ? $"Camera {i + 1}" : name,
                        SerialNumber = _cameras[i].SerialNumber,
                        IsConnected = connected
                    };
                }
            }
        }
    }

    private void OnCamerasCollectionChanged(object? sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e)
    {
        // Handle reorder - when an item is moved, update the backend
        if (e.Action == System.Collections.Specialized.NotifyCollectionChangedAction.Move &&
            e.OldStartingIndex >= 0 && e.NewStartingIndex >= 0)
        {
            Debug.WriteLine($"[ConnectPage] Camera reordered: {e.OldStartingIndex} -> {e.NewStartingIndex}");
            CaptureService.SetCameraOrder(e.OldStartingIndex, e.NewStartingIndex);
        }
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
            TurntableControlsPanel.Visibility = Visibility.Visible;
            TurntableProgressBar.Visibility = Visibility.Collapsed;
            UpdateTurntablePosition();
        }
        else if (CaptureService.IsBluetoothScanning)
        {
            TurntableStatusDot.Fill = new SolidColorBrush(Colors.Orange);
            TurntableStatusText.Text = "Scanning...";
            TurntableProgressBar.Visibility = Visibility.Visible;
            TurntableProgressBar.IsIndeterminate = true;
            TurntableControlsPanel.Visibility = Visibility.Collapsed;
        }
        else
        {
            TurntableStatusDot.Fill = new SolidColorBrush(Colors.Gray);
            TurntableStatusText.Text = "Not connected";
            TurntableProgressBar.Visibility = Visibility.Collapsed;
            TurntableControlsPanel.Visibility = Visibility.Collapsed;
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
        try
        {
            Debug.WriteLine("[ConnectPage] Starting camera discovery...");
            _cameras.Clear();
            await CaptureService.DiscoverCamerasAsync();
            Debug.WriteLine($"[ConnectPage] Discovery complete. Found {CaptureService.DiscoveredCameraCount} cameras.");

            // Apply saved camera order after discovery
            if (CaptureService.DiscoveredCameraCount > 0)
            {
                Debug.WriteLine("[ConnectPage] Applying saved camera order...");
                CaptureService.ApplySavedCameraOrder();
                // Force refresh the camera list to show new order
                _cameras.Clear();
                UpdateCameraList();
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[ConnectPage] Discovery error: {ex.Message}\n{ex.StackTrace}");
        }
    }

    private async void ConnectCamerasButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            Debug.WriteLine("[ConnectPage] Connecting to cameras...");
            await CaptureService.ConnectAllCamerasAsync();
            Debug.WriteLine($"[ConnectPage] Connection complete. Connected: {CaptureService.ConnectedCameraCount}");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[ConnectPage] Connection error: {ex.Message}\n{ex.StackTrace}");
        }
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

    private async void ConnectTurntableButton_Click(object sender, RoutedEventArgs e)
    {
        if (DeviceListView.SelectedItem is BluetoothDeviceItem device)
        {
            CaptureService.StopBluetoothScan();

            // Update UI to show connecting
            ConnectTurntableButton.IsEnabled = false;
            ConnectTurntableButton.Content = "Connecting...";
            TurntableStatusText.Text = "Connecting...";

            // Run connection on background thread to not block UI
            bool success = await System.Threading.Tasks.Task.Run(() =>
                CaptureService.ConnectBluetooth(device.Id));

            // Update UI based on result
            ConnectTurntableButton.IsEnabled = true;
            ConnectTurntableButton.Content = "Connect";

            if (success)
            {
                TurntableNameText.Text = device.Name;
                TurntableStatusText.Text = "Connected";
                TurntableControlsPanel.Visibility = Visibility.Visible;
                UpdateTurntablePosition();
            }
            else
            {
                TurntableStatusText.Text = "Connection failed";
                TurntableControlsPanel.Visibility = Visibility.Collapsed;
            }
        }
    }

    #region Turntable Controls

    private void UpdateTurntablePosition()
    {
        RotationAngleText.Text = $"{CaptureService.CurrentAngle:F1}°";
        TiltAngleText.Text = $"{CaptureService.CurrentTilt:F1}°";
    }

    private async void RotateLeft_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.RotateTurntable(-10));
        UpdateTurntablePosition();
    }

    private async void RotateRight_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.RotateTurntable(10));
        UpdateTurntablePosition();
    }

    private async void RotateHome_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.ReturnToZero());
        UpdateTurntablePosition();
    }

    private async void TiltDown_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.TiltTurntable(-5));
        UpdateTurntablePosition();
    }

    private async void TiltUp_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.TiltTurntable(5));
        UpdateTurntablePosition();
    }

    private async void TiltHome_Click(object sender, RoutedEventArgs e)
    {
        await System.Threading.Tasks.Task.Run(() => CaptureService.TiltTurntable(-CaptureService.CurrentTilt));
        UpdateTurntablePosition();
    }

    #endregion
}
