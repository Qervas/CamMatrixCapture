// Simple JSON parser for camera configuration
// Based on the nlohmann/json library but with minimal functionality

#ifndef JSON_HPP
#define JSON_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace simple_json {

class JsonValue;

// Forward declarations for JSON types
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

// Main JSON value class that can hold different types
class JsonValue {
public:
    enum Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    // Constructors for different types
    JsonValue() : type_(Null) {}
    JsonValue(bool value) : type_(Boolean), bool_value_(value) {}
    JsonValue(int value) : type_(Number), number_value_(value) {}
    JsonValue(double value) : type_(Number), number_value_(value) {}
    JsonValue(const std::string& value) : type_(String), string_value_(value) {}
    JsonValue(const char* value) : type_(String), string_value_(value) {}
    JsonValue(const JsonArray& value) : type_(Array), array_value_(value) {}
    JsonValue(const JsonObject& value) : type_(Object), object_value_(value) {}

    // Type checking
    bool is_null() const { return type_ == Null; }
    bool is_boolean() const { return type_ == Boolean; }
    bool is_number() const { return type_ == Number; }
    bool is_string() const { return type_ == String; }
    bool is_array() const { return type_ == Array; }
    bool is_object() const { return type_ == Object; }
    bool is_valid() const { return true; } // Simple implementation - all constructed values are valid

    // Serialization to JSON string
    std::string serialize() const {
        switch (type_) {
            case Null:
                return "null";
            case Boolean:
                return bool_value_ ? "true" : "false";
            case Number:
                return std::to_string(number_value_);
            case String:
                return "\"" + escape_string(string_value_) + "\"";
            case Array:
                return serialize_array();
            case Object:
                return serialize_object();
        }
        return "null";
    }

    // Value getters
    bool get_boolean() const {
        if (type_ != Boolean) throw std::runtime_error("Not a boolean");
        return bool_value_;
    }

    double get_number() const {
        if (type_ != Number) throw std::runtime_error("Not a number");
        return number_value_;
    }

    int get_int() const {
        return static_cast<int>(get_number());
    }

    const std::string& get_string() const {
        if (type_ != String) throw std::runtime_error("Not a string");
        return string_value_;
    }

    const JsonArray& get_array() const {
        if (type_ != Array) throw std::runtime_error("Not an array");
        return array_value_;
    }

    const JsonObject& get_object() const {
        if (type_ != Object) throw std::runtime_error("Not an object");
        return object_value_;
    }

    // Object access
    const JsonValue& operator[](const std::string& key) const {
        if (type_ != Object) throw std::runtime_error("Not an object");
        auto it = object_value_.find(key);
        if (it == object_value_.end()) throw std::runtime_error("Key not found: " + key);
        return it->second;
    }

    // Array access
    const JsonValue& operator[](size_t index) const {
        if (type_ != Array) throw std::runtime_error("Not an array");
        if (index >= array_value_.size()) throw std::runtime_error("Array index out of bounds");
        return array_value_[index];
    }

    // Helper method to safely get values with defaults
    template<typename T>
    T value(const std::string& key, const T& default_value) const {
        if (type_ != Object) return default_value;
        auto it = object_value_.find(key);
        if (it == object_value_.end()) return default_value;
        try {
            if constexpr (std::is_same_v<T, bool>) {
                return it->second.get_boolean();
            } else if constexpr (std::is_integral_v<T>) {
                return static_cast<T>(it->second.get_int());
            } else if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(it->second.get_number());
            } else if constexpr (std::is_same_v<T, std::string>) {
                return it->second.get_string();
            }
        } catch (...) {
            return default_value;
        }
        return default_value;
    }

private:
    Type type_;
    bool bool_value_ = false;
    double number_value_ = 0.0;
    std::string string_value_;
    JsonArray array_value_;
    JsonObject object_value_;

    std::string escape_string(const std::string& str) const {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                case '\\': result += "\\\\"; break;
                case '"': result += "\\\""; break;
                default: result += c; break;
            }
        }
        return result;
    }

    std::string serialize_array() const {
        std::string result = "[";
        for (size_t i = 0; i < array_value_.size(); ++i) {
            if (i > 0) result += ",";
            result += array_value_[i].serialize();
        }
        result += "]";
        return result;
    }

    std::string serialize_object() const {
        std::string result = "{";
        bool first = true;
        for (const auto& pair : object_value_) {
            if (!first) result += ",";
            first = false;
            result += "\"" + escape_string(pair.first) + "\":" + pair.second.serialize();
        }
        result += "}";
        return result;
    }
};

// Simple JSON parser
class JsonParser {
public:
    static JsonValue parse(const std::string& json_string) {
        std::istringstream stream(json_string);
        return parse_value(stream);
    }

    static JsonValue parse_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }

private:
    static void skip_whitespace(std::istream& stream) {
        while (stream.peek() == ' ' || stream.peek() == '\t' || 
               stream.peek() == '\n' || stream.peek() == '\r') {
            stream.get();
        }
    }

    static JsonValue parse_value(std::istream& stream) {
        skip_whitespace(stream);
        char next = stream.peek();

        if (next == '{') {
            return parse_object(stream);
        } else if (next == '[') {
            return parse_array(stream);
        } else if (next == '"') {
            return parse_string(stream);
        } else if (next == 't' || next == 'f') {
            return parse_boolean(stream);
        } else if (next == 'n') {
            return parse_null(stream);
        } else if (next == '-' || (next >= '0' && next <= '9')) {
            return parse_number(stream);
        }
        
        throw std::runtime_error("Unexpected character in JSON");
    }

    static JsonValue parse_object(std::istream& stream) {
        JsonObject obj;
        char c;
        stream >> c; // Skip '{'
        
        skip_whitespace(stream);
        if (stream.peek() == '}') {
            stream.get(); // Skip '}'
            return obj;
        }
        
        while (true) {
            skip_whitespace(stream);
            
            // Parse key (must be a string)
            if (stream.peek() != '"') {
                throw std::runtime_error("Expected string as object key");
            }
            std::string key = parse_string(stream).get_string();
            
            skip_whitespace(stream);
            stream >> c; // Skip ':'
            if (c != ':') {
                throw std::runtime_error("Expected ':' after object key");
            }
            
            // Parse value
            obj[key] = parse_value(stream);
            
            skip_whitespace(stream);
            stream >> c; // Skip ',' or '}'
            
            if (c == '}') {
                break;
            } else if (c != ',') {
                throw std::runtime_error("Expected ',' or '}' in object");
            }
        }
        
        return obj;
    }

    static JsonValue parse_array(std::istream& stream) {
        JsonArray arr;
        char c;
        stream >> c; // Skip '['
        
        skip_whitespace(stream);
        if (stream.peek() == ']') {
            stream.get(); // Skip ']'
            return arr;
        }
        
        while (true) {
            arr.push_back(parse_value(stream));
            
            skip_whitespace(stream);
            stream >> c; // Skip ',' or ']'
            
            if (c == ']') {
                break;
            } else if (c != ',') {
                throw std::runtime_error("Expected ',' or ']' in array");
            }
        }
        
        return arr;
    }

    static JsonValue parse_string(std::istream& stream) {
        std::string result;
        char c;
        
        stream >> c; // Skip opening quote
        if (c != '"') {
            throw std::runtime_error("Expected opening quote for string");
        }
        
        while (stream.peek() != EOF && stream.peek() != '"') {
            c = stream.get();
            if (c == '\\') {
                // Handle escape sequences
                c = stream.get();
                switch (c) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += c; break;
                }
            } else {
                result += c;
            }
        }
        
        stream >> c; // Skip closing quote
        if (c != '"') {
            throw std::runtime_error("Expected closing quote for string");
        }
        
        return result;
    }

    static JsonValue parse_boolean(std::istream& stream) {
        if (stream.peek() == 't') {
            // Parse "true"
            char buf[4];
            stream.read(buf, 4);
            std::string value(buf, 4);
            if (value != "true") {
                throw std::runtime_error("Expected 'true'");
            }
            return true;
        } else {
            // Parse "false"
            char buf[5];
            stream.read(buf, 5);
            std::string value(buf, 5);
            if (value != "false") {
                throw std::runtime_error("Expected 'false'");
            }
            return false;
        }
    }

    static JsonValue parse_null(std::istream& stream) {
        char buf[4];
        stream.read(buf, 4);
        std::string value(buf, 4);
        if (value != "null") {
            throw std::runtime_error("Expected 'null'");
        }
        return JsonValue();
    }

    static JsonValue parse_number(std::istream& stream) {
        std::string num_str;
        while (stream.peek() == '-' || stream.peek() == '+' || 
               stream.peek() == '.' || stream.peek() == 'e' || 
               stream.peek() == 'E' || (stream.peek() >= '0' && stream.peek() <= '9')) {
            num_str += stream.get();
        }
        
        try {
            return std::stod(num_str);
        } catch (...) {
            throw std::runtime_error("Invalid number: " + num_str);
        }
    }
};

} // namespace simple_json

#endif // JSON_HPP 