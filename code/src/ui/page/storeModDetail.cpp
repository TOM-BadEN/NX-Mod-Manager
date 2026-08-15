/**
 * StoreModDetail - 商店模组详情页面实现
 */

#include "ui/page/storeModDetail.hpp"
#include <borealis/core/i18n.hpp>
#include "ui/dataSource/commentListDS.hpp"
#include "ui/view/commentCard.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "ui/view/dialog/progressDialog.hpp"
#include "ui/view/imageViewer.hpp"
#include "ui/view/qrCodeView.hpp"
#include "common/modInfo.hpp"
#include "utils/format.hpp"
#include "utils/textClean.hpp"
#include "utils/threadPool.hpp"
#include "core/audio.hpp"
#include "core/device.hpp"
#include "core/frameQueue.hpp"
#include "common/settings.hpp"
#include "ui/navigation/navigationGroups.hpp"
#include "utils/keyboard.hpp"
#include "common/config.hpp"
#include "ui/core/pageHost.hpp"
#include "ui/page/modList.hpp"
#include "ui/page/storeModList.hpp"
#include <borealis/core/cache_helper.hpp>
#include <yoga/Yoga.h>
#include <chrono>

StoreModDetail::StoreModDetail(int modId, std::string gameTid, std::string gameName, std::string gameIconKey, GameManager& gameManager, ReturnCallback onReturn, ModManager* localModManager, std::string gameVersion, bool fromModList, bool directFromModList)
    : m_manager(modId, std::move(gameTid)), m_gameName(std::move(gameName)), m_gameIconKey(std::move(gameIconKey)), m_gameManager(gameManager), m_onReturn(std::move(onReturn)), m_localModManager(localModManager), m_gameVersion(std::move(gameVersion)), m_fromModList(fromModList), m_directFromModList(directFromModList) {
    inflateFromXMLRes("xml/view/page/storeModDetail.xml");

    m_focusChangedSubscription = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* focusedView) {
        for (auto* view = focusedView; view; view = view->getParent()) {
            if (view == m_commentGrid) return;
        }
        ShellState::setIndexText("");
    });

    setHeader();
}

StoreModDetail::~StoreModDetail() {
    brls::Application::getGlobalFocusChangeEvent()->unsubscribe(m_focusChangedSubscription);
    m_stopSource.request_stop();
    m_downloadStop.request_stop();

    if (m_onReturn && m_detailLoaded) {
        auto& d = m_manager.getDetail();
        m_onReturn(d.likeCount, d.dislikeCount, d.downloadCount, d.downloaded);
    }
}

void StoreModDetail::setHeader() {
    TitleState titleState;
    titleState.iconTextureKey = m_gameIconKey;
    titleState.iconPath = config::defaultGameIconResource;
    titleState.title = m_gameName;
    titleState.subtitle = brls::getStr("page/storeModDetail/notInstalled");
    if (m_gameVersion != "...") titleState.subtitle = brls::getStr("page/storeModDetail/localVersion", format::cleanVersion(m_gameVersion));

    HeaderState headerState;
    if (m_directFromModList) headerState.setNavigation(createModNavigationState(ModNavigationPage::StoreModDetail));
    headerState.setTitle(titleState);
    ShellState::setHeaderState(headerState);
}

void StoreModDetail::setupNavigationActions() {
    if (!m_directFromModList) return;

    // ZL：直接返回 ModList
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        popFromDetail(1);
        return true;
    }, true);

    // ZR：导航右边界
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        shakeHeaderNav(true);
        return true;
    }, true);
}

void StoreModDetail::onContentAvailable() {
    setupLeftCard();
    setupScreenshots();
    setupButtons();
    setupCommentGrid();
    // B 键：返回
    registerAction("", brls::BUTTON_B, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        popFromDetail(1);
        return true;
    }, true);

    setupNavigationActions();

    // Y 键：更新记录
    registerAction(brls::getStr("page/storeModDetail/changelogBtn"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        showChangelogDialog();
        return true;
    });
    setActionAvailable(brls::BUTTON_Y, false);
    // X 键：查看作者
    registerAction(brls::getStr("page/storeModDetail/viewAuthor"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        auto& authorLink = m_manager.getDetail().authorLink;
        if (!authorLink.empty()) QrCodeView::show(authorLink);
        return true;
    });
    setActionAvailable(brls::BUTTON_X, false);
    // + 键：上传模组
    registerAction(brls::getStr("page/storeModDetail/uploadMod"), brls::BUTTON_START, [](...) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::show(config::websiteUrl, config::websiteUrl);
        return true;
    });

    loadDetail();
    loadScreenshots();
    loadNextCommentPage();

    m_layoutReady = true;
}

void StoreModDetail::onLayout() {
    Page::onLayout();
    if (m_layoutReady) updateScrollHintVisibility();
}

void StoreModDetail::popFromDetail(int popCount) {
    auto* pageHost = dynamic_cast<PageHost*>(getParent());
    auto* previousPage = pageHost->getPreviousPage();
    auto anim = PageAnimType::SlideFromRight;
    if (popCount == 1 && dynamic_cast<StoreModList*>(previousPage)) anim = PageAnimType::None;
    else if (dynamic_cast<ModList*>(previousPage)) anim = PageAnimType::SlideFromLeft;
    Page::popPages(popCount, anim);
}

void StoreModDetail::setupLeftCard() {
    // 标签区域启用自动换行
    YGNodeStyleSetFlexWrap(m_tagRow->getYGNode(), YGWrapWrap);

    m_iconLike->setImageFromRes(format::themedIconPath("img/mod/like"));
    m_iconDislike->setImageFromRes(format::themedIconPath("img/mod/dislike"));
    m_iconDownload->setImageFromRes(format::themedIconPath("img/mod/download"));
    m_iconTime->setImageFromRes(format::themedIconPath("img/mod/time"));

    // 焦点进入时显示描述区滚动条，离开时显示底部提示
    m_scroll->setScrollingIndicatorVisible(false);
    m_scrollHint->setVisibility(brls::Visibility::INVISIBLE);
    m_leftCard->getFocusEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(true);
        updateScrollHintVisibility();
    });
    m_leftCard->getFocusLostEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(false);
        updateScrollHintVisibility();
    });

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

        // 点击截图：打开大图浏览（构建纹理和来源控件列表，支持左右切换）
        screenshots[i]->registerAction("", brls::BUTTON_A, [this, i](...) {
            std::vector<int> textureIds;
            std::vector<brls::Image*> sourceImages;
            if (m_screenshot1->isFocusable()) {
                textureIds.push_back(m_screenshot1->getTexture());
                sourceImages.push_back(m_screenshot1);
            }
            if (m_screenshot2->isFocusable()) {
                textureIds.push_back(m_screenshot2->getTexture());
                sourceImages.push_back(m_screenshot2);
            }
            if (textureIds.empty()) return true;
            int startIdx = (i == 0) ? 0 : (int)textureIds.size() - 1;
            Audio::instance()->play(SoundEffect::Enter);
            ImageViewer::open(textureIds, sourceImages, startIdx);
            return true;
        }, true);

        // 触摸点击截图
        screenshots[i]->addGestureRecognizer(new brls::TapGestureRecognizer(screenshots[i]));
    }

    // 上键：截图1边界抖动，截图2 → 截图1（可聚焦时）或边界抖动
    m_screenshot1->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        m_screenshot1->shakeHighlight(brls::FocusDirection::UP);
        return true;
    }, true);
    m_screenshot2->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        if (m_screenshot1->isFocusable()) {
            Audio::instance()->play(SoundEffect::Focus);
            brls::Application::giveFocus(m_screenshot1);
        } else {
            Audio::instance()->play(SoundEffect::FocusLimit);
            m_screenshot2->shakeHighlight(brls::FocusDirection::UP);
        }
        return true;
    }, true);

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

    m_buttonLike->registerAction("", brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onLikeAction();
        return true;
    }, true);
    m_buttonDislike->registerAction("", brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onDislikeAction();
        return true;
    }, true);
    m_buttonDownload->registerAction("", brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onDownloadAction();
        return true;
    }, true);
    m_buttonComment->registerAction("", brls::BUTTON_A, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_detailLoaded) onCommentAction();
        return true;
    }, true);

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
    m_commentGrid->setPadding(5, 0, 5, 0);
    m_commentGrid->registerCell("CommentCard", CommentCard::create);
    m_commentGrid->setScrollingIndicatorVisible(false);
    m_commentGrid->setFocusChangeCallback([this](size_t index) {
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.commentTotal()));
    });

    // 分页加载
    m_commentGrid->onNextPage([this] {
        if (!m_commentLoading && m_manager.hasMoreComments()) {
            loadNextCommentPage();
        }
    });

    // 左键：仅恢复截图或留言按钮，否则按截图1、截图2、留言按钮回退
    m_commentGrid->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        auto* target = m_middleCard->getDefaultFocus();
        if (target == m_screenshot1 || target == m_screenshot2 || target == m_buttonComment) brls::Application::giveFocus(target);
        else if (m_screenshot1->isFocusable()) brls::Application::giveFocus(m_screenshot1);
        else if (m_screenshot2->isFocusable()) brls::Application::giveFocus(m_screenshot2);
        else brls::Application::giveFocus(m_buttonComment);
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
    int modId = m_manager.modId();
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, modId](std::stop_token token) {
        auto result = api::mod::fetchModDetail(modId, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            if (!result.success) {
                m_leftLoading->setText(brls::getStr("page/storeModDetail/loadFailed"));
                auto goBack = [this, token] {
                    CustomDialog::close();
                    brls::sync([this, token] {
                        if (token.stop_requested()) return;
                        Page::popPage(PageAnimType::SlideFromRight);
                    });
                };
                CustomDialog::show(result.error, {{brls::getStr("page/storeModDetail/ok"), goBack}}, goBack);
                return;
            }
            m_manager.setDetail(std::move(result.detail));
            applyLocalState();
            m_leftContent->setVisibility(brls::Visibility::VISIBLE);
            m_leftLoading->setVisibility(brls::Visibility::GONE);
            m_detailLoaded = true;
            setActionAvailable(brls::BUTTON_Y, !m_manager.getDetail().changelog.empty());
            setActionAvailable(brls::BUTTON_X, !m_manager.getDetail().authorLink.empty());
            updateDetail();
        });
    }, token);
}

void StoreModDetail::loadScreenshots() {
    loadScreenshot(1);
    loadScreenshot(2);
}

void StoreModDetail::loadScreenshot(int index) {
    auto gameTid = m_manager.gameTid();
    int modId = m_manager.modId();
    auto token = m_stopSource.get_token();

    ThreadPool::instance().submit([this, gameTid, modId, index](std::stop_token token) {
        auto result = api::mod::fetchScreenshot(gameTid, modId, index, token);
        if (token.stop_requested()) return;
        webpDecoder::WebpImage image;
        if (result.success && !result.data.empty()) {
            image = webpDecoder::decode(result.data.data(), result.data.size());
        }
        if (token.stop_requested()) return;

        FrameQueue::enqueue(token, [this, index, image = std::move(image)]() mutable {
            applyScreenshot(index, std::move(image));
        });
    }, token);
}

void StoreModDetail::applyScreenshot(int index, webpDecoder::WebpImage image) {
    brls::Image* screenshots[] = { m_screenshot1, m_screenshot2 };
    SkeletonView* skeletons[] = { m_screenshotSkeleton1, m_screenshotSkeleton2 };
    auto* screenshot = screenshots[index - 1];
    auto* skeleton = skeletons[index - 1];

    int textureId = 0;
    if (image.width > 0 && image.height > 0 && !image.pixels.empty()) {
        textureId = nvgCreateImageRGBA(brls::Application::getNVGContext(), image.width, image.height, 0, image.pixels.data());
    }

    if (textureId > 0) {
        screenshot->innerSetImage(textureId);
        screenshot->setFreeTexture(true);  // 截图纹理由 Image 自己释放
        screenshot->setFocusable(true);
        screenshot->setShadowType(brls::ShadowType::GENERIC);
    } else {
        screenshot->setImageFromRes("img/mod/noImage.png");
        screenshot->setShadowType(brls::ShadowType::NONE);
    }
    skeleton->setVisibility(brls::Visibility::GONE);
    screenshot->setVisibility(brls::Visibility::VISIBLE);
}

void StoreModDetail::loadNextCommentPage() {
    m_commentLoading = true;
    int modId = m_manager.modId();
    int page = m_manager.commentPage() + 1;
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, modId, page](std::stop_token token) {
        auto result = api::comment::fetchComments(modId, page, 20, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            m_commentLoading = false;

            if (!result.success) {
                m_rightHintLabel->setText(brls::getStr("page/storeModDetail/loadFailed"));
                return;
            }

            for (auto& comment : result.list) {
                comment.nickname = displayCommentNickname(comment.nickname);
            }

            m_manager.appendCommentPage(std::move(result));

            if (m_manager.getComments().empty()) {
                m_rightHintLabel->setText(brls::getStr("page/storeModDetail/noComments"));
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
    }, token);
}

std::string StoreModDetail::displayCommentNickname(const std::string& nickname) {
    if (nickname == ANONYMOUS_NICKNAME) {
        return brls::getStr("page/storeModDetail/anonymousUser");
    }
    return nickname;
}

void StoreModDetail::updateScrollHintVisibility() {
    bool contentOverflows = m_scrollContent->getHeight() > m_scroll->getHeight();
    auto visibility = m_detailLoaded && !m_leftCard->isFocused() && contentOverflows ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE;
    if (m_scrollHint->getVisibility() != visibility) m_scrollHint->setVisibility(visibility);
}

void StoreModDetail::updateDetail() {
    auto& detail = m_manager.getDetail();

    // 图标和名称
    m_modIcon->setImageFromRes(modTypeIcon(detail.modType));
    m_modName->setText(detail.modName);
    m_installedIcon->setVisibility(detail.downloaded ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_installedUpdateBadge->setVisibility(detail.downloaded && detail.hasUpdate ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_buttonDownload->setBadgeVisible(detail.downloaded && detail.hasUpdate);

    // 统计数据
    m_statDownload->setText(std::to_string(detail.downloadCount));
    m_statTime->setText(format::timeAgo(detail.uploadTime));

    // 胶囊标签
    m_tagType->setText(modTypeText(detail.modType));
    m_tagAuthor->setText(detail.author.empty() ? brls::getStr("page/storeModDetail/unknownAuthor") : brls::getStr("page/storeModDetail/authorPrefix", detail.author));
    m_tagSize->setText(detail.fileSize > 0 ? format::fileSize(detail.fileSize) : brls::getStr("page/storeModDetail/sizeUnknown"));
    m_tagVersion->setText(detail.modVersion.empty() ? brls::getStr("page/storeModDetail/modVersionUnknown") : brls::getStr("page/storeModDetail/modVersionFmt", format::cleanVersion(detail.modVersion)));
    if (detail.gameVersion.empty()) m_tagGameVersion->setText(brls::getStr("page/storeModDetail/gameVersionUnknown"));
    else if (detail.gameVersion == "0") m_tagGameVersion->setText(brls::getStr("page/storeModDetail/gameVersionUniversal"));
    else m_tagGameVersion->setText(brls::getStr("page/storeModDetail/gameVersionFmt", format::cleanVersion(detail.gameVersion)));

    // 描述（左卡片）
    m_descBody->setText(detail.description.empty() ? brls::getStr("page/storeModDetail/noDescription") : detail.description);

    // 根据赞踩状态刷新图标和数字
    updateVoteIcons();
    updateScrollHintVisibility();
}

void StoreModDetail::applyLocalState() {
    auto& detail = m_manager.getDetail();
    detail.downloaded = false;
    detail.hasUpdate = false;
    detail.installed = false;

    if (!m_localModManager) return;

    int idx = m_localModManager->findByModID(detail.modId);
    if (idx < 0) return;

    detail.downloaded = true;

    auto& localMod = m_localModManager->mods()[idx];
    detail.installed = localMod.isInstalled;
    detail.hasUpdate = !localMod.fileCrc32.empty() && !detail.fileCrc32.empty() && localMod.fileCrc32 != detail.fileCrc32;
}

void StoreModDetail::updateVoteIcons() {
    auto& detail = m_manager.getDetail();

    std::string likeRes = detail.deviceLiked ? "img/mod/like-done.png" : format::themedIconPath("img/mod/like");
    m_iconLike->setImageFromRes(likeRes);
    m_buttonLike->setSelected(detail.deviceLiked);

    std::string dislikeRes = detail.deviceDisliked ? "img/mod/dislike-done.png" : format::themedIconPath("img/mod/dislike");
    m_iconDislike->setImageFromRes(dislikeRes);
    m_buttonDislike->setSelected(detail.deviceDisliked);

    // 数字
    m_statLike->setText(std::to_string(detail.likeCount));
    m_statDislike->setText(std::to_string(detail.dislikeCount));
}

void StoreModDetail::showChangelogDialog() {
    auto& changelog = m_manager.getDetail().changelog;
    std::string text = changelog.empty() ? brls::getStr("page/storeModDetail/noChangelog") : changelog;

    auto* body = new brls::Box();
    body->setAxis(brls::Axis::COLUMN);
    body->setWidthPercentage(100);

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(20);
    label->setLineHeight(1.35f);
    label->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    label->setSingleLine(false);
    label->setWidthPercentage(100);
    body->addView(label);

    ScrollDialog::show(body, brls::getStr("page/storeModDetail/cancel"), [] { ScrollDialog::close(); }, brls::getStr("page/storeModDetail/confirm"), [] { ScrollDialog::close(); });
}

void StoreModDetail::onLikeAction() {
    if (m_voteInFlight) {
        CustomDialog::show(brls::getStr("page/storeModDetail/tooFast"), {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
        return;
    }

    // 乐观更新：先翻转本地状态 + 刷新 UI
    m_manager.applyLikeToggle();
    updateVoteIcons();
    m_voteInFlight = true;

    // 异步请求服务器
    int modId = m_manager.modId();
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, modId](std::stop_token token) {
        auto result = api::mod::toggleLike(modId, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            m_voteInFlight = false;
            if (!result.success) {
                m_manager.applyLikeToggle();
                updateVoteIcons();
                CustomDialog::show(result.error, {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            }
        });
    }, token);
}

void StoreModDetail::onDislikeAction() {
    if (m_voteInFlight) {
        CustomDialog::show(brls::getStr("page/storeModDetail/tooFast"), {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
        return;
    }

    // 乐观更新：先翻转本地状态 + 刷新 UI
    m_manager.applyDislikeToggle();
    updateVoteIcons();
    m_voteInFlight = true;

    // 异步请求服务器
    int modId = m_manager.modId();
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, modId](std::stop_token token) {
        auto result = api::mod::toggleDislike(modId, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            m_voteInFlight = false;
            if (!result.success) {
                m_manager.applyDislikeToggle();
                updateVoteIcons();
                CustomDialog::show(result.error, {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            }
        });
    }, token);
}

void StoreModDetail::onDownloadAction() {
    auto& detail = m_manager.getDetail();

    if (showDownloadedPrompt(detail)) return;
    showDownloadConfirmDialog();
}

bool StoreModDetail::showDownloadedPrompt(const api::mod::ModDetail& detail) {
    if (!detail.downloaded) return false;

    if (!detail.hasUpdate) CustomDialog::show(brls::getStr("page/storeModDetail/noRepeatInstall"), {{brls::getStr("page/storeModDetail/ok"), [] { CustomDialog::close(); }}});
    else if (detail.installed) CustomDialog::show(brls::getStr("page/storeModDetail/updateInstalledBlocked"), {{brls::getStr("page/storeModDetail/ok"), [] { CustomDialog::close(); }}});
    else CustomDialog::show(brls::getStr("page/storeModDetail/confirmUpdate"), {{brls::getStr("page/storeModDetail/cancel"), [] { CustomDialog::close(); }}, {brls::getStr("page/storeModDetail/confirm"), [this] { CustomDialog::close([this] { startDownload(true); }); }}});

    return true;
}

void StoreModDetail::showDownloadConfirmDialog() {
    CustomDialog::show(brls::getStr("page/storeModDetail/confirmDownload"), {
        {brls::getStr("page/storeModDetail/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/storeModDetail/confirm"), [this] { CustomDialog::close([this] { startDownload(false); }); }},
    });
}

void StoreModDetail::startDownload(bool updateMode) {
    auto& detail = m_manager.getDetail();

    // 重置下载取消源
    m_downloadStop = std::stop_source{};
    auto dlToken = m_downloadStop.get_token();

    auto onCancel = [this] {
        m_downloadStop.request_stop();
        ProgressDialog::close();
    };

    detail.downloadCount++;
    m_statDownload->setText(std::to_string(detail.downloadCount));

    ProgressDialog::show(brls::getStr("page/storeModDetail/downloading", detail.modName), {{brls::getStr("page/storeModDetail/cancel"), onCancel}}, onCancel);
    ProgressDialog::setLeftText(brls::getStr("page/storeModDetail/calculating"));
    ProgressDialog::setRightText("--:--");

    int modId = detail.modId;
    int expectedFileSize = detail.fileSize;
    std::string gameTid = detail.gameTid;
    std::string gameName = detail.gameName;
    std::string modName = detail.modName;

    ThreadPool::instance().submit([this, updateMode, modId, expectedFileSize, gameTid, gameName, modName](std::stop_token token) {
        // ── 阶段一：获取下载信息 ──
        auto info = api::mod::fetchDownloadInfo(modId, token);
        if (token.stop_requested()) return;

        if (!info.success) {
            brls::sync([error = std::move(info.error), token] {
                if (token.stop_requested()) return;
                CustomDialog::show(error, {{brls::getStr("page/storeModDetail/ok"), [] { CustomDialog::close(); }}});
            });
            return;
        }

        // ── 阶段二：计算文件名 ──
        std::string modDirName = textClean::safeDirName(info.modNameEn);
        if (modDirName.empty()) modDirName = "mod_" + std::to_string(modId);
        std::string tempPath = std::string(config::modShopDir) + "temp/" + modDirName + ".zip";

        // ── 阶段三：下载 ──
        using Clock = std::chrono::steady_clock;
        auto lastProgressTime = Clock::now();
        auto lastBarTime = Clock::now();
        auto lastTextTime = Clock::now();
        long long lastBytes = 0;
        double smoothedSpeed = 0;

        auto onProgress = [&, token](size_t total, size_t now) -> bool {
            auto t = Clock::now();
            bool textUpdate = std::chrono::duration<double>(t - lastTextTime).count() >= 1.0;
            bool barUpdate = std::chrono::duration<double>(t - lastBarTime).count() >= 0.1;
            float pct = total > 0 ? now * 100.0f / total : 0;

            bool finalProgress = expectedFileSize <= 0 || total >= static_cast<size_t>(expectedFileSize) * 9 / 10;
            if (!finalProgress) return true;

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
                long long remaining = total > now ? total - now : 0;
                int etaSeconds = speed > 0 ? static_cast<int>(remaining / speed) : 0;
                size_t nowCopy = now;
                size_t totalCopy = total;

                brls::sync([=] {
                    if (token.stop_requested()) return;
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

        deviceControl::CpuBoost::enableFastLoad();
        auto result = api::mod::download(modId, tempPath, onProgress, token);
        deviceControl::CpuBoost::disable();

        if (!result.success) {
            brls::sync([error = std::move(result.error), token] {
                if (token.stop_requested()) return;
                CustomDialog::show(error, {{brls::getStr("page/storeModDetail/ok"), [] { CustomDialog::close(); }}});
            });
            return;
        }

        // ── 阶段四：安装 ──
        std::string gameNameEn = info.gameNameEn;
        brls::sync([this, updateMode, gameTid, gameNameEn, gameName, modDirName, tempPath, modName, token] {
            if (token.stop_requested()) return;
            if (updateMode) finishUpdateOnMainThread(tempPath, modName);
            else finishDownloadOnMainThread(gameTid, gameNameEn, gameName, modDirName, tempPath, modName);
        });
    }, dlToken);
}

void StoreModDetail::finishDownloadOnMainThread(const std::string& gameTid, const std::string& gameNameEn, const std::string& gameName, const std::string& modDirName, const std::string& tempPath, const std::string& modName) {
    ProgressDialog::hideButtons();

    std::string gameDir;
    if (m_localModManager) {
        gameDir = m_gameManager.addExistingGameFromStore(m_localModManager->game().dirPath);
        ModInfo modInfo = m_manager.createDownloadedMod(gameDir, modDirName, tempPath);
        m_localModManager->setPendingFocus(modInfo.modID);
        m_localModManager->addModFromStore(std::move(modInfo));
    } else {
        gameDir = m_gameManager.addNewGameFromStore(gameTid, gameNameEn, gameName);
        m_manager.saveDownloadedMod(gameDir, modDirName, tempPath);
    }

    int gameIndex = m_gameManager.findByDirPath(gameDir);
    loadGameMetadata(gameIndex, gameTid);
    m_gameManager.setPendingFocusPath(gameDir);
    m_installedIcon->setVisibility(brls::Visibility::VISIBLE);
    showCompleteDialog(brls::getStr("page/storeModDetail/downloadComplete", modName));
}

void StoreModDetail::loadGameMetadata(int gameIndex, const std::string& gameTid) {
    auto& game = m_gameManager.games()[gameIndex];
    if (game.iconId > 0) return;

    auto meta = m_gameManager.fetchMetadata(gameIndex);
    if (meta.name.empty()) return;

    m_gameManager.setGameName(gameIndex, meta.name);
    m_gameManager.setVersion(gameIndex, meta.version);

    auto& tc = brls::TextureCache::instance();
    int iconId = tc.getCache(gameTid);
    if (iconId == 0) {
        auto* vg = brls::Application::getNVGContext();
        iconId = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(meta.icon.data()), meta.icon.size());
        if (iconId > 0) tc.addCache(gameTid, iconId);
    }
    if (iconId > 0) game.iconId = iconId;
}

void StoreModDetail::finishUpdateOnMainThread(const std::string& tempPath, const std::string& modName) {
    ProgressDialog::hideButtons();

    auto& detail = m_manager.getDetail();
    int index = m_localModManager->findByModID(detail.modId);
    auto& oldMod = m_localModManager->mods()[index];
    ModInfo modInfo = m_manager.buildDownloadedModInfo(oldMod.dirName, oldMod.path);
    if (!m_localModManager->updateModFromStore(index, std::move(modInfo), tempPath)) {
        CustomDialog::show(brls::getStr("page/storeModDetail/updateFailed"), {{brls::getStr("page/storeModDetail/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    m_gameManager.setPendingFocusPath(m_localModManager->game().dirPath);
    m_localModManager->setPendingFocus(detail.modId);
    applyLocalState();
    updateDetail();

    showCompleteDialog(brls::getStr("page/storeModDetail/updateComplete", modName));
}

void StoreModDetail::showCompleteDialog(const std::string& message) {
    bool fromModList = m_fromModList;
    std::string backKey = fromModList ? "page/storeModDetail/backToModList" : "page/storeModDetail/backToHome";
    CustomDialog::show(message, {
        {brls::getStr("page/storeModDetail/continueBrowse"), [] { CustomDialog::close(); }},
        {brls::getStr(backKey), [this, fromModList] {
            auto* pageHost = dynamic_cast<PageHost*>(getParent());
            int popCount = static_cast<int>(pageHost->getPageCount()) - (fromModList ? 2 : 1);
            CustomDialog::close([this, popCount] { popFromDetail(popCount); });
        }},
    });
}

void StoreModDetail::showAnonymousCommentPrompt() {
    CustomDialog::show(brls::getStr("page/storeModDetail/anonymousCommentPrompt"), {
        {brls::getStr("page/storeModDetail/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/storeModDetail/createNickname"), [this] {
            CustomDialog::close([this] {
                brls::sync([this] { createNicknameFromCommentPrompt(); });
            });
        }},
        {brls::getStr("page/storeModDetail/directComment"), [this] {
            CustomDialog::close([this] {
                brls::sync([this] { beginCommentInput(ANONYMOUS_NICKNAME); });
            });
        }},
    });
}

void StoreModDetail::createNicknameFromCommentPrompt() {
    std::string current = Settings::getString("modShop", "nickname");
    std::string name = keyboard::showText(brls::getStr("page/storeModDetail/inputNickname"), brls::getStr("page/storeModDetail/inputNickname"), current, 32);
    if (name.empty()) return;
    Settings::setString("modShop", "nickname", name);
}

void StoreModDetail::beginCommentInput(std::string nickname) {
    // 弹出键盘输入留言内容（限制 200 字符）
    std::string content = keyboard::showText(brls::getStr("page/storeModDetail/inputComment"), brls::getStr("page/storeModDetail/inputComment"), "", 200);
    if (content.empty()) return;

    CustomDialog::show(brls::getStr("page/storeModDetail/submittingComment"), {}, [] {});

    // 异步提交
    int modId = m_manager.modId();
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, modId, nickname, content](std::stop_token token) {
        auto result = api::comment::submitComment(modId, nickname, content, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token] {
            if (token.stop_requested()) return;
            if (result.success) {
                Audio::instance()->play(SoundEffect::Enter);
                CustomDialog::show(brls::getStr("page/storeModDetail/commentSuccess"), {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            } else {
                Audio::instance()->play(SoundEffect::Enter);
                CustomDialog::show(result.error, {{brls::getStr("page/storeModDetail/okShort"), [] { CustomDialog::close(); }}});
            }
        });
    }, token);
}

void StoreModDetail::onCommentAction() {
    std::string nickname = Settings::getString("modShop", "nickname");
    if (nickname.empty()) {
        showAnonymousCommentPrompt();
        return;
    }

    beginCommentInput(std::move(nickname));
}
