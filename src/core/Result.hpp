/**
 * Result.hpp - Modern Result Pattern Implementation
 * Monadic operations, error chaining, and comprehensive error handling
 */

#pragma once

#include "Types.hpp"
#include <expected>
#include <functional>
#include <format>
#include <source_location>

namespace sapera::core {

// =============================================================================
// RESULT CREATION HELPERS - Clean, expressive error creation
// =============================================================================

template<typename T>
[[nodiscard]] constexpr Result<T> Ok(T&& value) {
    return Result<T>{std::forward<T>(value)};
}

[[nodiscard]] constexpr VoidResult Ok() {
    return VoidResult{};
}

template<typename T>
[[nodiscard]] constexpr Result<T> Err(Error error) {
    return Result<T>{std::unexpected{std::move(error)}};
}

[[nodiscard]] constexpr VoidResult Err(Error error) {
    return VoidResult{std::unexpected{std::move(error)}};
}

// =============================================================================
// ERROR CREATION HELPERS - Expressive error construction
// =============================================================================

[[nodiscard]] inline Error make_error(
    ErrorCode code, 
    std::string_view message, 
    std::string_view details = "",
    std::source_location location = std::source_location::current()
) {
    return Error{code, message, details}
        .with_context(std::format("{}:{}", location.file_name(), location.line()));
}

// Specific error creators for common scenarios
[[nodiscard]] inline Error camera_not_found(const CameraId& id) {
    return make_error(ErrorCode::CameraNotFound, 
        std::format("Camera '{}' not found", id.get()));
}

[[nodiscard]] inline Error camera_already_connected(const CameraId& id) {
    return make_error(ErrorCode::CameraAlreadyConnected, 
        std::format("Camera '{}' is already connected", id.get()));
}

[[nodiscard]] inline Error camera_not_connected(const CameraId& id) {
    return make_error(ErrorCode::CameraNotConnected, 
        std::format("Camera '{}' is not connected", id.get()));
}

[[nodiscard]] inline Error capture_timeout(const CameraId& id, std::chrono::milliseconds timeout) {
    return make_error(ErrorCode::CaptureTimeout, 
        std::format("Capture timeout for camera '{}' after {}ms", id.get(), timeout.count()));
}

[[nodiscard]] inline Error sapera_sdk_error(std::string_view operation, std::string_view details) {
    return make_error(ErrorCode::SaperaSDKError, 
        std::format("Sapera SDK error during '{}': {}", operation, details));
}

[[nodiscard]] inline Error file_write_error(const FilePath& path, std::string_view reason) {
    return make_error(ErrorCode::FileWriteError, 
        std::format("Failed to write file '{}': {}", path.get(), reason));
}

// =============================================================================
// MONADIC OPERATIONS - Functional error handling
// =============================================================================

// Transform success value (map)
template<typename T, typename F>
[[nodiscard]] constexpr auto operator|(const Result<T>& result, F&& func) -> Result<std::invoke_result_t<F, T>> {
    if (result.has_value()) {
        if constexpr (std::is_void_v<std::invoke_result_t<F, T>>) {
            std::invoke(func, *result);
            return Ok();
        } else {
            return Ok(std::invoke(func, *result));
        }
    } else {
        return Err<std::invoke_result_t<F, T>>(result.error());
    }
}

// Chain operations that return Result (flatMap)
template<typename T, typename F>
[[nodiscard]] constexpr auto operator>>=(const Result<T>& result, F&& func) -> std::invoke_result_t<F, T> {
    if (result.has_value()) {
        return std::invoke(func, *result);
    } else {
        return std::invoke_result_t<F, T>{std::unexpected{result.error()}};
    }
}

// Error handling/recovery
template<typename T, typename F>
[[nodiscard]] constexpr Result<T> or_else(const Result<T>& result, F&& func) {
    if (result.has_value()) {
        return result;
    } else {
        return std::invoke(func, result.error());
    }
}

// =============================================================================
// RESULT COMBINATORS - Combining multiple results
// =============================================================================

// Combine two results - both must succeed
template<typename T1, typename T2>
[[nodiscard]] constexpr Result<std::pair<T1, T2>> combine(const Result<T1>& r1, const Result<T2>& r2) {
    if (r1.has_value() && r2.has_value()) {
        return Ok(std::make_pair(*r1, *r2));
    } else if (!r1.has_value()) {
        return Err<std::pair<T1, T2>>(r1.error());
    } else {
        return Err<std::pair<T1, T2>>(r2.error());
    }
}

// Combine multiple results - all must succeed
template<typename... Results>
[[nodiscard]] constexpr auto combine_all(const Results&... results) {
    if ((results.has_value() && ...)) {
        return Ok(std::make_tuple(*results...));
    } else {
        // Return first error encountered
        Error first_error = make_error(ErrorCode::UnknownError, "Multiple errors occurred");
        ((results.has_value() ? void() : (first_error = results.error(), void())), ...);
        return Err<std::tuple<typename Results::value_type...>>(first_error);
    }
}

// First successful result wins
template<typename T>
[[nodiscard]] constexpr Result<T> first_success(const std::vector<Result<T>>& results) {
    for (const auto& result : results) {
        if (result.has_value()) {
            return result;
        }
    }
    
    if (!results.empty()) {
        return results.back(); // Return last error
    }
    
    return Err<T>(make_error(ErrorCode::UnknownError, "No results provided"));
}

// =============================================================================
// RESULT UTILITIES - Convenience functions
// =============================================================================

// Convert boolean to Result
template<typename T>
[[nodiscard]] constexpr Result<T> from_bool(bool success, T value, Error error) {
    return success ? Ok(std::move(value)) : Err<T>(std::move(error));
}

[[nodiscard]] constexpr VoidResult from_bool(bool success, Error error) {
    return success ? Ok() : Err(std::move(error));
}

// Convert optional to Result
template<typename T>
[[nodiscard]] constexpr Result<T> from_optional(const std::optional<T>& opt, Error error) {
    return opt.has_value() ? Ok(*opt) : Err<T>(std::move(error));
}

// Convert exception to Result
template<typename T, typename F>
[[nodiscard]] Result<T> try_catch(F&& func) {
    try {
        if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
            std::invoke(func);
            return Ok();
        } else {
            return Ok(std::invoke(func));
        }
    } catch (const std::exception& e) {
        return Err<T>(make_error(ErrorCode::UnknownError, 
            std::format("Exception caught: {}", e.what())));
    } catch (...) {
        return Err<T>(make_error(ErrorCode::UnknownError, 
            "Unknown exception caught"));
    }
}

// =============================================================================
// RESULT INSPECTION - Debugging and logging helpers
// =============================================================================

template<typename T>
[[nodiscard]] std::string to_string(const Result<T>& result) {
    if (result.has_value()) {
        if constexpr (std::is_void_v<T>) {
            return "Ok()";
        } else {
            return std::format("Ok({})", *result);
        }
    } else {
        const auto& error = result.error();
        return std::format("Err(code={}, message='{}')", 
            static_cast<int>(error.code), error.message);
    }
}

// Log result for debugging
template<typename T>
void log_result(const Result<T>& result, std::string_view context = "") {
    if (!context.empty()) {
        std::cout << "[" << context << "] ";
    }
    std::cout << to_string(result) << std::endl;
}

// =============================================================================
// ASYNC RESULT PATTERNS - Modern async support
// =============================================================================

#include <future>
#include <chrono>

template<typename T>
using AsyncResult = std::future<Result<T>>;

// Convert std::future to AsyncResult
template<typename T>
AsyncResult<T> make_async_result(std::future<T> future) {
    return std::async(std::launch::async, [future = std::move(future)]() mutable -> Result<T> {
        try {
            if constexpr (std::is_void_v<T>) {
                future.get();
                return Ok();
            } else {
                return Ok(future.get());
            }
        } catch (const std::exception& e) {
            return Err<T>(make_error(ErrorCode::UnknownError, 
                std::format("Async operation failed: {}", e.what())));
        }
    });
}

// Timeout for async operations
template<typename T>
Result<T> get_with_timeout(AsyncResult<T>& async_result, std::chrono::milliseconds timeout) {
    if (async_result.wait_for(timeout) == std::future_status::timeout) {
        return Err<T>(make_error(ErrorCode::OperationCancelled, 
            std::format("Operation timed out after {}ms", timeout.count())));
    }
    return async_result.get();
}

} // namespace sapera::core 