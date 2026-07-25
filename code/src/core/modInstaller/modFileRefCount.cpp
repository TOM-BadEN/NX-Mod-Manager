/**
 * ModFileRefCount - MOD 共享文件引用计数实现
 */

#include "core/modInstaller/modFileRefCount.hpp"

namespace {
    constexpr const char* refCount = "refCount";
}

bool ModFileRefCount::load(const std::string& path) {
    return m_json.load(path);
}

bool ModFileRefCount::save() {
    return m_json.save();
}

void ModFileRefCount::increment(const std::string& filePath) {
    std::string val = m_json.getString(refCount, filePath, "0");
    int count = std::stoi(val) + 1;
    m_json.setString(refCount, filePath, std::to_string(count));
}

bool ModFileRefCount::decrement(const std::string& filePath) {
    std::string val = m_json.getString(refCount, filePath, "");
    if (val.empty()) return true;

    int count = std::stoi(val) - 1;
    if (count <= 0) m_json.removeKey(refCount, filePath);
    else m_json.setString(refCount, filePath, std::to_string(count));
    return false;
}
