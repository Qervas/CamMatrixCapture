# Sapera Camera Module

This submodule contains the thread-safe implementation of the Sapera camera interface. It is designed to prevent deadlocks and ensure smooth operation with hardware cameras in a multi-threaded environment.

## Key Components

### CameraThread

A dedicated worker thread for camera operations that ensures that hardware interactions don't block the UI thread. This thread:

- Processes operations in a queue to avoid concurrency issues
- Handles exceptions safely to prevent application crashes
- Provides both synchronous and asynchronous operation interfaces

### SaperaCamera

A thread-safe implementation of the camera interface that:

- Supports real Sapera SDK when available, with fallback to simulation
- Handles frame acquisition in a dedicated thread
- Provides non-blocking frame access methods to prevent UI freezes
- Uses atomic variables for thread-safe state management

## Thread Safety Design

The module uses multiple techniques to ensure thread safety:

1. **Dedicated Operation Thread**: All camera operations run in a separate thread
2. **Message Queue Pattern**: Operations are queued and processed sequentially
3. **Non-blocking Mutex Locks**: Using try_lock to prevent deadlocks
4. **Signal/Slot Connections**: Using Qt::QueuedConnection for cross-thread communication
5. **Atomic State Variables**: Thread-safe status flags without locking
6. **Promise/Future Pattern**: For synchronous operation completion notification

## Usage Example

```cpp
// Create a camera instance
auto camera = std::make_unique<SaperaCamera>("CameraName");

// Connect signals for frame updates
connect(camera.get(), &SaperaCamera::newFrameAvailable, 
        this, &MyClass::handleNewFrame, Qt::QueuedConnection);

// Synchronous connection (blocks until complete)
if (camera->connectCamera()) {
    // Camera is connected
}

// Asynchronous connection (returns immediately)
camera->connectCameraAsync([](bool success) {
    // Called when connection is complete
    if (success) {
        // Camera is connected
    }
});
```

## Conditional Compilation

The module supports three different implementation modes:

1. **HAS_SAPERA**: Full Sapera SDK implementation
2. **HAS_GIGE_VISION**: GigE Vision camera implementation
3. **Fallback Mode**: Simulation mode when no camera SDK is available 