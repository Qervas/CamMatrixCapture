/**
 * Professional JSON Helper
 * Provides a clean, type-safe interface for JSON operations
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <functional>

namespace SaperaCapturePro {

// Forward declarations
class JsonValue;
class JsonObject;
class JsonArray;

/**
 * JSON value variant type
 */
using JsonVariant = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::shared_ptr<JsonObject>,
    std::shared_ptr<JsonArray>
>;

/**
 * JSON value wrapper with type-safe access
 */
class JsonValue {
public:
    JsonValue() : value_(nullptr) {}
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool val) : value_(val) {}
    JsonValue(int val) : value_(static_cast<int64_t>(val)) {}
    JsonValue(int64_t val) : value_(val) {}
    JsonValue(double val) : value_(val) {}
    JsonValue(const std::string& val) : value_(val) {}
    JsonValue(const char* val) : value_(std::string(val)) {}
    JsonValue(const JsonObject& obj);
    JsonValue(const JsonArray& arr);
    
    // Type checking
    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isNumber() const { return std::holds_alternative<int64_t>(value_) || std::holds_alternative<double>(value_); }
    bool isInt() const { return std::holds_alternative<int64_t>(value_); }
    bool isDouble() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isObject() const { return std::holds_alternative<std::shared_ptr<JsonObject>>(value_); }
    bool isArray() const { return std::holds_alternative<std::shared_ptr<JsonArray>>(value_); }
    
    // Value extraction with defaults
    bool getBool(bool defaultValue = false) const;
    int getInt(int defaultValue = 0) const;
    int64_t getInt64(int64_t defaultValue = 0) const;
    double getDouble(double defaultValue = 0.0) const;
    std::string getString(const std::string& defaultValue = "") const;
    JsonObject getObject() const;
    JsonArray getArray() const;
    
    // Conversion to string
    std::string toString() const;
    
    // Assignment operators
    JsonValue& operator=(const JsonVariant& val) { value_ = val; return *this; }
    
private:
    JsonVariant value_;
};

/**
 * JSON object wrapper
 */
class JsonObject {
public:
    JsonObject() = default;
    
    // Access methods
    bool has(const std::string& key) const;
    JsonValue get(const std::string& key) const;
    JsonValue get(const std::string& key, const JsonValue& defaultValue) const;
    void set(const std::string& key, const JsonValue& value);
    bool remove(const std::string& key);
    
    // Iteration
    std::vector<std::string> keys() const;
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    
    // Operators
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    
    // Conversion
    std::string toString() const;
    
private:
    std::map<std::string, JsonValue> data_;
};

/**
 * JSON array wrapper
 */
class JsonArray {
public:
    JsonArray() = default;
    
    // Access methods
    JsonValue get(size_t index) const;
    JsonValue get(size_t index, const JsonValue& defaultValue) const;
    void set(size_t index, const JsonValue& value);
    void push(const JsonValue& value);
    bool remove(size_t index);
    
    // Properties
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }
    
    // Operators
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;
    
    // Conversion
    std::string toString() const;
    
private:
    std::vector<JsonValue> data_;
};

/**
 * JSON parsing and serialization utilities
 */
class JsonHelper {
public:
    // Parsing
    static JsonValue parse(const std::string& jsonString);
    static JsonValue parseFile(const std::string& filePath);
    
    // Serialization
    static std::string stringify(const JsonValue& value, bool pretty = false);
    static bool saveToFile(const JsonValue& value, const std::string& filePath, bool pretty = true);
    
    // Validation
    static bool isValidJson(const std::string& jsonString);
    static std::string getLastError();
    
    // Utility functions
    static JsonObject createObject();
    static JsonArray createArray();
    
private:
    static std::string lastError_;
    static JsonValue parseValue(const std::string& str, size_t& pos);
    static JsonObject parseObject(const std::string& str, size_t& pos);
    static JsonArray parseArray(const std::string& str, size_t& pos);
    static std::string parseString(const std::string& str, size_t& pos);
    static JsonValue parseNumber(const std::string& str, size_t& pos);
    static void skipWhitespace(const std::string& str, size_t& pos);
    static std::string escapeString(const std::string& str);
    static std::string unescapeString(const std::string& str);
};

} // namespace SaperaCapturePro 