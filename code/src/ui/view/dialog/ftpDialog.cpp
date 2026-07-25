/**
 * FtpDialog - FTP 传输状态对话框实现
 */

#include "ui/view/dialog/ftpDialog.hpp"
#include "core/device.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <memory>
#include <string>
#include <utility>

namespace {

// 传输状态（仅在 brls::sync 主线程内访问，无需原子变量）
struct FtpState {
    FtpManager manager;        // FTP 传输管理
    std::string ip;            // FTP 服务 IP 地址（start 后赋值）
    unsigned port = 0;         // FTP 服务端口（start 后赋值）
    int successCount = 0;      // 累计成功传输文件数
    int failedCount = 0;       // 累计失败传输文件数
    bool transferring = false; // 当前是否有文件正在传输
    bool stopped = false;      // 是否已调用 endFtp（防重入）
    size_t idleDelayId = 0;    // 空闲检测定时器 ID（2 秒无新传输后隐藏"正在传输"）
};

} // namespace

void ftpDialog::open(std::vector<FtpMount> mounts) {
    auto state = std::make_shared<FtpState>();

    // ── 结束 FTP 的统一入口（防重入） ──
    auto endFtp = [state] {
        if (state->stopped) return;
        state->stopped = true;
        deviceControl::CpuBoost::disable();
        brls::cancelDelay(state->idleDelayId);
        state->manager.stop();
        CustomDialog::close();
    };

    // ── 事件回调（FTP 子线程调用，brls::sync 投递到主线程） ──
    auto onEvent = [state](const FtpEventData& event) {
        FtpEventData eventCopy = event;

        brls::sync([=] {
            if (state->stopped) return;
            const std::string& ip = state->ip;

            switch (eventCopy.event) {
                case FtpEvent::Connected:
                    brls::cancelDelay(state->idleDelayId);
                    state->idleDelayId = 0;
                    CustomDialog::setText(brls::getStr("view/ftpDialog/connected"));
                    break;

                case FtpEvent::Disconnected:
                    brls::cancelDelay(state->idleDelayId);
                    state->idleDelayId = 0;
                    CustomDialog::setText(brls::getStr("view/ftpDialog/disconnected", ip, state->port));
                    break;

                case FtpEvent::TransferBegin: {
                    state->transferring = true;
                    brls::cancelDelay(state->idleDelayId);
                    state->idleDelayId = 0;
                    std::string text = brls::getStr("view/ftpDialog/transferring");
                    if (state->successCount > 0) text += brls::getStr("view/ftpDialog/successCount", std::to_string(state->successCount));
                    if (state->failedCount > 0) text += brls::getStr("view/ftpDialog/failedCount", std::to_string(state->failedCount));
                    CustomDialog::setText(text);
                    break;
                }

                case FtpEvent::TransferEnd: {
                    state->transferring = false;
                    state->successCount = eventCopy.successCount;
                    state->failedCount = eventCopy.failedCount;
                    std::string text = brls::getStr("view/ftpDialog/transferring") + brls::getStr("view/ftpDialog/successCount", std::to_string(state->successCount));
                    if (state->failedCount > 0) text += brls::getStr("view/ftpDialog/failedCount", std::to_string(state->failedCount));
                    CustomDialog::setText(text);

                    // 2 秒后若无新传输，隐藏 "正在传输文件..."
                    int success = state->successCount;
                    int failed = state->failedCount;
                    state->idleDelayId = brls::delay(2000, [=] {
                        if (state->stopped || state->transferring) return;
                        std::string idleText = brls::getStr("view/ftpDialog/transferDone");
                        idleText += brls::getStr("view/ftpDialog/successCount", std::to_string(success));
                        if (failed > 0) idleText += brls::getStr("view/ftpDialog/failedCount", std::to_string(failed));
                        CustomDialog::setText(idleText);
                    });
                    break;
                }

                case FtpEvent::TransferFailed: {
                    state->transferring = false;
                    state->successCount = eventCopy.successCount;
                    state->failedCount = eventCopy.failedCount;
                    std::string text = brls::getStr("view/ftpDialog/transferring");
                    if (state->successCount > 0) text += brls::getStr("view/ftpDialog/successCount", std::to_string(state->successCount));
                    text += brls::getStr("view/ftpDialog/failedCount", std::to_string(state->failedCount));
                    CustomDialog::setText(text);

                    // 2 秒后若无新传输，隐藏 "正在传输文件..."
                    int success = state->successCount;
                    int failed = state->failedCount;
                    state->idleDelayId = brls::delay(2000, [=] {
                        if (state->stopped || state->transferring) return;
                        std::string idleText = brls::getStr("view/ftpDialog/transferDone");
                        if (success > 0) idleText += brls::getStr("view/ftpDialog/successCount", std::to_string(success));
                        if (failed > 0) idleText += brls::getStr("view/ftpDialog/failedCount", std::to_string(failed));
                        CustomDialog::setText(idleText);
                    });
                    break;
                }
            }
        });
    };

    // ── 启动 FTP ──
    deviceControl::CpuBoost::enableFastLoad();
    auto result = state->manager.start({.mounts = std::move(mounts)}, onEvent);
    if (!result.success) {
        deviceControl::CpuBoost::disable();
        CustomDialog::show(result.errorMessage, {{brls::getStr("view/ftpDialog/acknowledged"), [] { CustomDialog::close(); }}});
        return;
    }
    state->ip = result.ipAddress;
    state->port = result.port;

    // ── 对话框 ──
    CustomDialog::show(brls::getStr("view/ftpDialog/started", result.ipAddress, result.port), {{brls::getStr("view/ftpDialog/end"), endFtp}}, endFtp);
}
