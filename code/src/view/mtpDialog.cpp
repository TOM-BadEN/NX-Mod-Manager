/**
 * MtpDialog - MTP 传输进度对话框实现
 */

#include "view/mtpDialog.hpp"
#include "view/customDialog.hpp"
#include "core/device.hpp"
#include "utils/format.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <chrono>
#include <memory>

namespace {

// 传输状态（仅在 brls::sync 主线程内访问，无需原子变量）
struct MtpState {
    MtpManager manager;                                          // MTP 传输管理
    int fileIndex = 0;                                           // 当前文件序号（累计，从 1 开始）
    int completedCount = 0;                                      // 已完成传输的文件总数
    bool transferring = false;                                   // 当前是否有文件正在传输
    bool stopped = false;                                        // 是否已调用 endMtp（防重入）
    size_t idleDelayId = 0;                                      // 空闲检测定时器 ID（2 秒无新传输后显示"等待中"）
    std::chrono::steady_clock::time_point lastProgressTime;      // 上次进度回调的时间戳（用于计算瞬时速度）
    std::chrono::steady_clock::time_point lastUpdateTime;        // 上次进度条刷新的时间戳（100ms 节流）
    std::chrono::steady_clock::time_point lastTextUpdateTime;    // 上次速度/ETA 文本刷新的时间戳（500ms 节流）
    long long lastTransferred = 0;                               // 上次进度回调时的已传输字节数
    double smoothedSpeed = 0;                                    // EMA 平滑后的传输速度（字节/秒）
};

} // anonymous namespace

void mtpDialog::open(std::vector<MtpMount> mounts) {
    // ── 创建进度对话框 ──
    auto* content = dynamic_cast<brls::Box*>(brls::View::createFromXMLResource("view/progressDialog.xml"));
    if (!content) return;

    auto* text         = dynamic_cast<brls::Label*>(content->getView("progress_dialog/text"));
    auto* leftText     = dynamic_cast<brls::Label*>(content->getView("progress_dialog/leftText"));
    auto* rightText    = dynamic_cast<brls::Label*>(content->getView("progress_dialog/rightText"));
    auto* mainFill     = dynamic_cast<brls::Rectangle*>(content->getView("progress_dialog/mainFill"));
    auto* percentage   = dynamic_cast<brls::Label*>(content->getView("progress_dialog/percentage"));
    auto* progressArea = dynamic_cast<brls::Box*>(content->getView("progress_dialog/progressArea"));

    // 初始状态：等待连接
    text->setText(brls::getStr("views/mtpDialog/waiting"));
    text->setMarginBottom(0);
    progressArea->setVisibility(brls::Visibility::GONE);

    // ── 共享状态 ──
    auto state = std::make_shared<MtpState>();

    // ── 结束 MTP 的统一入口（防重入） ──
    auto endMtp = [state] {
        if (state->stopped) return;
        state->stopped = true;
        deviceControl::CpuBoost::disable();
        brls::cancelDelay(state->idleDelayId);
        state->manager.stop();
        CustomDialog::close();
    };

    // ── 对话框 ──
    auto* dialog = new CustomDialog(content);
    dialog->addButton(brls::getStr("views/mtpDialog/end"), endMtp);
    dialog->onB(endMtp);
    dialog->open();

    deviceControl::CpuBoost::enableFastLoad();

    // ── 启动 MTP ──
    auto onEvent = [state, text, leftText, rightText, mainFill, percentage, progressArea, endMtp](const MtpEventData& event) {
        auto now = std::chrono::steady_clock::now();
        MtpEventData eventCopy = event;

        brls::sync([=] {
            if (state->stopped) return;

            switch (eventCopy.event) {
                case MtpEvent::Connected:
                    text->setMarginBottom(0);
                    text->setText(brls::getStr("views/mtpDialog/connected"));
                    break;

                case MtpEvent::Disconnected:
                    endMtp();
                    break;

                case MtpEvent::WriteBegin:
                case MtpEvent::ReadBegin: {
                    state->fileIndex++;
                    state->transferring = true;
                    state->lastTransferred = 0;
                    state->lastProgressTime = now;
                    state->smoothedSpeed = 0;

                    brls::cancelDelay(state->idleDelayId);
                    state->idleDelayId = 0;

                    // 提取纯文件名（去除路径）
                    std::string filename = eventCopy.filename;
                    auto slashPosition = filename.rfind('/');
                    if (slashPosition != std::string::npos) filename = filename.substr(slashPosition + 1);

                    text->setMarginBottom(45);
                    text->setText("(" + std::to_string(state->fileIndex) + ") " + filename);
                    progressArea->setVisibility(brls::Visibility::VISIBLE);
                    break;
                }

                case MtpEvent::WriteProgress:
                case MtpEvent::ReadProgress: {
                    double elapsed = std::chrono::duration<double>(now - state->lastProgressTime).count();
                    double instantSpeed = elapsed > 0.001 ? (eventCopy.transferred - state->lastTransferred) / elapsed : 0;
                    state->lastProgressTime = now;
                    state->lastTransferred = eventCopy.transferred;

                    constexpr double alpha = 0.1;
                    state->smoothedSpeed = (state->smoothedSpeed < 1.0) ? instantSpeed : alpha * instantSpeed + (1.0 - alpha) * state->smoothedSpeed;
                    double speed = state->smoothedSpeed;

                    float percent = eventCopy.fileSize > 0 ? eventCopy.transferred * 100.0f / eventCopy.fileSize : 0;

                    // 速度/ETA 文本节流：每 500ms 刷新一次
                    double timeSinceTextUpdate = std::chrono::duration<double>(now - state->lastTextUpdateTime).count();
                    if (timeSinceTextUpdate >= 0.5) {
                        state->lastTextUpdateTime = now;

                        long long remaining = eventCopy.fileSize - eventCopy.transferred;
                        int etaSeconds = speed > 0 ? static_cast<int>(remaining / speed) : 0;

                        leftText->setText(format::transferSpeed(speed) + "  " + format::fileSize(eventCopy.transferred) + " / " + format::fileSize(eventCopy.fileSize));
                        rightText->setText(format::duration(etaSeconds, "HH:MM:SS"));
                    }

                    // 进度条节流：距上次更新不足 100ms 则跳过
                    double timeSinceLastUpdate = std::chrono::duration<double>(now - state->lastUpdateTime).count();
                    if (timeSinceLastUpdate < 0.1) break;
                    state->lastUpdateTime = now;

                    char buffer[32];
                    mainFill->setVisibility(brls::Visibility::VISIBLE);
                    mainFill->setWidthPercentage(percent);
                    snprintf(buffer, sizeof(buffer), "%.1f%%", percent);
                    percentage->setText(buffer);
                    break;
                }

                case MtpEvent::WriteEnd:
                case MtpEvent::ReadEnd: {
                    state->completedCount++;
                    state->transferring = false;

                    // 文件完成，进度拉满
                    mainFill->setVisibility(brls::Visibility::VISIBLE);
                    mainFill->setWidthPercentage(100);
                    percentage->setText("100%");

                    // 2 秒后若无新传输，切到"等待中"
                    int completed = state->completedCount;
                    state->idleDelayId = brls::delay(2000, [=] {
                        if (state->stopped || state->transferring) return;
                        text->setMarginBottom(0);
                        text->setText(brls::getStr("views/mtpDialog/completed", std::to_string(completed)));
                        progressArea->setVisibility(brls::Visibility::GONE);
                    });
                    break;
                }
                default:
                    break;
            }
        });
    };

    // 启动失败（如 MTP 已在运行）则关闭对话框
    if (!state->manager.start(std::move(mounts), onEvent)) {
        endMtp();
    }
}
