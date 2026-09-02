/**
 * ModInstaller - 模组安装目标策略实现
 */

#include "core/modInstaller/targetStrategy.hpp"

#include <algorithm>
#include <array>

namespace {

constexpr const char* atmospherePath = "/atmosphere";
constexpr const char* contentsPath = "/atmosphere/contents";

// IPS 文件后缀检查不依赖 filesystem，便于在主机侧单独测试路径策略。
bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// 只匹配完整路径段，避免把 romfs_backup 或 my_nro_patches 误识别为根目录。
bool findPathSegment(const std::string& path, const std::string& segment, size_t& pos) {
    size_t searchPos = 0;
    while ((searchPos = path.find(segment, searchPos)) != std::string::npos) {
        bool leftOk = searchPos == 0 || path[searchPos - 1] == '/';
        size_t end = searchPos + segment.size();
        bool rightOk = end == path.size() || path[end] == '/';
        if (leftOk && rightOk) {
            pos = searchPos;
            return true;
        }
        searchPos = end;
    }
    return false;
}

// contents 可以省略 Title ID，也可以在关键词前提供 16 位十六进制 Title ID。
// 只有紧邻关键词的路径段才会被当作显式 Title ID。
std::string extractTidBeforeKeyword(const std::string& path, size_t keywordPos) {
    if (keywordPos < 17 || path[keywordPos - 1] != '/') return {};
    if (keywordPos >= 18 && path[keywordPos - 18] != '/') return {};

    for (size_t i = keywordPos - 17; i < keywordPos - 1; ++i) {
        char c = path[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return {};
    }
    return path.substr(keywordPos - 17, 16);
}

// 目录扫描会同时返回根目录和子目录，因此统一去重，保持安装计划稳定。
void appendUnique(std::vector<std::string>& values, const std::string& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

// ManagedTarget 的去重同时比较类型和路径；不同类型不能合并，因为生命周期操作不同。
void appendUnique(std::vector<ModInstaller::targets::ManagedTarget>& values, ModInstaller::targets::ManagedTargetKind kind, const std::string& path) {
    auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.kind == kind && value.path == path;
    });
    if (it == values.end()) values.push_back({kind, path});
}

class ContentsStrategy final : public ModInstaller::targets::TargetStrategy {
public:
    // romfs、romfslite、exefs、cheats 等内容最终都挂到一个 Title ID 目录下。
    bool findKeywordPos(const std::string& path, size_t& pos) const override {
        for (const auto& keyword : m_keywords) {
            if (findPathSegment(path, keyword, pos)) return true;
        }
        return false;
    }

    bool acceptsFile(const std::string&, size_t) const override {
        return true;
    }

    std::string buildTargetPath(const std::string& path, size_t pos, const std::string& gameTid) const override {
        std::string sourceTid = extractTidBeforeKeyword(path, pos);
        const std::string& targetTid = sourceTid.empty() ? gameTid : sourceTid;
        return std::string(contentsPath) + "/" + targetTid + "/" + path.substr(pos);
    }

    void appendTargetDirs(const std::string& path, size_t pos, const std::string& gameTid, std::vector<std::string>& dirs) const override {
        std::string sourceTid = extractTidBeforeKeyword(path, pos);
        const std::string& targetTid = sourceTid.empty() ? gameTid : sourceTid;
        std::string root = std::string(contentsPath) + "/" + targetTid;
        appendUnique(dirs, root);
        appendUnique(dirs, root + "/" + path.substr(pos));
    }

    bool collectsManagedTargetsFromDirs() const override {
        return true;
    }

    void appendManagedTargets(const std::string& path, size_t pos, const std::string& gameTid, std::vector<ModInstaller::targets::ManagedTarget>& targets) const override {
        std::string sourceTid = extractTidBeforeKeyword(path, pos);
        const std::string& targetTid = sourceTid.empty() ? gameTid : sourceTid;
        appendUnique(targets, ModInstaller::targets::ManagedTargetKind::ContentsDirectory, std::string(contentsPath) + "/" + targetTid);
    }

private:
    const std::array<std::string, 5> m_keywords = {"romfs", "romfslite", "exefs", "cheats", "romfs.bin"};
};

class AtmospherePatchStrategy final : public ModInstaller::targets::TargetStrategy {
public:
    AtmospherePatchStrategy(std::string rootName, bool ipsOnly, bool collectFromDirs)
        : m_rootName(std::move(rootName)), m_ipsOnly(ipsOnly), m_collectFromDirs(collectFromDirs) {}

    bool findKeywordPos(const std::string& path, size_t& pos) const override {
        return findPathSegment(path, m_rootName, pos);
    }

    bool acceptsFile(const std::string& path, size_t pos) const override {
        // exefs_patches 保留原有目录结构；nro_patches 严格限制为
        // nro_patches/<补丁集>/<文件>.ips，拒绝说明文件和嵌套路径。
        if (!m_ipsOnly) return true;
        if (!endsWith(path, ".ips")) return false;

        size_t patchSetStart = pos + m_rootName.size();
        if (patchSetStart >= path.size() || path[patchSetStart] != '/') return false;
        size_t patchSetEnd = path.find('/', patchSetStart + 1);
        if (patchSetEnd == std::string::npos || patchSetEnd + 1 >= path.size()) return false;
        return path.find('/', patchSetEnd + 1) == std::string::npos;
    }

    std::string buildTargetPath(const std::string& path, size_t pos, const std::string&) const override {
        return std::string(atmospherePath) + "/" + path.substr(pos);
    }

    void appendTargetDirs(const std::string& path, size_t pos, const std::string&, std::vector<std::string>& dirs) const override {
        std::string root = std::string(atmospherePath) + "/" + m_rootName;
        appendUnique(dirs, root);
        if (m_ipsOnly) {
            // NRO IPS 只创建根目录和补丁集目录，不为非法嵌套目录创建目标。
            size_t patchSetStart = pos + m_rootName.size();
            if (patchSetStart >= path.size() || path[patchSetStart] != '/') return;
            size_t patchSetEnd = path.find('/', patchSetStart + 1);
            std::string patchSet = path.substr(patchSetStart + 1, patchSetEnd - patchSetStart - 1);
            if (!patchSet.empty()) appendUnique(dirs, root + "/" + patchSet);
            return;
        }
        appendUnique(dirs, std::string(atmospherePath) + "/" + path.substr(pos));
    }

    bool collectsManagedTargetsFromDirs() const override {
        return m_collectFromDirs;
    }

    void appendManagedTargets(const std::string& path, size_t pos, const std::string&, std::vector<ModInstaller::targets::ManagedTarget>& targets) const override {
        // 生命周期以补丁集为单位，因此同一补丁集中的多个 IPS 文件会一起处理。
        size_t patchSetStart = pos + m_rootName.size();
        if (patchSetStart >= path.size() || path[patchSetStart] != '/') return;
        ++patchSetStart;

        size_t patchSetEnd = path.find('/', patchSetStart);
        std::string patchSet = path.substr(patchSetStart, patchSetEnd - patchSetStart);
        if (patchSet.empty()) return;

        appendUnique(targets, ModInstaller::targets::ManagedTargetKind::IpsDirectory, std::string(atmospherePath) + "/" + m_rootName + "/" + patchSet);
    }

private:
    std::string m_rootName;       // Atmosphere 根目录名，例如 exefs_patches 或 nro_patches
    bool m_ipsOnly;                // 是否启用 NRO IPS 的严格文件结构校验
    bool m_collectFromDirs;        // 是否允许仅凭目录结构登记生命周期目标
};
//进行策略的注册，注册了/contents，exefs和nro的3个策略方法
const std::array<const ModInstaller::targets::TargetStrategy*, 3>& strategies() {
    // 策略对象无状态，使用静态实例避免每次扫描重复构造。
    static const ContentsStrategy contents;
    static const AtmospherePatchStrategy exefsPatches("exefs_patches", false, true);
    static const AtmospherePatchStrategy nroPatches("nro_patches", true, false);
    static const std::array<const ModInstaller::targets::TargetStrategy*, 3> all = {
        &contents,
        &exefsPatches,
        &nroPatches
    };
    return all;
}

} // namespace

namespace ModInstaller::targets {
//查找策略
const TargetStrategy* TargetStrategyRegistry::find(const std::string& path, size_t& pos) {
    const TargetStrategy* result = nullptr;
    size_t resultPos = std::string::npos;
    for (const auto* strategy : strategies()) {
        size_t candidatePos = 0;
        if (strategy->findKeywordPos(path, candidatePos) && candidatePos < resultPos) {
            result = strategy;
            resultPos = candidatePos;
        }
    }
    // 不能按注册顺序直接返回：补丁集名称可能恰好叫 romfs 等关键词。
    if (result) pos = resultPos;
    return result;
}

size_t findKeywordPos(const std::string& path) {
    // 这是给旧安装器入口保留的兼容封装；新代码应优先使用 Registry::find，
    // 因为它同时返回策略对象和关键词位置。
    size_t pos = 0;
    return TargetStrategyRegistry::find(path, pos) ? pos : std::string::npos;
}

std::string buildTargetPath(const std::string& path, const std::string& gameTid) {
    // 先选策略，再让策略决定文件是否合法和如何映射；未知路径统一返回空字符串。
    size_t pos = 0;
    const TargetStrategy* strategy = TargetStrategyRegistry::find(path, pos);
    if (!strategy || !strategy->acceptsFile(path, pos)) return {};
    return strategy->buildTargetPath(path, pos, gameTid);
}

std::vector<std::string> buildTargetDirs(const std::vector<std::string>& sourceDirs, const std::string& gameTid) {
    // 目录创建计划与文件安装计划共用同一策略识别，避免目录能创建但文件不能安装。
    std::vector<std::string> result;
    result.reserve(sourceDirs.size() + 2);

    for (const auto& path : sourceDirs) {
        size_t pos = 0;
        const TargetStrategy* strategy = TargetStrategyRegistry::find(path, pos);
        if (strategy) strategy->appendTargetDirs(path, pos, gameTid, result);
    }
    return result;
}

std::vector<ManagedTarget> collectManagedTargets(const std::vector<std::string>& sourceDirs, const std::vector<std::string>& sourceFiles, const std::string& gameTid) {
    std::vector<ManagedTarget> result;

    for (const auto& path : sourceDirs) {
        size_t pos = 0;
        const TargetStrategy* strategy = TargetStrategyRegistry::find(path, pos);
        if (strategy && strategy->collectsManagedTargetsFromDirs()) strategy->appendManagedTargets(path, pos, gameTid, result);
    }

    // IPS 补丁必须从文件确认；目录扫描只负责 contents 这类可由目录本身确定目标的策略。
    for (const auto& path : sourceFiles) {
        size_t pos = 0;
        const TargetStrategy* strategy = TargetStrategyRegistry::find(path, pos);
        if (strategy && strategy->acceptsFile(path, pos)) strategy->appendManagedTargets(path, pos, gameTid, result);
    }
    return result;
}

} // namespace ModInstaller::targets
