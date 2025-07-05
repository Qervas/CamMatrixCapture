/**
 * Professional JSON Helper Implementation
 * Basic JSON parsing and serialization for configuration management
 */

#include "JsonHelper.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace SaperaCapturePro {

std::string JsonHelper::lastError_;

// JsonValue implementations
JsonValue::JsonValue(const JsonObject& obj) 
    : value_(std::make_shared<JsonObject>(obj)) {}

JsonValue::JsonValue(const JsonArray& arr) 
    : value_(std::make_shared<JsonArray>(arr)) {}

bool JsonValue::getBool(bool defaultValue) const {
    if (std::holds_alternative<bool>(value_)) {
        return std::get<bool>(value_);
    }
    return defaultValue;
}

int JsonValue::getInt(int defaultValue) const {
    if (std::holds_alternative<int64_t>(value_)) {
        return static_cast<int>(std::get<int64_t>(value_));
    }
    if (std::holds_alternative<double>(value_)) {
        return static_cast<int>(std::get<double>(value_));
    }
    return defaultValue;
}

int64_t JsonValue::getInt64(int64_t defaultValue) const {
    if (std::holds_alternative<int64_t>(value_)) {
        return std::get<int64_t>(value_);
    }
    if (std::holds_alternative<double>(value_)) {
        return static_cast<int64_t>(std::get<double>(value_));
    }
    return defaultValue;
}

double JsonValue::getDouble(double defaultValue) const {
    if (std::holds_alternative<double>(value_)) {
        return std::get<double>(value_);
    }
    if (std::holds_alternative<int64_t>(value_)) {
        return static_cast<double>(std::get<int64_t>(value_));
    }
    return defaultValue;
}

std::string JsonValue::getString(const std::string& defaultValue) const {
    if (std::holds_alternative<std::string>(value_)) {
        return std::get<std::string>(value_);
    }
    return defaultValue;
}

JsonObject JsonValue::getObject() const {
    if (std::holds_alternative<std::shared_ptr<JsonObject>>(value_)) {
        return *std::get<std::shared_ptr<JsonObject>>(value_);
    }
    return JsonObject();
}

JsonArray JsonValue::getArray() const {
    if (std::holds_alternative<std::shared_ptr<JsonArray>>(value_)) {
        return *std::get<std::shared_ptr<JsonArray>>(value_);
    }
    return JsonArray();
}

std::string JsonValue::toString() const {
    return JsonHelper::stringify(*this);
}

// JsonObject implementations
bool JsonObject::has(const std::string& key) const {
    return data_.find(key) != data_.end();
}

JsonValue JsonObject::get(const std::string& key) const {
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : JsonValue();
}

JsonValue JsonObject::get(const std::string& key, const JsonValue& defaultValue) const {
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : defaultValue;
}

void JsonObject::set(const std::string& key, const JsonValue& value) {
    data_[key] = value;
}

bool JsonObject::remove(const std::string& key) {
    return data_.erase(key) > 0;
}

std::vector<std::string> JsonObject::keys() const {
    std::vector<std::string> result;
    result.reserve(data_.size());
    for (const auto& pair : data_) {
        result.push_back(pair.first);
    }
    return result;
}

JsonValue& JsonObject::operator[](const std::string& key) {
    return data_[key];
}

const JsonValue& JsonObject::operator[](const std::string& key) const {
    static JsonValue nullValue;
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : nullValue;
}

std::string JsonObject::toString() const {
    return JsonHelper::stringify(JsonValue(*this));
}

// JsonArray implementations
JsonValue JsonArray::get(size_t index) const {
    return (index < data_.size()) ? data_[index] : JsonValue();
}

JsonValue JsonArray::get(size_t index, const JsonValue& defaultValue) const {
    return (index < data_.size()) ? data_[index] : defaultValue;
}

void JsonArray::set(size_t index, const JsonValue& value) {
    if (index >= data_.size()) {
        data_.resize(index + 1);
    }
    data_[index] = value;
}

void JsonArray::push(const JsonValue& value) {
    data_.push_back(value);
}

bool JsonArray::remove(size_t index) {
    if (index >= data_.size()) {
        return false;
    }
    data_.erase(data_.begin() + index);
    return true;
}

JsonValue& JsonArray::operator[](size_t index) {
    if (index >= data_.size()) {
        data_.resize(index + 1);
    }
    return data_[index];
}

const JsonValue& JsonArray::operator[](size_t index) const {
    static JsonValue nullValue;
    return (index < data_.size()) ? data_[index] : nullValue;
}

std::string JsonArray::toString() const {
    return JsonHelper::stringify(JsonValue(*this));
}

// JsonHelper implementations
JsonValue JsonHelper::parse(const std::string& jsonString) {
    lastError_.clear();
    size_t pos = 0;
    
    try {
        skipWhitespace(jsonString, pos);
        if (pos >= jsonString.length()) {
            lastError_ = "Empty JSON string";
            return JsonValue();
        }
        
        return parseValue(jsonString, pos);
    } catch (const std::exception& e) {
        lastError_ = e.what();
        return JsonValue();
    }
}

JsonValue JsonHelper::parseFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastError_ = "Could not open file: " + filePath;
        return JsonValue();
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    return parse(content);
}

std::string JsonHelper::stringify(const JsonValue& value, bool pretty) {
    std::ostringstream oss;
    
    if (value.isNull()) {
        oss << "null";
    } else if (value.isBool()) {
        oss << (value.getBool() ? "true" : "false");
    } else if (value.isInt()) {
        oss << value.getInt64();
    } else if (value.isDouble()) {
        oss << value.getDouble();
    } else if (value.isString()) {
        oss << "\"" << escapeString(value.getString()) << "\"";
    } else if (value.isObject()) {
        JsonObject obj = value.getObject();
        oss << "{";
        bool first = true;
        for (const auto& key : obj.keys()) {
            if (!first) oss << ",";
            if (pretty) oss << "\n  ";
            oss << "\"" << escapeString(key) << "\":";
            if (pretty) oss << " ";
            oss << stringify(obj.get(key), false); // Nested pretty printing simplified for now
            first = false;
        }
        if (pretty && !obj.keys().empty()) oss << "\n";
        oss << "}";
    } else if (value.isArray()) {
        JsonArray arr = value.getArray();
        oss << "[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) oss << ",";
            if (pretty) oss << "\n  ";
            oss << stringify(arr.get(i), false);
        }
        if (pretty && arr.size() > 0) oss << "\n";
        oss << "]";
    }
    
    return oss.str();
}

bool JsonHelper::saveToFile(const JsonValue& value, const std::string& filePath, bool pretty) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        lastError_ = "Could not open file for writing: " + filePath;
        return false;
    }
    
    file << stringify(value, pretty);
    file.close();
    
    return true;
}

bool JsonHelper::isValidJson(const std::string& jsonString) {
    JsonValue result = parse(jsonString);
    return lastError_.empty();
}

std::string JsonHelper::getLastError() {
    return lastError_;
}

JsonObject JsonHelper::createObject() {
    return JsonObject();
}

JsonArray JsonHelper::createArray() {
    return JsonArray();
}

// Private helper methods
JsonValue JsonHelper::parseValue(const std::string& str, size_t& pos) {
    skipWhitespace(str, pos);
    
    if (pos >= str.length()) {
        throw std::runtime_error("Unexpected end of JSON");
    }
    
    char c = str[pos];
    
    if (c == '{') {
        return JsonValue(parseObject(str, pos));
    } else if (c == '[') {
        return JsonValue(parseArray(str, pos));
    } else if (c == '"') {
        return JsonValue(parseString(str, pos));
    } else if (c == 't' || c == 'f') {
        // Parse boolean
        if (str.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue(true);
        } else if (str.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue(false);
        } else {
            throw std::runtime_error("Invalid boolean value");
        }
    } else if (c == 'n') {
        // Parse null
        if (str.substr(pos, 4) == "null") {
            pos += 4;
            return JsonValue();
        } else {
            throw std::runtime_error("Invalid null value");
        }
    } else if (std::isdigit(c) || c == '-') {
        return parseNumber(str, pos);
    } else {
        throw std::runtime_error("Unexpected character: " + std::string(1, c));
    }
}

JsonObject JsonHelper::parseObject(const std::string& str, size_t& pos) {
    JsonObject obj;
    
    if (str[pos] != '{') {
        throw std::runtime_error("Expected '{'");
    }
    pos++; // Skip '{'
    
    skipWhitespace(str, pos);
    
    if (pos < str.length() && str[pos] == '}') {
        pos++; // Skip '}'
        return obj; // Empty object
    }
    
    while (pos < str.length()) {
        skipWhitespace(str, pos);
        
        // Parse key
        if (str[pos] != '"') {
            throw std::runtime_error("Expected string key");
        }
        std::string key = parseString(str, pos);
        
        skipWhitespace(str, pos);
        
        // Expect ':'
        if (pos >= str.length() || str[pos] != ':') {
            throw std::runtime_error("Expected ':'");
        }
        pos++; // Skip ':'
        
        skipWhitespace(str, pos);
        
        // Parse value
        JsonValue value = parseValue(str, pos);
        obj.set(key, value);
        
        skipWhitespace(str, pos);
        
        if (pos >= str.length()) {
            throw std::runtime_error("Unexpected end of object");
        }
        
        if (str[pos] == '}') {
            pos++; // Skip '}'
            break;
        } else if (str[pos] == ',') {
            pos++; // Skip ','
        } else {
            throw std::runtime_error("Expected ',' or '}'");
        }
    }
    
    return obj;
}

JsonArray JsonHelper::parseArray(const std::string& str, size_t& pos) {
    JsonArray arr;
    
    if (str[pos] != '[') {
        throw std::runtime_error("Expected '['");
    }
    pos++; // Skip '['
    
    skipWhitespace(str, pos);
    
    if (pos < str.length() && str[pos] == ']') {
        pos++; // Skip ']'
        return arr; // Empty array
    }
    
    while (pos < str.length()) {
        skipWhitespace(str, pos);
        
        JsonValue value = parseValue(str, pos);
        arr.push(value);
        
        skipWhitespace(str, pos);
        
        if (pos >= str.length()) {
            throw std::runtime_error("Unexpected end of array");
        }
        
        if (str[pos] == ']') {
            pos++; // Skip ']'
            break;
        } else if (str[pos] == ',') {
            pos++; // Skip ','
        } else {
            throw std::runtime_error("Expected ',' or ']'");
        }
    }
    
    return arr;
}

std::string JsonHelper::parseString(const std::string& str, size_t& pos) {
    if (str[pos] != '"') {
        throw std::runtime_error("Expected '\"'");
    }
    pos++; // Skip opening '"'
    
    std::string result;
    while (pos < str.length() && str[pos] != '"') {
        if (str[pos] == '\\') {
            pos++; // Skip '\'
            if (pos >= str.length()) {
                throw std::runtime_error("Unexpected end of string");
            }
            
            char escaped = str[pos];
            switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default:
                    result += escaped; // Simple fallback
                    break;
            }
        } else {
            result += str[pos];
        }
        pos++;
    }
    
    if (pos >= str.length()) {
        throw std::runtime_error("Unterminated string");
    }
    
    pos++; // Skip closing '"'
    return result;
}

JsonValue JsonHelper::parseNumber(const std::string& str, size_t& pos) {
    size_t start = pos;
    
    // Handle negative sign
    if (str[pos] == '-') {
        pos++;
    }
    
    // Parse digits
    bool hasDigits = false;
    while (pos < str.length() && std::isdigit(str[pos])) {
        pos++;
        hasDigits = true;
    }
    
    if (!hasDigits) {
        throw std::runtime_error("Invalid number");
    }
    
    bool isFloat = false;
    
    // Handle decimal point
    if (pos < str.length() && str[pos] == '.') {
        isFloat = true;
        pos++;
        
        hasDigits = false;
        while (pos < str.length() && std::isdigit(str[pos])) {
            pos++;
            hasDigits = true;
        }
        
        if (!hasDigits) {
            throw std::runtime_error("Invalid decimal number");
        }
    }
    
    // Handle exponent (simplified)
    if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
        isFloat = true;
        pos++;
        
        if (pos < str.length() && (str[pos] == '+' || str[pos] == '-')) {
            pos++;
        }
        
        hasDigits = false;
        while (pos < str.length() && std::isdigit(str[pos])) {
            pos++;
            hasDigits = true;
        }
        
        if (!hasDigits) {
            throw std::runtime_error("Invalid exponent");
        }
    }
    
    std::string numberStr = str.substr(start, pos - start);
    
    if (isFloat) {
        return JsonValue(std::stod(numberStr));
    } else {
        return JsonValue(static_cast<int64_t>(std::stoll(numberStr)));
    }
}

void JsonHelper::skipWhitespace(const std::string& str, size_t& pos) {
    while (pos < str.length() && std::isspace(str[pos])) {
        pos++;
    }
}

std::string JsonHelper::escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 2); // Reserve space for potential escaping
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                result += c;
                break;
        }
    }
    
    return result;
}

std::string JsonHelper::unescapeString(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            char next = str[i + 1];
            switch (next) {
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case 'b': result += '\b'; i++; break;
                case 'f': result += '\f'; i++; break;
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                default:
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

} // namespace SaperaCapturePro 