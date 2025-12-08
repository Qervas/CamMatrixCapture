# GUI Module

WinUI 3-based user interface with wizard-style navigation.

## Architecture

The GUI is built with WinUI 3 (Windows App SDK) in C#, communicating with the C++ backend via P/Invoke.

```
┌─────────────────────────────────────────────────────────────┐
│              WinUI 3 Frontend (C#)                          │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │
│  │ Connect │ │  Setup  │ │ Capture │ │  Done   │          │
│  │  Page   │ │  Page   │ │  Page   │ │  Page   │          │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘          │
│                      │                                      │
│              ┌───────┴───────┐                             │
│              │CaptureService │  (P/Invoke)                 │
│              └───────┬───────┘                             │
└──────────────────────┼──────────────────────────────────────┘
                       │
┌──────────────────────┼──────────────────────────────────────┐
│              CaptureCore.dll (C++)                          │
│              ┌───────┴───────┐                             │
│              │  C API Layer  │                             │
│              └───────────────┘                             │
└─────────────────────────────────────────────────────────────┘
```

## Wizard Flow

The application uses a linear wizard flow:

```
Connect → Setup → Capture → Done
   1        2        3        4
```

Users must complete each step before proceeding to the next.

## Key Components

### App.xaml.cs

Application entry point and lifecycle management.

```csharp
public partial class App : Application
{
    public static Window? MainWindow { get; private set; }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        CaptureService.Initialize();  // Initialize C++ backend
        m_window = new MainWindow();
        MainWindow = m_window;
        m_window.Activate();
    }
}
```

### MainWindow

Main window containing the wizard framework.

**Layout:**
```
┌─────────────────────────────────────────────────────────────┐
│  Camera Matrix Capture                              ⚙️      │
├─────────────────────────────────────────────────────────────┤
│     ●━━━━━━━━━━○━━━━━━━━━━○━━━━━━━━━━○                      │
│   Connect    Setup     Capture     Done                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│              Page Content (Frame)                           │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│     [ Back ]                              [ Next → ]        │
└─────────────────────────────────────────────────────────────┘
```

**Key methods:**
```csharp
private void NavigateToStep(int step);      // Navigate to wizard step
private void UpdateStepIndicators();        // Update circle colors
public void SetNextEnabled(bool enabled);   // Control Next button state
```

### Pages

#### ConnectPage (Step 1)

Device connection interface for cameras and turntable.

**UI sections:**
- **Cameras card**: Discover and connect cameras
- **Turntable card**: Scan and connect Bluetooth turntable

**Key functionality:**
```csharp
private async void DiscoverButton_Click(...)
{
    await CaptureService.DiscoverCamerasAsync();
}

private void ConnectTurntableButton_Click(...)
{
    CaptureService.ConnectBluetooth(device.Id);
}
```

**State updates:**
- Polls `CaptureService` every 500ms for status updates
- Enables Next button when both cameras and turntable are connected

#### SetupPage (Step 2)

Capture configuration with presets.

**Presets:**
- **Quick (36 positions)**: 10° rotation steps
- **Detailed (72 positions)**: 5° rotation steps
- **Custom**: User-defined positions and angle steps

**Summary display:**
```csharp
SummaryText.Text = $"{TotalPositions} positions at {AngleStep}° intervals\n" +
                   $"{cameraCount} cameras × {TotalPositions} = {totalImages} images\n" +
                   $"Estimated time: {timeEstimate}";
```

#### CapturePage (Step 3)

Capture execution with progress tracking.

**UI elements:**
- Large Start/Stop button
- Progress bar with position count
- Current angle display
- Total images captured

**Capture flow:**
```csharp
private void CaptureButton_Click(...)
{
    if (_isCapturing)
    {
        CaptureService.StopCapture();
    }
    else
    {
        CaptureService.StartCapture(sessionName, totalPositions, angleStep);
    }
}
```

**Events:**
- `OnProgress`: Updates progress bar
- `OnCaptureComplete`: Navigates to Done page

#### DonePage (Step 4)

Capture completion summary.

**Features:**
- Success icon and message
- Image count display
- Session path (selectable text)
- "Open Folder" button
- "New Capture" button (returns to Step 1)

### Dialogs

#### SettingsDialog

Application settings flyout accessible from any page.

**Settings:**
- **Camera**: Exposure time (μs), Gain (dB)
- **Capture**: Parallel groups, Stagger delay (ms)
- **Output**: Directory path with browse button

**Save flow:**
```csharp
private void SaveButton_Click(...)
{
    CaptureService.ExposureTime = (int)ExposureSlider.Value;
    CaptureService.Gain = (float)GainSlider.Value;
    CaptureService.SaveSettings();
}
```

### Services

#### CaptureService

P/Invoke wrapper for the C++ backend DLL.

**Categories:**

1. **Lifecycle**
```csharp
public static void Initialize();
public static void Shutdown();
```

2. **Camera operations**
```csharp
public static Task DiscoverCamerasAsync();
public static Task ConnectAllCamerasAsync();
public static int DiscoveredCameraCount { get; }
public static int ConnectedCameraCount { get; }
public static bool IsDiscovering { get; }
```

3. **Bluetooth operations**
```csharp
public static void StartBluetoothScan();
public static bool ConnectBluetooth(string deviceId);
public static bool IsBluetoothConnected { get; }
public static bool RotateTurntable(float angle);
```

4. **Capture operations**
```csharp
public static void StartCapture(string sessionName, int positions, float angleStep);
public static void StopCapture();
public static int CaptureProgress { get; }
```

5. **Settings**
```csharp
public static int ExposureTime { get; set; }
public static float Gain { get; set; }
public static int ParallelGroups { get; set; }
public static string OutputPath { get; set; }
```

6. **Events**
```csharp
public static event Action<string>? OnLog;
public static event Action<int, int>? OnProgress;
public static event Action<string, string>? OnDeviceDiscovered;
public static event Action<bool, string>? OnCaptureComplete;
```

## XAML Patterns

### Card layout (ConnectPage)
```xml
<Border CornerRadius="8" Padding="20"
        Background="{ThemeResource CardBackgroundFillColorDefaultBrush}">
    <StackPanel>
        <StackPanel Orientation="Horizontal">
            <FontIcon Glyph="&#xE722;" FontSize="24"/>
            <TextBlock Text="Cameras" FontSize="20" FontWeight="SemiBold"/>
        </StackPanel>
        <!-- Content -->
    </StackPanel>
</Border>
```

### Status indicator
```xml
<StackPanel Orientation="Horizontal">
    <Ellipse x:Name="StatusDot" Width="8" Height="8" Fill="Gray"/>
    <TextBlock x:Name="StatusText" Text="Not connected"/>
</StackPanel>
```

### Progress bar
```xml
<ProgressBar x:Name="CaptureProgressBar" Maximum="36" Value="0"/>
<TextBlock x:Name="ProgressDetailText" Text="0 / 36 positions"/>
```

### Preset radio buttons (SetupPage)
```xml
<RadioButton x:Name="QuickPreset" GroupName="Preset" IsChecked="True"
             Checked="Preset_Checked">
    <StackPanel>
        <TextBlock Text="Quick (36 positions)" FontWeight="SemiBold"/>
        <TextBlock Text="10° steps, ~3 minutes" Opacity="0.7"/>
    </StackPanel>
</RadioButton>
```

## Theme and Styling

Uses WinUI 3 system themes with custom brushes defined in `App.xaml`:

```xml
<Application.Resources>
    <ResourceDictionary>
        <ResourceDictionary.MergedDictionaries>
            <XamlControlsResources xmlns="using:Microsoft.UI.Xaml.Controls"/>
        </ResourceDictionary.MergedDictionaries>

        <SolidColorBrush x:Key="AccentBrush" Color="#0078D4"/>
        <SolidColorBrush x:Key="SuccessBrush" Color="#107C10"/>
    </ResourceDictionary>
</Application.Resources>
```

## File Structure

```
App/
├── App.xaml                    # Application resources
├── App.xaml.cs                 # Application entry point
├── MainWindow.xaml             # Wizard container
├── MainWindow.xaml.cs          # Navigation logic
├── Pages/
│   ├── ConnectPage.xaml        # Step 1: Device connection
│   ├── ConnectPage.xaml.cs
│   ├── SetupPage.xaml          # Step 2: Capture settings
│   ├── SetupPage.xaml.cs
│   ├── CapturePage.xaml        # Step 3: Capture execution
│   ├── CapturePage.xaml.cs
│   ├── DonePage.xaml           # Step 4: Completion
│   └── DonePage.xaml.cs
├── Dialogs/
│   ├── SettingsDialog.xaml     # Settings flyout
│   └── SettingsDialog.xaml.cs
├── Services/
│   └── CaptureService.cs       # P/Invoke wrapper
├── app.manifest                # Windows manifest
└── CameraMatrixCapture.csproj  # Project file
```

## Build Configuration

**Project settings (CameraMatrixCapture.csproj):**
```xml
<PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows10.0.19041.0</TargetFramework>
    <UseWinUI>true</UseWinUI>
    <WindowsPackageType>None</WindowsPackageType>
    <WindowsAppSDKSelfContained>true</WindowsAppSDKSelfContained>
</PropertyGroup>
```

**Build command:**
```bash
dotnet build -c Release -p:Platform=x64
```

## Extension Points

### Adding a new page

1. Create XAML page:
```xml
<Page x:Class="CameraMatrixCapture.Pages.NewPage">
    <!-- Content -->
</Page>
```

2. Add to MainWindow navigation:
```csharp
case 5:
    ContentFrame.Navigate(typeof(NewPage));
    break;
```

3. Update step indicator grid.

### Adding settings

1. Add UI control to `SettingsDialog.xaml`
2. Add property to `CaptureService.cs`
3. Add P/Invoke import for C++ getter/setter
4. Implement in `CaptureAPI.cpp`

### Custom dialogs

```csharp
var dialog = new ContentDialog
{
    Title = "Confirm",
    Content = "Are you sure?",
    PrimaryButtonText = "Yes",
    CloseButtonText = "No",
    XamlRoot = this.XamlRoot
};

var result = await dialog.ShowAsync();
if (result == ContentDialogResult.Primary)
{
    // Action
}
```

## Performance Considerations

- **UI thread**: All UI updates must run on the dispatcher thread
- **Async operations**: Camera discovery and capture run on background threads
- **Polling**: Status updates poll every 200-500ms instead of continuous callbacks
- **Memory**: Dispose of event handlers when pages are unloaded
