/**
 * StoreModDetail - 商店模组详情页面实现
 */

#include "activity/storeModDetail.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/commentListDS.hpp"
#include "view/commentCard.hpp"
#include "view/customDialog.hpp"
#include "view/progressDialog.hpp"
#include "view/imageViewer.hpp"
#include "common/modInfo.hpp"
#include "utils/format.hpp"
#include "utils/webpDecoder.hpp"
#include "core/audio.hpp"
#include "common/settings.hpp"
#include "utils/keyboard.hpp"
#include "common/config.hpp"
#include <borealis/core/cache_helper.hpp>
#include <yoga/Yoga.h>
#include <chrono>

StoreModDetail::StoreModDetail(int modId, std::string gameTid, std::string gameName, GameManager& gameManager, ReturnCallback onReturn, ModManager* localModManager)
    : m_manager(modId, std::move(gameTid)), m_gameName(std::move(gameName)), m_gameManager(gameManager), m_onReturn(std::move(onReturn)), m_localModManager(localModManager) {}

StoreModDetail::~StoreModDetail() {
    m_alive->store(false);

    if (m_onReturn) {
        auto& d = m_manager.getDetail();
        m_onReturn(d.likeCount, d.dislikeCount, d.downloadCount, d.isInstalled);
    }

    // 提前对所有异步任务发出停止信号，让 HTTP 请求并行中断
    m_detailTask.request_stop();
    m_screenshotTask.request_stop();
    m_commentTask.request_stop();
    m_voteTask.request_stop();
    m_downloadTask.request_stop();

    // 释放截图纹理
    auto& screenshots = m_manager.getScreenshots();
    NVGcontext* vg = brls::Application::getNVGContext();
    if (screenshots.texture1 > 0) nvgDeleteImage(vg, screenshots.texture1);
    if (screenshots.texture2 > 0) nvgDeleteImage(vg, screenshots.texture2);
}

void StoreModDetail::onContentAvailable() {
    m_frame->setTitle(m_gameName);
    setupLeftCard();
    setupScreenshots();
    setupButtons();
    setupCommentGrid();
    loadDetail();
    loadScreenshots();
    loadNextCommentPage();
}

void StoreModDetail::setupLeftCard() {
    // 标签区域启用自动换行
    YGNodeStyleSetFlexWrap(m_tagRow->getYGNode(), YGWrapWrap);

    // SVG 图标根据主题选择颜色
    bool isDarkTheme = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
    std::string suffix = isDarkTheme ? "-dark.svg" : "-light.svg";
    m_iconLike->setImageFromSVGRes("img/mod/like" + suffix);
    m_iconDislike->setImageFromSVGRes("img/mod/dislike" + suffix);
    m_iconDownload->setImageFromSVGRes("img/mod/download" + suffix);
    m_iconTime->setImageFromSVGRes("img/mod/time" + suffix);

    // 描述区滚动条隐藏
    m_scroll->setScrollingIndicatorVisible(false);

    // 右键：左卡片 → 截图（优先截图1 → 截图2）或按钮
    m_leftCard->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        if (m_screenshot1->isFocusable()) brls::Application::giveFocus(m_screenshot1);
        else if (m_screenshot2->isFocusable()) brls::Application::giveFocus(m_screenshot2);
        else brls::Application::giveFocus(m_buttonLike);
        return true;
    }, true);

    // 左键：左卡片边界抖动
    m_leftCard->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        m_leftCard->shakeHighlight(brls::FocusDirection::LEFT);
        return true;
    }, true);

    // 上下键：驱动描述区滚动
    m_leftCard->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur + 60.0f, true);
        return true;
    }, true, true);
    m_leftCard->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur - 60.0f, true);
        return true;
    }, true, true);
}

void StoreModDetail::setupScreenshots() {
    brls::Image* screenshots[] = { m_screenshot1, m_screenshot2 };

    // 左右键：共享 lambda
    auto screenshotRight = [this](...) {
        if (m_commentGrid->getVisibility() == brls::Visibility::VISIBLE) {
            Audio::instance()->play(SoundEffect::Focus);
            brls::Application::giveFocus(m_commentGrid);
        } else {
            Audio::instance()->play(SoundEffect::FocusLimit);
            brls::Application::getCurrentFocus()->shakeHighlight(brls::FocusDirection::RIGHT);
        }
        return true;
    };
    auto screenshotLeft = [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_leftCard);
        return true;
    };

    for (int i = 0; i < 2; i++) {
        screenshots[i]->registerAction("", brls::BUTTON_NAV_RIGHT, screenshotRight, true);
        screenshots[i]->registerAction("", brls::BUTTON_NAV_LEFT, screenshotLeft, true);

        // 点击截图：打开大图浏览（构建纹理列表，支持左右切换）
        screenshots[i]->registerAction("", brls::BUTTON_A, [this, i](...) {
            auto& ss = m_manager.getScreenshots();
            std::vector<int> textureIds;
            if (ss.texture1 > 0) textureIds.push_back(ss.texture1);
            if (ss.texture2 > 0) textureIds.push_back(ss.texture2);
            if (textureIds.empty()) return true;
            int startIdx = (i == 0) ? 0 : (int)textureIds.size() - 1;
            Audio::instance()->play(SoundEffect::Enter);
            ImageViewer::open(textureIds, startIdx);
            return true;
        }, true);

        // 触摸点击截图
        screenshots[i]->addGestureRecognizer(new brls::TapGestureRecognizer(screenshots[i]));
    }

    // 下键：截图1 → 截图2（可聚焦时）或按钮，截图2 → 按钮
    m_screenshot1->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_screenshot2->isFocusable() ? (brls::View*)m_screenshot2 : m_buttonLike);
        return true;
    }, true);
    m_screenshot2->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_buttonLike);
        return true;
    }, true);
}

void StoreModDetail::setupButtons() {
    CircleButton* buttons[] = { m_buttonLike, m_buttonDislike, m_buttonDownload, m_buttonComment };
    constexpr int count = 4;

    // 上键：跳到截图（优先截图2 → 截图1 → 抖动）
    auto buttonUp = [this](...) {
        if (m_screenshot2->isFocusable()) {
            Audio::instance()->play(SoundEffect::Focus);
            brls::Application::giveFocus(m_screenshot2);
        } else if (m_screenshot1->isFocusable()) {
            Audio::instance()->play(SoundEffect::Focus);
            brls::Application::giveFocus(m_screenshot1);
        } else {
            Audio::instance()->play(SoundEffect::FocusLimit);
            brls::Application::getCurrentFocus()->shakeHighlight(brls::FocusDirection::UP);
        }
        return true;
    };

    // 下键：边界抖动
    auto buttonDown = [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        brls::Application::getCurrentFocus()->shakeHighlight(brls::FocusDirection::DOWN);
        return true;
    };

    m_buttonLike->registerAction(brls::getStr("storeModDetail/btnLike"), brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onLikeAction();
        return true;
    });
    m_buttonDislike->registerAction(brls::getStr("storeModDetail/btnDislike"), brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onDislikeAction();
        return true;
    });
    m_buttonDownload->registerAction(brls::getStr("storeModDetail/btnDownload"), brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onDownloadAction();
        return true;
    });
    m_buttonComment->registerAction(brls::getStr("storeModDetail/btnComment"), brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onCommentAction();
        return true;
    });

    for (int i = 0; i < count; i++) {
        buttons[i]->registerAction("", brls::BUTTON_NAV_UP, buttonUp, true);
        buttons[i]->registerAction("", brls::BUTTON_NAV_DOWN, buttonDown, true);

        // 左右链式导航
        if (i > 0) {
            auto* prev = buttons[i - 1];
            buttons[i]->registerAction("", brls::BUTTON_NAV_LEFT, [prev](...) {
                Audio::instance()->play(SoundEffect::Focus);
                brls::Application::giveFocus(prev);
                return true;
            }, true);
        }
        if (i < count - 1) {
            auto* next = buttons[i + 1];
            buttons[i]->registerAction("", brls::BUTTON_NAV_RIGHT, [next](...) {
                Audio::instance()->play(SoundEffect::Focus);
                brls::Application::giveFocus(next);
                return true;
            }, true);
        }
    }

    // 覆盖边界：最左按钮左键 → 左卡片
    m_buttonLike->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_leftCard);
        return true;
    }, true);

    // 覆盖边界：最右按钮右键 → 留言列表（可见时）或抖动
    m_buttonComment->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        if (m_commentGrid->getVisibility() == brls::Visibility::VISIBLE) {
            Audio::instance()->play(SoundEffect::Focus);
            brls::Application::giveFocus(m_commentGrid);
        } else {
            Audio::instance()->play(SoundEffect::FocusLimit);
            brls::Application::getCurrentFocus()->shakeHighlight(brls::FocusDirection::RIGHT);
        }
        return true;
    }, true);
}

void StoreModDetail::setupCommentGrid() {
    m_commentGrid->registerCell("CommentCard", CommentCard::create);
    m_commentGrid->setScrollingIndicatorVisible(false);

    // 分页加载
    m_commentGrid->onNextPage([this] {
        if (!m_commentLoading && m_manager.hasMoreComments()) {
            loadNextCommentPage();
        }
    });

    // 左键：留言列表 → 截图（优先截图1 → 截图2）或按钮
    m_commentGrid->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        if (m_screenshot1->isFocusable()) brls::Application::giveFocus(m_screenshot1);
        else if (m_screenshot2->isFocusable()) brls::Application::giveFocus(m_screenshot2);
        else brls::Application::giveFocus(m_buttonDownload);
        return true;
    }, true);

    // 右键：留言列表边界抖动
    m_commentGrid->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        brls::Application::getCurrentFocus()->shakeHighlight(brls::FocusDirection::RIGHT);
        return true;
    }, true);
}

void StoreModDetail::loadDetail() {
    m_detailTask = util::async([this](std::stop_token token) {
        auto result = m_manager.fetchDetail(token);
        if (token.stop_requested()) return;
        auto alive = m_alive;
        brls::sync([this, alive, result = std::move(result)]() mutable {
            if (!alive->load()) return;
            if (!result.success) {
                m_leftLoading->setText(brls::getStr("storeModDetail/loadFailed"));
                auto goBack = [this] {
                    CustomDialog::close();
                    auto alive = m_alive;
                    brls::sync([alive] {
                        if (!alive->load()) return;
                        brls::Application::popActivity();
                    });
                };
                CustomDialog::show(result.error, {{brls::getStr("storeModDetail/ok"), goBack}}, goBack);
                return;
            }
            m_manager.setDetail(std::move(result.detail));
            m_leftContent->setVisibility(brls::Visibility::VISIBLE);
            m_leftLoading->setVisibility(brls::Visibility::GONE);
            m_detailLoaded = true;
            updateDetail();
            brls::Application::giveFocus(m_leftCard);
        });
    });
}

void StoreModDetail::loadScreenshots() {
    m_screenshotTask = util::async([this](std::stop_token token) {
        // 并行下载两张截图
        auto future1 = std::async(std::launch::async, [this, &token] {
            return m_manager.fetchScreenshot(1, token);
        });
        auto future2 = std::async(std::launch::async, [this, &token] {
            return m_manager.fetchScreenshot(2, token);
        });

        auto response1 = future1.get();
        auto response2 = future2.get();
        if (token.stop_requested()) return;

        auto body1 = std::move(response1.body);
        auto body2 = std::move(response2.body);
        bool ok1 = response1.ok() && !body1.empty();
        bool ok2 = response2.ok() && !body2.empty();

        auto alive = m_alive;
        brls::sync([this, alive, body1 = std::move(body1), body2 = std::move(body2), ok1, ok2] {
            if (!alive->load()) return;

            auto* vg = brls::Application::getNVGContext();
            auto& screenshots = m_manager.getScreenshots();

            if (ok1) {
                auto img = webpDecoder::decode(body1.data(), body1.size());
                if (img.width > 0) screenshots.texture1 = nvgCreateImageRGBA(vg, img.width, img.height, 0, img.pixels.data());
            }
            if (ok2) {
                auto img = webpDecoder::decode(body2.data(), body2.size());
                if (img.width > 0) screenshots.texture2 = nvgCreateImageRGBA(vg, img.width, img.height, 0, img.pixels.data());
            }

            // 成功：替换为真实截图；失败：替换为 noImage 占位图
            if (screenshots.texture1 > 0) {
                m_screenshot1->setFreeTexture(false);
                m_screenshot1->innerSetImage(screenshots.texture1);
                m_screenshot1->setFocusable(true);
            } else {
                m_screenshot1->setImageFromRes("img/mod/noImage.png");
            }
            if (screenshots.texture2 > 0) {
                m_screenshot2->setFreeTexture(false);
                m_screenshot2->innerSetImage(screenshots.texture2);
                m_screenshot2->setFocusable(true);
            } else {
                m_screenshot2->setImageFromRes("img/mod/noImage.png");
            }
        });
    });
}

void StoreModDetail::loadNextCommentPage() {
    m_commentLoading = true;
    m_commentTask = util::async([this](std::stop_token token) {
        auto result = m_manager.fetchNextCommentPage(token);
        if (token.stop_requested()) return;
        auto alive = m_alive;
        brls::sync([this, alive, result = std::move(result)]() mutable {
            if (!alive->load()) return;
            m_commentLoading = false;

            if (!result.success) {
                m_rightHintLabel->setText(brls::getStr("storeModDetail/loadFailed"));
                return;
            }

            m_manager.appendCommentPage(std::move(result));

            if (m_manager.getComments().empty()) {
                m_rightHintLabel->setText(brls::getStr("storeModDetail/noComments"));
                return;
            }

            m_rightHintCard->setVisibility(brls::Visibility::GONE);
            m_commentGrid->setVisibility(brls::Visibility::VISIBLE);

            if (!m_commentGrid->getDataSource()) {
                m_commentGrid->setDataSource(new CommentListDS(m_manager.getComments()));
            } else {
                m_commentGrid->notifyDataChanged();
            }
        });
    });
}

void StoreModDetail::updateDetail() {
    auto& detail = m_manager.getDetail();

    // 图标和名称
    m_modIcon->setImageFromRes(modTypeIcon(detail.modType));
    m_modName->setText(detail.modName);
    m_installedIcon->setVisibility(detail.isInstalled ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);

    // 统计数据
    m_statDownload->setText(std::to_string(detail.downloadCount));
    m_statTime->setText(format::timeAgo(detail.uploadTime));

    // 胶囊标签
    m_tagType->setText(modTypeText(detail.modType));
    m_tagAuthor->setText(detail.author.empty() ? brls::getStr("storeModDetail/unknownAuthor") : brls::getStr("storeModDetail/authorPrefix", detail.author));
    m_tagSize->setText(detail.fileSize > 0 ? format::fileSize(detail.fileSize) : brls::getStr("storeModDetail/sizeUnknown"));
    m_tagVersion->setText(detail.modVersion.empty() ? brls::getStr("storeModDetail/modVersionUnknown") : brls::getStr("storeModDetail/modVersionFmt", detail.modVersion));
    if (detail.gameVersion.empty()) m_tagGameVersion->setText(brls::getStr("storeModDetail/gameVersionUnknown"));
    else if (detail.gameVersion == "0") m_tagGameVersion->setText(brls::getStr("storeModDetail/gameVersionUniversal"));
    else m_tagGameVersion->setText(brls::getStr("storeModDetail/gameVersionFmt", detail.gameVersion));

    // 描述（左卡片）
    m_descBody->setText(detail.description.empty() ? brls::getStr("storeModDetail/noDescription") : detail.description);

    // 根据赞踩状态刷新 SVG 图标和数字
    updateVoteIcons();
}

void StoreModDetail::updateVoteIcons() {
    auto& detail = m_manager.getDetail();
    bool isDarkTheme = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
    std::string suffix = isDarkTheme ? "-dark.svg" : "-light.svg";

    // 左卡片图标 + 圆形按钮图标
    std::string likeRes = detail.deviceLiked ? "img/mod/like-done.svg" : "img/mod/like" + suffix;
    m_iconLike->setImageFromSVGRes(likeRes);
    m_buttonLike->getIcon()->setImageFromSVGRes(likeRes);

    std::string dislikeRes = detail.deviceDisliked ? "img/mod/dislike-done.svg" : "img/mod/dislike" + suffix;
    m_iconDislike->setImageFromSVGRes(dislikeRes);
    m_buttonDislike->getIcon()->setImageFromSVGRes(dislikeRes);

    // 数字
    m_statLike->setText(std::to_string(detail.likeCount));
    m_statDislike->setText(std::to_string(detail.dislikeCount));
}

void StoreModDetail::onLikeAction() {
    if (m_voteTask.valid() && m_voteTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        CustomDialog::show(brls::getStr("storeModDetail/tooFast"), {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
        return;
    }

    // 乐观更新：先翻转本地状态 + 刷新 UI
    m_manager.applyLikeToggle();
    updateVoteIcons();

    // 异步请求服务器
    m_voteTask = util::async([this](std::stop_token token) {
        auto result = m_manager.toggleLike(token);
        if (token.stop_requested()) return;
        if (!result.success) {
            auto alive = m_alive;
            brls::sync([this, alive, error = std::move(result.error)] {
                if (!alive->load()) return;
                m_manager.applyLikeToggle();
                updateVoteIcons();
                CustomDialog::show(error, {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            });
        }
    });
}

void StoreModDetail::onDislikeAction() {
    if (m_voteTask.valid() && m_voteTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        CustomDialog::show(brls::getStr("storeModDetail/tooFast"), {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
        return;
    }

    // 乐观更新：先翻转本地状态 + 刷新 UI
    m_manager.applyDislikeToggle();
    updateVoteIcons();

    // 异步请求服务器
    m_voteTask = util::async([this](std::stop_token token) {
        auto result = m_manager.toggleDislike(token);
        if (token.stop_requested()) return;
        if (!result.success) {
            auto alive = m_alive;
            brls::sync([this, alive, error = std::move(result.error)] {
                if (!alive->load()) return;
                m_manager.applyDislikeToggle();
                updateVoteIcons();
                CustomDialog::show(error, {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            });
        }
    });
}

void StoreModDetail::onDownloadAction() {
    auto& detail = m_manager.getDetail();

    if (detail.isInstalled) {
        CustomDialog::show(brls::getStr("storeModDetail/noRepeatInstall"), {{brls::getStr("storeModDetail/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto onConfirm = [this] {
        auto& detail = m_manager.getDetail();
        auto stopped = std::make_shared<std::atomic<bool>>(false);
        auto onCancel = [this, stopped] {
            stopped->store(true);
            m_downloadTask.request_stop();
            ProgressDialog::close();
        };

        detail.downloadCount++;
        m_statDownload->setText(std::to_string(detail.downloadCount));

        ProgressDialog::show(brls::getStr("storeModDetail/downloading", detail.modName), {{brls::getStr("storeModDetail/cancel"), onCancel}}, onCancel);
        ProgressDialog::setLeftText(brls::getStr("storeModDetail/calculating"));
        ProgressDialog::setRightText("--:--");

        int modId = detail.modId;
        std::string gameTid = detail.gameTid;
        std::string gameName = detail.gameName;
        std::string modName = detail.modName;

        m_downloadTask = util::async([this, modId, gameTid, gameName, modName, stopped](std::stop_token token) {
            // ── 阶段一：获取下载信息 ──
            auto info = api::mod::fetchDownloadInfo(modId, token);
            if (token.stop_requested()) return;

            if (!info.success) {
                auto alive = m_alive;
                brls::sync([alive, stopped, error = std::move(info.error)] {
                    if (!alive->load() || stopped->load()) return;
                    CustomDialog::show(error, {{brls::getStr("storeModDetail/ok"), [] { CustomDialog::close(); }}});
                });
                return;
            }

            // ── 阶段二：计算文件名 ──
            std::string modDirName = format::safeDirName(info.modNameEn);
            if (modDirName.empty()) modDirName = "mod_" + std::to_string(modId);
            std::string tempPath = std::string(config::modShopDir) + "temp/" + modDirName + ".zip";

            // ── 阶段三：下载 ──
            using Clock = std::chrono::steady_clock;
            auto lastProgressTime = Clock::now();
            auto lastBarTime = Clock::now();
            auto lastTextTime = Clock::now();
            long long lastBytes = 0;
            double smoothedSpeed = 0;

            auto onProgress = [&, stopped](size_t total, size_t now) -> bool {
                auto t = Clock::now();
                bool textUpdate = std::chrono::duration<double>(t - lastTextTime).count() >= 1.0;
                bool barUpdate = std::chrono::duration<double>(t - lastBarTime).count() >= 0.1;

                if (textUpdate) {
                    double dt = std::chrono::duration<double>(t - lastProgressTime).count();
                    double instant = dt > 0.01 ? (now - lastBytes) / dt : 0;
                    constexpr double alpha = 0.3;
                    smoothedSpeed = (smoothedSpeed < 1.0) ? instant : alpha * instant + (1.0 - alpha) * smoothedSpeed;
                    lastBytes = static_cast<long long>(now);
                    lastProgressTime = t;
                }

                if (textUpdate || barUpdate) {
                    double speed = smoothedSpeed;
                    float pct = total > 0 ? now * 100.0f / total : 0;
                    long long remaining = total > now ? total - now : 0;
                    int etaSeconds = speed > 0 ? static_cast<int>(remaining / speed) : 0;
                    size_t nowCopy = now;
                    size_t totalCopy = total;

                    brls::sync([=] {
                        if (stopped->load()) return;
                        if (barUpdate) ProgressDialog::setMainProgress(pct);
                        if (textUpdate) {
                            ProgressDialog::setLeftText(format::transferSpeed(speed) + "  " + format::fileSize(nowCopy) + " / " + format::fileSize(totalCopy));
                            ProgressDialog::setRightText(format::duration(etaSeconds, "HH:MM:SS"));
                        }
                    });

                    if (textUpdate) lastTextTime = t;
                    if (barUpdate) lastBarTime = t;
                }
                return true;
            };

            auto resp = http::downloadToFile(api::url::mod::download(modId), tempPath, onProgress, token);

            if (!resp.ok()) {
                auto alive = m_alive;
                long code = resp.code;
                brls::sync([alive, stopped, code] {
                    if (!alive->load() || stopped->load()) return;
                    std::string msg;
                    if (code == 429) msg = brls::getStr("storeModDetail/downloadRateLimit");
                    else msg = brls::getStr("storeModDetail/downloadFailed");
                    CustomDialog::show(msg, {{brls::getStr("storeModDetail/ok"), [] { CustomDialog::close(); }}});
                });
                return;
            }

            // ── 阶段四：安装 ──
            auto alive = m_alive;
            std::string gameNameEn = info.gameNameEn;
            brls::sync([this, alive, stopped, gameTid, gameNameEn, gameName, modDirName, tempPath, modName] {
                if (!alive->load() || stopped->load()) return;
                ProgressDialog::hideButtons();

                std::string gameDir = m_gameManager.addGameFromStore(gameTid, gameNameEn, gameName);
                ModInfo modInfo = m_manager.installMod(gameDir, modDirName, tempPath);

                uint64_t appId = format::appIdFromHex(gameTid);
                int idx = m_gameManager.findByAppId(appId);
                if (idx >= 0) {
                    m_gameManager.openNacp();
                    auto meta = m_gameManager.fetchMetadata(idx);
                    m_gameManager.closeNacp();

                    if (!meta.name.empty()) {
                        m_gameManager.setGameName(idx, meta.name);
                        m_gameManager.setVersion(idx, meta.version);
                        auto* vg = brls::Application::getNVGContext();
                        int iconId = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(meta.icon.data()), meta.icon.size());
                        if (iconId > 0) {
                            m_gameManager.games()[idx].iconId = iconId;
                            auto& tc = brls::TextureCache::instance();
                            if (tc.getCache(gameTid) == 0) tc.addCache(gameTid, iconId);
                        }
                    }
                }

                m_gameManager.setPendingFocus(appId);

                if (m_localModManager) {
                    m_localModManager->setPendingFocus(modInfo.modID);
                    m_localModManager->addModFromStore(std::move(modInfo));
                }

                m_installedIcon->setVisibility(brls::Visibility::VISIBLE);

                CustomDialog::show(brls::getStr("storeModDetail/downloadComplete", modName), {{brls::getStr("storeModDetail/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("storeModDetail/confirmDownload"), {
        {brls::getStr("storeModDetail/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("storeModDetail/confirm"), [onConfirm] { CustomDialog::close(onConfirm); }},
    });
}

void StoreModDetail::onCommentAction() {
    // 获取昵称
    std::string nickname = Settings::getString("modShop", "nickname", brls::getStr("storeModDetail/anonymousUser"));
    // 弹出键盘输入留言内容（限制 200 字符）
    std::string content = keyboard::showText(brls::getStr("storeModDetail/inputComment"), brls::getStr("storeModDetail/inputComment"), "", 200);
    if (content.empty()) return;

    // 异步提交
    m_commentTask = util::async([this, nickname, content](std::stop_token token) {
        auto result = m_manager.submitComment(nickname, content, token);
        if (token.stop_requested()) return;
        auto alive = m_alive;
        brls::sync([this, alive, result = std::move(result)] {
            if (!alive->load()) return;
            if (result.success) {
                Audio::instance()->play(SoundEffect::Enter);
                CustomDialog::show(brls::getStr("storeModDetail/commentSuccess"), {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            } else {
                Audio::instance()->play(SoundEffect::Enter);
                CustomDialog::show(result.error, {{brls::getStr("storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            }
        });
    });
}
