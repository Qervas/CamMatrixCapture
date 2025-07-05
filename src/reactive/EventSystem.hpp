/**
 * EventSystem.hpp - Modern Reactive Event System
 * Type-safe events, async processing, and modern observer patterns
 */

#pragma once

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <ranges>
#include <concepts>
#include <chrono>
#include <future>

namespace sapera::reactive {

// =============================================================================
// MODERN EVENT TYPES - Type-safe event system
// =============================================================================

template<typename T>
concept EventData = requires {
    typename T::EventType;
    requires std::is_copy_constructible_v<T>;
    requires std::is_move_constructible_v<T>;
};

enum class EventPriority {
    Low,
    Normal,
    High,
    Critical
};

template<typename T>
struct Event {
    using EventType = T;
    
    T data;
    std::chrono::system_clock::time_point timestamp;
    EventPriority priority = EventPriority::Normal;
    std::optional<std::string> source;
    std::optional<std::string> correlationId;
    
    Event(T data, EventPriority priority = EventPriority::Normal)
        : data(std::move(data))
        , timestamp(std::chrono::system_clock::now())
        , priority(priority)
    {}
    
    Event with_source(std::string_view src) && {
        source = src;
        return *this;
    }
    
    Event with_correlation_id(std::string_view id) && {
        correlationId = id;
        return *this;
    }
};

// =============================================================================
// CAMERA EVENTS - Specific event types for camera system
// =============================================================================

struct CameraConnectedEvent {
    using EventType = CameraConnectedEvent;
    core::CameraId cameraId;
    core::CameraInfo cameraInfo;
    std::chrono::system_clock::time_point connectedAt;
};

struct CameraDisconnectedEvent {
    using EventType = CameraDisconnectedEvent;
    core::CameraId cameraId;
    std::chrono::system_clock::time_point disconnectedAt;
    std::optional<std::string> reason;
};

struct CameraErrorEvent {
    using EventType = CameraErrorEvent;
    core::CameraId cameraId;
    core::Error error;
    std::chrono::system_clock::time_point errorAt;
};

struct ImageCapturedEvent {
    using EventType = ImageCapturedEvent;
    core::CameraId cameraId;
    std::unique_ptr<core::ImageBuffer> imageBuffer;
    core::CaptureStatistics statistics;
    std::chrono::system_clock::time_point capturedAt;
};

struct CaptureStartedEvent {
    using EventType = CaptureStartedEvent;
    core::CameraId cameraId;
    core::CaptureSettings settings;
    std::chrono::system_clock::time_point startedAt;
};

struct CaptureStoppedEvent {
    using EventType = CaptureStoppedEvent;
    core::CameraId cameraId;
    std::chrono::system_clock::time_point stoppedAt;
    std::optional<std::string> reason;
};

struct SystemHealthEvent {
    using EventType = SystemHealthEvent;
    double cpuUsage;
    double memoryUsage;
    uint64_t totalFramesProcessed;
    std::vector<core::CaptureStatistics> cameraStatistics;
    std::chrono::system_clock::time_point healthAt;
};

// =============================================================================
// MODERN OBSERVER INTERFACE - Type-safe observers
// =============================================================================

template<typename EventType>
class IEventObserver {
public:
    virtual ~IEventObserver() = default;
    
    /// Handle event (sync)
    virtual void on_event(const Event<EventType>& event) = 0;
    
    /// Handle event (async) - optional override
    virtual std::future<void> on_event_async(const Event<EventType>& event) {
        return std::async(std::launch::async, [this, event]() {
            on_event(event);
        });
    }
    
    /// Get observer info
    virtual std::string get_observer_info() const = 0;
    
    /// Check if observer is active
    virtual bool is_active() const { return true; }
};

// =============================================================================
// MODERN EVENT PUBLISHER - Type-safe event publishing
// =============================================================================

template<typename EventType>
class EventPublisher {
private:
    std::vector<std::weak_ptr<IEventObserver<EventType>>> observers_;
    std::mutex observers_mutex_;
    std::atomic<bool> enabled_{true};
    
public:
    /// Subscribe observer
    void subscribe(std::shared_ptr<IEventObserver<EventType>> observer) {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.push_back(observer);
    }
    
    /// Unsubscribe observer
    void unsubscribe(std::shared_ptr<IEventObserver<EventType>> observer) {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [&observer](const std::weak_ptr<IEventObserver<EventType>>& weak_obs) {
                    return weak_obs.expired() || weak_obs.lock() == observer;
                }
            ),
            observers_.end()
        );
    }
    
    /// Publish event synchronously
    void publish(const Event<EventType>& event) {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(observers_mutex_);
        
        // Clean up expired observers
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [](const std::weak_ptr<IEventObserver<EventType>>& weak_obs) {
                    return weak_obs.expired();
                }
            ),
            observers_.end()
        );
        
        // Notify active observers
        for (const auto& weak_obs : observers_) {
            if (auto obs = weak_obs.lock(); obs && obs->is_active()) {
                try {
                    obs->on_event(event);
                } catch (const std::exception& e) {
                    // Log error but continue with other observers
                    std::cerr << "Observer error: " << e.what() << std::endl;
                }
            }
        }
    }
    
    /// Publish event asynchronously
    std::future<void> publish_async(const Event<EventType>& event) {
        if (!enabled_) {
            return std::async(std::launch::deferred, []() {});
        }
        
        return std::async(std::launch::async, [this, event]() {
            std::lock_guard<std::mutex> lock(observers_mutex_);
            
            std::vector<std::future<void>> futures;
            
            for (const auto& weak_obs : observers_) {
                if (auto obs = weak_obs.lock(); obs && obs->is_active()) {
                    try {
                        futures.push_back(obs->on_event_async(event));
                    } catch (const std::exception& e) {
                        std::cerr << "Observer async error: " << e.what() << std::endl;
                    }
                }
            }
            
            // Wait for all observers to complete
            for (auto& future : futures) {
                try {
                    future.get();
                } catch (const std::exception& e) {
                    std::cerr << "Observer async completion error: " << e.what() << std::endl;
                }
            }
        });
    }
    
    /// Get observer count
    [[nodiscard]] size_t get_observer_count() const {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        return std::count_if(observers_.begin(), observers_.end(),
            [](const std::weak_ptr<IEventObserver<EventType>>& weak_obs) {
                return !weak_obs.expired();
            }
        );
    }
    
    /// Enable/disable publishing
    void set_enabled(bool enabled) {
        enabled_ = enabled;
    }
    
    [[nodiscard]] bool is_enabled() const {
        return enabled_;
    }
};

// =============================================================================
// MODERN EVENT BUS - Centralized event management
// =============================================================================

class EventBus {
private:
    // Type-erased event publishers
    std::unordered_map<std::string, std::unique_ptr<void>> publishers_;
    std::mutex publishers_mutex_;
    
    // Event processing thread
    std::thread processing_thread_;
    std::atomic<bool> running_{false};
    
    // Event queue for async processing
    struct QueuedEvent {
        std::function<void()> processor;
        EventPriority priority;
        std::chrono::system_clock::time_point timestamp;
        
        bool operator<(const QueuedEvent& other) const {
            // Higher priority first, then older timestamp
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return timestamp > other.timestamp;
        }
    };
    
    std::priority_queue<QueuedEvent> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    template<typename EventType>
    EventPublisher<EventType>* get_publisher() {
        const std::string type_name = typeid(EventType).name();
        std::lock_guard<std::mutex> lock(publishers_mutex_);
        
        auto it = publishers_.find(type_name);
        if (it == publishers_.end()) {
            auto publisher = std::make_unique<EventPublisher<EventType>>();
            auto* ptr = publisher.get();
            publishers_[type_name] = std::move(publisher);
            return ptr;
        }
        
        return static_cast<EventPublisher<EventType>*>(it->second.get());
    }
    
    void process_events() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            queue_cv_.wait(lock, [this] {
                return !event_queue_.empty() || !running_;
            });
            
            if (!running_) break;
            
            if (!event_queue_.empty()) {
                auto event = event_queue_.top();
                event_queue_.pop();
                lock.unlock();
                
                try {
                    event.processor();
                } catch (const std::exception& e) {
                    std::cerr << "Event processing error: " << e.what() << std::endl;
                }
            }
        }
    }
    
public:
    EventBus() = default;
    
    ~EventBus() {
        stop();
    }
    
    /// Start event processing
    void start() {
        if (!running_) {
            running_ = true;
            processing_thread_ = std::thread(&EventBus::process_events, this);
        }
    }
    
    /// Stop event processing
    void stop() {
        if (running_) {
            running_ = false;
            queue_cv_.notify_all();
            if (processing_thread_.joinable()) {
                processing_thread_.join();
            }
        }
    }
    
    /// Subscribe to event type
    template<typename EventType>
    void subscribe(std::shared_ptr<IEventObserver<EventType>> observer) {
        auto* publisher = get_publisher<EventType>();
        publisher->subscribe(observer);
    }
    
    /// Unsubscribe from event type
    template<typename EventType>
    void unsubscribe(std::shared_ptr<IEventObserver<EventType>> observer) {
        auto* publisher = get_publisher<EventType>();
        publisher->unsubscribe(observer);
    }
    
    /// Publish event synchronously
    template<typename EventType>
    void publish(const Event<EventType>& event) {
        auto* publisher = get_publisher<EventType>();
        publisher->publish(event);
    }
    
    /// Publish event asynchronously (queued)
    template<typename EventType>
    void publish_async(const Event<EventType>& event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        auto* publisher = get_publisher<EventType>();
        
        event_queue_.push({
            [publisher, event]() {
                publisher->publish(event);
            },
            event.priority,
            event.timestamp
        });
        
        queue_cv_.notify_one();
    }
    
    /// Get statistics
    [[nodiscard]] std::unordered_map<std::string, size_t> get_observer_counts() const {
        std::unordered_map<std::string, size_t> counts;
        std::lock_guard<std::mutex> lock(publishers_mutex_);
        
        for (const auto& [type_name, publisher_ptr] : publishers_) {
            // This is a bit tricky with type erasure - would need RTTI or additional bookkeeping
            // For now, just indicate that publishers exist
            counts[type_name] = 1; // Placeholder
        }
        
        return counts;
    }
    
    /// Get queue size
    [[nodiscard]] size_t get_queue_size() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return event_queue_.size();
    }
    
    /// Check if running
    [[nodiscard]] bool is_running() const {
        return running_;
    }
};

// =============================================================================
// REACTIVE STREAMS - Modern functional reactive programming
// =============================================================================

template<typename T>
class Observable {
private:
    std::function<void(std::function<void(T)>)> subscription_;
    
public:
    explicit Observable(std::function<void(std::function<void(T)>)> subscription)
        : subscription_(std::move(subscription)) {}
    
    /// Subscribe to observable
    void subscribe(std::function<void(T)> observer) {
        subscription_(observer);
    }
    
    /// Transform stream
    template<typename U>
    Observable<U> map(std::function<U(T)> transform) {
        return Observable<U>([this, transform](std::function<void(U)> observer) {
            subscription_([transform, observer](T value) {
                observer(transform(value));
            });
        });
    }
    
    /// Filter stream
    Observable<T> filter(std::function<bool(T)> predicate) {
        return Observable<T>([this, predicate](std::function<void(T)> observer) {
            subscription_([predicate, observer](T value) {
                if (predicate(value)) {
                    observer(value);
                }
            });
        });
    }
    
    /// Take first n items
    Observable<T> take(size_t count) {
        return Observable<T>([this, count](std::function<void(T)> observer) {
            auto remaining = std::make_shared<std::atomic<size_t>>(count);
            subscription_([remaining, observer](T value) {
                if (remaining->load() > 0) {
                    observer(value);
                    remaining->fetch_sub(1);
                }
            });
        });
    }
    
    /// Debounce stream
    Observable<T> debounce(std::chrono::milliseconds delay) {
        return Observable<T>([this, delay](std::function<void(T)> observer) {
            auto last_value = std::make_shared<std::optional<T>>();
            auto timer = std::make_shared<std::thread>();
            auto mutex = std::make_shared<std::mutex>();
            
            subscription_([last_value, timer, mutex, delay, observer](T value) {
                std::lock_guard<std::mutex> lock(*mutex);
                
                *last_value = value;
                
                if (timer->joinable()) {
                    timer->detach();
                }
                
                *timer = std::thread([last_value, delay, observer]() {
                    std::this_thread::sleep_for(delay);
                    if (last_value->has_value()) {
                        observer(*last_value);
                    }
                });
            });
        });
    }
};

// =============================================================================
// REACTIVE CAMERA SYSTEM - Putting it all together
// =============================================================================

class ReactiveSystem {
private:
    std::unique_ptr<EventBus> event_bus_;
    std::vector<std::unique_ptr<void>> observers_; // Type-erased observers
    
public:
    ReactiveSystem() : event_bus_(std::make_unique<EventBus>()) {
        event_bus_->start();
    }
    
    ~ReactiveSystem() {
        event_bus_->stop();
    }
    
    /// Get event bus
    [[nodiscard]] EventBus& get_event_bus() {
        return *event_bus_;
    }
    
    /// Create camera events observable
    Observable<Event<CameraConnectedEvent>> camera_connected_events() {
        return Observable<Event<CameraConnectedEvent>>([this](auto observer) {
            auto obs = std::make_shared<SimpleCameraObserver<CameraConnectedEvent>>(observer);
            event_bus_->subscribe<CameraConnectedEvent>(obs);
            observers_.push_back(std::make_unique<decltype(obs)>(obs));
        });
    }
    
    Observable<Event<ImageCapturedEvent>> image_captured_events() {
        return Observable<Event<ImageCapturedEvent>>([this](auto observer) {
            auto obs = std::make_shared<SimpleCameraObserver<ImageCapturedEvent>>(observer);
            event_bus_->subscribe<ImageCapturedEvent>(obs);
            observers_.push_back(std::make_unique<decltype(obs)>(obs));
        });
    }
    
    /// Publish camera events
    void publish_camera_connected(const core::CameraId& id, const core::CameraInfo& info) {
        auto event = Event<CameraConnectedEvent>{
            CameraConnectedEvent{
                .cameraId = id,
                .cameraInfo = info,
                .connectedAt = std::chrono::system_clock::now()
            },
            EventPriority::High
        };
        
        event_bus_->publish(event);
    }
    
    void publish_image_captured(const core::CameraId& id, std::unique_ptr<core::ImageBuffer> buffer) {
        auto event = Event<ImageCapturedEvent>{
            ImageCapturedEvent{
                .cameraId = id,
                .imageBuffer = std::move(buffer),
                .capturedAt = std::chrono::system_clock::now()
            },
            EventPriority::Normal
        };
        
        event_bus_->publish_async(event);
    }
    
private:
    template<typename EventType>
    class SimpleCameraObserver : public IEventObserver<EventType> {
    private:
        std::function<void(Event<EventType>)> callback_;
        
    public:
        explicit SimpleCameraObserver(std::function<void(Event<EventType>)> callback)
            : callback_(std::move(callback)) {}
        
        void on_event(const Event<EventType>& event) override {
            callback_(event);
        }
        
        std::string get_observer_info() const override {
            return "SimpleCameraObserver";
        }
    };
};

} // namespace sapera::reactive
 