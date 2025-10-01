# Module Documentation

Each module is designed to be relatively independent and replaceable. This documentation helps you understand what each module does and how to extend or replace it.

## Module List

- **[Hardware](hardware.md)** - Camera integration via Sapera SDK
- **[Bluetooth](bluetooth.md)** - Turntable control via BLE
- **[GUI](gui.md)** - ImGui interface and panels
- **[Capture](capture.md)** - Automated capture orchestration
- **[Utils](utils.md)** - Settings, sessions, notifications

## Module Interface Pattern

Each module follows a consistent pattern:

1. **Manager class**: Singleton or application-owned coordinator
2. **Worker classes**: Device/resource abstractions (Camera, BluetoothDevice, etc.)
3. **Data structures**: Configuration, state, parameters
4. **GUI integration**: Panel classes that render UI for the module

## Replacing a Module

Want to use different cameras or turntable protocols? Each module doc includes:

- **Core interface**: What the rest of the system expects
- **Extension points**: Where to add new functionality
- **Example adaptations**: Common modifications (e.g., swap Sapera for OpenCV)

## Inter-Module Communication

Modules communicate through:

1. **Direct calls**: Application → Manager → Worker
2. **Callbacks**: Worker → Manager → Application (for events)
3. **Shared state**: SettingsManager, SessionManager (read-only from modules)

Modules do NOT directly call each other (except via Application).
