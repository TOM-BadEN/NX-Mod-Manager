#pragma once

#include <borealis/views/dialog.hpp>
#include <functional>
#include <string>

namespace dialog {

    inline void showMessage(const std::string& msg) {
        auto* dialog = new brls::Dialog(msg);
        dialog->addButton("取消", []{});
        dialog->addButton("确定", []{});
        dialog->open();
    }

    inline void showConfirm(const std::string& msg, std::function<void()> cb) {
        auto* dialog = new brls::Dialog(msg);
        dialog->addButton("取消", []{});
        dialog->addButton("确定", [cb]{ cb(); });
        dialog->open();
    }

} // namespace dialog
