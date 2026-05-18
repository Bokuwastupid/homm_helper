#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace arcanus {

class JsonWriter {
public:
    void BeginObject();
    void EndObject();
    void BeginArray();
    void EndArray();

    void Key(const std::string& key);
    void KeyNull(const std::string& key);
    void KeyValue(const std::string& key, const std::string& value);
    void KeyValue(const std::string& key, const char* value);
    void KeyValue(const std::string& key, int value);
    void KeyValue(const std::string& key, float value);
    void KeyValue(const std::string& key, bool value);
    void Value(const std::string& value);

    std::string Str() const;

private:
    void BeforeValue();
    void PushCommaScope();
    void PopCommaScope();
    static std::string Escape(const std::string& value);

    std::ostringstream out_;
    std::vector<bool> needs_comma_;
    bool after_key_ = false;
};

}

