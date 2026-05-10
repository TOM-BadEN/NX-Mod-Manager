/**
 * DownloadRecord - 商店下载记录
 */

#include "core/downloadRecord.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include <cstdio>
#include <cstdlib>

void DownloadRecord::load() {
    m_ids.clear();
    FILE* f = fopen(config::downloadRecordPath, "r");
    if (!f) return;
    char buf[32];
    while (fgets(buf, sizeof(buf), f)) {
        int id = atoi(buf);
        if (id > 0) m_ids.insert(id);
    }
    fclose(f);
}

void DownloadRecord::save() {
    fs::ensureDir(config::modShopDir);
    FILE* f = fopen(config::downloadRecordPath, "w");
    if (!f) return;
    for (int id : m_ids) fprintf(f, "%d\n", id);
    fclose(f);
}

bool DownloadRecord::has(int modId) const {
    return m_ids.count(modId) > 0;
}

void DownloadRecord::add(int modId) {
    m_ids.insert(modId);
}

void DownloadRecord::remove(int modId) {
    m_ids.erase(modId);
}
