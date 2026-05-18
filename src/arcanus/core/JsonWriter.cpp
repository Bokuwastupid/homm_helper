#include "arcanus/core/JsonWriter.h"

#include <iomanip>

namespace arcanus {

void JsonWriter::BeginObject() {
    BeforeValue();
    out_ << "{";
    PushCommaScope();
}

void JsonWriter::EndObject() {
    out_ << "}";
    PopCommaScope();
}

void JsonWriter::BeginArray() {
    BeforeValue();
    out_ << "[";
    PushCommaScope();
}

void JsonWriter::EndArray() {
    out_ << "]";
    PopCommaScope();
}

void JsonWriter::Key(const std::string& key) {
    BeforeValue();
    out_ << "\"" << Escape(key) << "\":";
    after_key_ = true;
}

void JsonWriter::KeyNull(const std::string& key) {
    Key(key);
    out_ << "null";
    after_key_ = false;
}

void JsonWriter::KeyValue(const std::string& key, const std::string& value) {
    Key(key);
    out_ << "\"" << Escape(value) << "\"";
    after_key_ = false;
}

void JsonWriter::KeyValue(const std::string& key, const char* value) {
    KeyValue(key, std::string(value));
}

void JsonWriter::KeyValue(const std::string& key, int value) {
    Key(key);
    out_ << value;
    after_key_ = false;
}

void JsonWriter::KeyValue(const std::string& key, float value) {
    Key(key);
    out_ << std::fixed << std::setprecision(3) << value;
    after_key_ = false;
}

void JsonWriter::KeyValue(const std::string& key, bool value) {
    Key(key);
    out_ << (value ? "true" : "false");
    after_key_ = false;
}

void JsonWriter::Value(const std::string& value) {
    BeforeValue();
    out_ << "\"" << Escape(value) << "\"";
}

std::string JsonWriter::Str() const {
    return out_.str();
}

void JsonWriter::BeforeValue() {
    if (after_key_) {
        after_key_ = false;
        return;
    }
    if (!needs_comma_.empty()) {
        if (needs_comma_.back()) {
            out_ << ",";
        }
        needs_comma_.back() = true;
    }
}

void JsonWriter::PushCommaScope() {
    needs_comma_.push_back(false);
}

void JsonWriter::PopCommaScope() {
    if (!needs_comma_.empty()) {
        needs_comma_.pop_back();
    }
}

std::string JsonWriter::Escape(const std::string& value) {
    std::ostringstream escaped;
    for (char c : value) {
        switch (c) {
        case '\\': escaped << "\\\\"; break;
        case '"': escaped << "\\\""; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default: escaped << c; break;
        }
    }
    return escaped.str();
}

}

