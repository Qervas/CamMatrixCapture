/**
 * ModernDemo.cpp - Showcase of Modern SaperaCapture Pro Architecture
 * Demonstrates type-safe operations, reactive patterns, and clean error handling
 */

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include "../interfaces/CameraInterface.hpp"
#include "../reactive/EventSystem.hpp"
#include <iostream>
#include <format>
#include <ranges>
#include <chrono>
#include <thread>

using namespace sapera;
using namespace std::chrono_literals;

// =============================================================================
// MODERN DEMO APPLICATION - Showcasing 2025 patterns
// =============================================================================

class ModernCameraDemo {
private:
    std::unique_ptr<reactive::ReactiveSystem> reactive_system_;
    std::unique_ptr<interfaces::ICamera> camera_;
    std::vector<core::CameraId> connected_cameras_;
    
public:
    ModernCameraDemo() : reactive_system_(std::make_unique<reactive::ReactiveSystem>()) {}
    
    /// Initialize the demo system
    [[nodiscard]] core::VoidResult initialize() {
        std::cout << "🚀 Initializing Modern SaperaCapture Pro Demo\n" << std::endl;
        
        // Setup reactive event handling
        setup_reactive_events();
        
        // TODO: Initialize camera system (would be injected in real app)
        // camera_ = camera_factory->create_camera();
        
        std::cout << "✅ Demo system initialized successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Run the complete demo
    [[nodiscard]] core::VoidResult run_demo() {
        std::cout << "\n🎯 Running Modern Architecture Demo\n" << std::endl;
        
        // Demonstrate type-safe operations
        if (auto result = demonstrate_type_safety(); !result.has_value()) {
            std::cout << "❌ Type safety demo failed: " << result.error().message << std::endl;
            return result;
        }
        
        // Demonstrate modern error handling
        if (auto result = demonstrate_error_handling(); !result.has_value()) {
            std::cout << "❌ Error handling demo failed: " << result.error().message << std::endl;
            return result;
        }
        
        // Demonstrate reactive patterns
        if (auto result = demonstrate_reactive_patterns(); !result.has_value()) {
            std::cout << "❌ Reactive patterns demo failed: " << result.error().message << std::endl;
            return result;
        }
        
        // Demonstrate monadic operations
        if (auto result = demonstrate_monadic_operations(); !result.has_value()) {
            std::cout << "❌ Monadic operations demo failed: " << result.error().message << std::endl;
            return result;
        }
        
        // Demonstrate async patterns
        if (auto result = demonstrate_async_patterns(); !result.has_value()) {
            std::cout << "❌ Async patterns demo failed: " << result.error().message << std::endl;
            return result;
        }
        
        std::cout << "\n🎉 All demos completed successfully!" << std::endl;
        return core::Ok();
    }
    
private:
    /// Setup reactive event handling
    void setup_reactive_events() {
        std::cout << "🔄 Setting up reactive event system..." << std::endl;
        
        // Subscribe to camera connection events
        reactive_system_->camera_connected_events()
            .filter([](const auto& event) {
                return event.data.cameraInfo.type == core::CameraType::Industrial;
            })
            .take(5)
            .subscribe([](const auto& event) {
                std::cout << std::format("📸 Camera connected: {} ({})", 
                    event.data.cameraId.get(),
                    event.data.cameraInfo.modelName) << std::endl;
            });
        
        // Subscribe to image capture events with debouncing
        reactive_system_->image_captured_events()
            .debounce(100ms)
            .subscribe([](const auto& event) {
                const auto& metadata = event.data.imageBuffer->metadata();
                std::cout << std::format("📷 Image captured: {}x{} from camera {}", 
                    metadata.width, metadata.height, 
                    event.data.cameraId.get()) << std::endl;
            });
        
        std::cout << "✅ Reactive event system configured" << std::endl;
    }
    
    /// Demonstrate type-safe operations
    [[nodiscard]] core::VoidResult demonstrate_type_safety() {
        std::cout << "\n🔒 Demonstrating Type Safety\n" << std::endl;
        
        // Strong typing prevents mixing up IDs
        auto camera_id = core::CameraId{"camera_001"};
        auto serial_number = core::SerialNumber{"S1138848"};
        auto server_name = core::ServerName{"CameraLink_0"};
        
        // This would be a compile error - type safety at work!
        // auto wrong_assignment = camera_id == serial_number; // ❌ Won't compile
        
        std::cout << std::format("📋 Camera ID: {}", camera_id.get()) << std::endl;
        std::cout << std::format("📋 Serial Number: {}", serial_number.get()) << std::endl;
        std::cout << std::format("📋 Server Name: {}", server_name.get()) << std::endl;
        
        // Demonstrate enum class safety
        auto camera_state = core::CameraState::Ready;
        auto camera_type = core::CameraType::Industrial;
        
        std::cout << std::format("📋 Camera State: {}", static_cast<int>(camera_state)) << std::endl;
        std::cout << std::format("📋 Camera Type: {}", static_cast<int>(camera_type)) << std::endl;
        
        // Demonstrate safe image format handling
        auto supported_formats = std::vector<core::ImageFormat>{
            core::ImageFormat::Mono8,
            core::ImageFormat::Mono16,
            core::ImageFormat::RGB8
        };
        
        std::cout << "📋 Supported formats: ";
        for (const auto& format : supported_formats) {
            std::cout << static_cast<int>(format) << " ";
        }
        std::cout << std::endl;
        
        std::cout << "✅ Type safety demonstrated successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Demonstrate modern error handling
    [[nodiscard]] core::VoidResult demonstrate_error_handling() {
        std::cout << "\n🛡️ Demonstrating Modern Error Handling\n" << std::endl;
        
        // Demonstrate Result pattern with successful operation
        auto success_result = simulate_camera_operation(true);
        if (success_result.has_value()) {
            std::cout << std::format("✅ Success: {}", success_result.value()) << std::endl;
        }
        
        // Demonstrate Result pattern with error
        auto error_result = simulate_camera_operation(false);
        if (!error_result.has_value()) {
            const auto& error = error_result.error();
            std::cout << std::format("❌ Error: {} (Code: {})", 
                error.message, static_cast<int>(error.code)) << std::endl;
            std::cout << std::format("📋 Details: {}", error.details) << std::endl;
            if (error.context) {
                std::cout << std::format("📋 Context: {}", *error.context) << std::endl;
            }
        }
        
        // Demonstrate error chaining
        auto chained_result = simulate_camera_operation(true)
            .and_then([](const std::string& result) {
                return simulate_another_operation(false);
            });
        
        if (!chained_result.has_value()) {
            std::cout << std::format("🔗 Chained operation failed: {}", 
                chained_result.error().message) << std::endl;
        }
        
        // Demonstrate error recovery
        auto recovered_result = core::or_else(error_result, [](const auto& error) {
            std::cout << std::format("🔄 Recovering from error: {}", error.message) << std::endl;
            return core::Ok<std::string>("Recovered successfully");
        });
        
        if (recovered_result.has_value()) {
            std::cout << std::format("✅ Recovery: {}", recovered_result.value()) << std::endl;
        }
        
        std::cout << "✅ Error handling demonstrated successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Demonstrate reactive patterns
    [[nodiscard]] core::VoidResult demonstrate_reactive_patterns() {
        std::cout << "\n🌊 Demonstrating Reactive Patterns\n" << std::endl;
        
        // Simulate camera connection events
        auto camera_id = core::CameraId{"demo_camera_001"};
        auto camera_info = core::CameraInfo{
            .id = camera_id,
            .serverName = core::ServerName{"DemoServer"},
            .serialNumber = core::SerialNumber{"DEMO123456"},
            .modelName = "Nano-C4020-Demo",
            .displayName = "Demo Camera 001",
            .type = core::CameraType::Industrial,
            .state = core::CameraState::Ready,
            .resourceIndex = 0
        };
        
        std::cout << "📡 Publishing camera connection event..." << std::endl;
        reactive_system_->publish_camera_connected(camera_id, camera_info);
        
        // Give time for async event processing
        std::this_thread::sleep_for(100ms);
        
        // Simulate image capture
        std::cout << "📡 Simulating image capture events..." << std::endl;
        for (int i = 0; i < 3; ++i) {
            auto metadata = core::ImageMetadata{
                .timestamp = std::chrono::system_clock::now(),
                .cameraSerial = camera_info.serialNumber,
                .frameNumber = static_cast<uint64_t>(i),
                .format = core::ImageFormat::Mono8,
                .width = 4112,
                .height = 3008,
                .bitsPerPixel = 8,
                .bytesPerPixel = 1,
                .dataSize = 4112 * 3008
            };
            
            auto image_buffer = std::make_unique<core::ImageBuffer>(metadata.dataSize, std::move(metadata));
            reactive_system_->publish_image_captured(camera_id, std::move(image_buffer));
            
            std::this_thread::sleep_for(50ms);
        }
        
        // Give time for debounced events
        std::this_thread::sleep_for(200ms);
        
        std::cout << "✅ Reactive patterns demonstrated successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Demonstrate monadic operations
    [[nodiscard]] core::VoidResult demonstrate_monadic_operations() {
        std::cout << "\n🔄 Demonstrating Monadic Operations\n" << std::endl;
        
        // Demonstrate map operation
        auto result = core::Ok<int>(42);
        auto mapped = result | [](int x) { return x * 2; };
        
        if (mapped.has_value()) {
            std::cout << std::format("🔄 Map: 42 -> {}", mapped.value()) << std::endl;
        }
        
        // Demonstrate flatMap operation
        auto flat_mapped = result >>= [](int x) -> core::Result<std::string> {
            if (x > 0) {
                return core::Ok(std::format("Positive: {}", x));
            } else {
                return core::Err<std::string>(core::make_error(
                    core::ErrorCode::InvalidArgument, "Value must be positive"));
            }
        };
        
        if (flat_mapped.has_value()) {
            std::cout << std::format("🔄 FlatMap: {}", flat_mapped.value()) << std::endl;
        }
        
        // Demonstrate result combination
        auto result1 = core::Ok<int>(10);
        auto result2 = core::Ok<std::string>("cameras");
        auto combined = core::combine(result1, result2);
        
        if (combined.has_value()) {
            auto [count, description] = combined.value();
            std::cout << std::format("🔄 Combined: {} {}", count, description) << std::endl;
        }
        
        // Demonstrate error propagation
        auto error_result = core::Err<int>(core::make_error(
            core::ErrorCode::CameraNotFound, "Demo error"));
        
        auto propagated = error_result | [](int x) { return x * 2; };
        
        if (!propagated.has_value()) {
            std::cout << std::format("🔄 Error propagated: {}", propagated.error().message) << std::endl;
        }
        
        std::cout << "✅ Monadic operations demonstrated successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Demonstrate async patterns
    [[nodiscard]] core::VoidResult demonstrate_async_patterns() {
        std::cout << "\n⚡ Demonstrating Async Patterns\n" << std::endl;
        
        // Demonstrate async operation
        auto future_result = std::async(std::launch::async, []() -> core::Result<std::string> {
            std::this_thread::sleep_for(100ms);
            return core::Ok<std::string>("Async operation completed");
        });
        
        std::cout << "⏳ Async operation started..." << std::endl;
        
        // Convert to AsyncResult
        auto async_result = core::make_async_result(std::move(future_result));
        
        // Wait with timeout
        auto result = core::get_with_timeout(async_result, 500ms);
        
        if (result.has_value()) {
            std::cout << std::format("✅ Async result: {}", result.value()) << std::endl;
        } else {
            std::cout << std::format("❌ Async error: {}", result.error().message) << std::endl;
        }
        
        // Demonstrate timeout
        auto slow_future = std::async(std::launch::async, []() -> core::Result<std::string> {
            std::this_thread::sleep_for(1000ms);
            return core::Ok<std::string>("Slow operation completed");
        });
        
        auto slow_async_result = core::make_async_result(std::move(slow_future));
        auto timeout_result = core::get_with_timeout(slow_async_result, 100ms);
        
        if (!timeout_result.has_value()) {
            std::cout << std::format("⏰ Timeout handled: {}", timeout_result.error().message) << std::endl;
        }
        
        std::cout << "✅ Async patterns demonstrated successfully!" << std::endl;
        return core::Ok();
    }
    
    /// Simulate camera operation for demo
    [[nodiscard]] core::Result<std::string> simulate_camera_operation(bool should_succeed) {
        if (should_succeed) {
            return core::Ok<std::string>("Camera operation succeeded");
        } else {
            return core::Err<std::string>(core::make_error(
                core::ErrorCode::CameraNotFound, 
                "Demo camera not found",
                "This is a simulated error for demonstration purposes"));
        }
    }
    
    /// Simulate another operation for chaining demo
    [[nodiscard]] core::Result<std::string> simulate_another_operation(bool should_succeed) {
        if (should_succeed) {
            return core::Ok<std::string>("Second operation succeeded");
        } else {
            return core::Err<std::string>(core::make_error(
                core::ErrorCode::CaptureTimeout, 
                "Second operation timed out"));
        }
    }
};

// =============================================================================
// MAIN DEMO RUNNER
// =============================================================================

int main() {
    std::cout << "🎨 Modern SaperaCapture Pro Architecture Demo\n" << std::endl;
    std::cout << "This demo showcases modern C++23 patterns and clean architecture.\n" << std::endl;
    
    try {
        ModernCameraDemo demo;
        
        // Initialize the demo
        if (auto result = demo.initialize(); !result.has_value()) {
            std::cout << std::format("❌ Demo initialization failed: {}", 
                result.error().message) << std::endl;
            return 1;
        }
        
        // Run the demo
        if (auto result = demo.run_demo(); !result.has_value()) {
            std::cout << std::format("❌ Demo execution failed: {}", 
                result.error().message) << std::endl;
            return 1;
        }
        
        std::cout << "\n🎊 Demo completed successfully!" << std::endl;
        std::cout << "\nKey features demonstrated:" << std::endl;
        std::cout << "✅ Type-safe strong typing (no more string confusion!)" << std::endl;
        std::cout << "✅ Modern error handling with std::expected" << std::endl;
        std::cout << "✅ Reactive event system with observables" << std::endl;
        std::cout << "✅ Monadic operations for clean composition" << std::endl;
        std::cout << "✅ Modern async patterns with timeouts" << std::endl;
        std::cout << "✅ Clean architecture with dependency injection" << std::endl;
        std::cout << "✅ Comprehensive error contexts and recovery" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << std::format("💥 Unexpected exception: {}", e.what()) << std::endl;
        return 1;
    }
} 