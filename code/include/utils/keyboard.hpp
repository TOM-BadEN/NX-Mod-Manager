/**
 * keyboard - 系统键盘封装
 * 直接调用 libnx swkbd API，同步阻塞，返回用户输入
 */

#pragma once

#include <string>
#include <switch.h>

namespace keyboard {

/**
 * @brief 文本键盘，同步阻塞，返回用户输入，取消返回空字符串
 * @param header 标题
 * @param guide 占位提示
 * @param initial 预填文本
 * @param maxLen 最大字符数
 */
inline std::string showText(const std::string& header = "",
                            const std::string& guide = "",
                            const std::string& initial = "",
                            int maxLen = 32) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return "";

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_All);
    swkbdConfigSetStringLenMax(&kbd, maxLen);
    swkbdConfigSetHeaderText(&kbd, header.c_str());
    swkbdConfigSetGuideText(&kbd, guide.c_str());
    swkbdConfigSetInitialText(&kbd, initial.c_str());
    swkbdConfigSetInitialCursorPos(&kbd, 1);
    swkbdConfigSetBlurBackground(&kbd, true);

    char buf[0x100] = {0};
    Result rc = swkbdShow(&kbd, buf, sizeof(buf));
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? std::string(buf) : "";
}

/**
 * @brief 数字键盘（左下角带小数点），同步阻塞，取消返回空字符串
 * @param header 标题
 * @param initial 预填文本
 * @param maxLen 最大字符数
 */
inline std::string showNumber(const std::string& header = "",
                              const std::string& initial = "",
                              int maxLen = 10) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return "";

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetStringLenMax(&kbd, maxLen);
    swkbdConfigSetLeftOptionalSymbolKey(&kbd, ".");
    swkbdConfigSetHeaderText(&kbd, header.c_str());
    swkbdConfigSetInitialText(&kbd, initial.c_str());
    swkbdConfigSetInitialCursorPos(&kbd, 1);
    swkbdConfigSetBlurBackground(&kbd, true);

    char buf[0x100] = {0};
    Result rc = swkbdShow(&kbd, buf, sizeof(buf));
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? std::string(buf) : "";
}

} // namespace keyboard
