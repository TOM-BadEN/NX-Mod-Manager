/**
 * MyFrame - 自定义应用框架
 * 
 * 结构：
 *   - Header：标题栏（左侧标题 + 右侧时间/电量）
 *   - Content：内容区域（用户的子视图放这里）
 *   - Footer：底部按钮提示栏
 */

#include "view/myframe.hpp"
#include <borealis/core/i18n.hpp>
#include "view/customDialog.hpp"
#include "core/audio.hpp"
#include "core/device.hpp"
#include "common/settings.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// 构造函数：加载布局 + 注册按键
MyFrame::MyFrame() {
    // 从资源文件加载 XML 布局
    inflateFromXMLRes("xml/view/myframe.xml");

    // 注册 subtitle XML 属性（页面 XML 可通过 subtitle="..." 设置副标题）
    this->registerStringXMLAttribute("subtitle", [this](const std::string& value) {
        setSubtitle(value);
    });

    // 返回按钮处理
    auto backAction = [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        if (!brls::Application::popActivity()) {
            CustomDialog::show(brls::getStr("views/myframe/exitConfirm"), {
                {brls::getStr("views/myframe/cancel"), [] { CustomDialog::close(); }},
                {brls::getStr("views/myframe/ok"), [] { CustomDialog::close([] { brls::Application::quit(); }); }},
            });
        }
        return true;
    };
    registerAction(brls::getStr("views/myframe/back"), brls::BUTTON_B, backAction, false, false);

    brls::Application::setFPSStatus(true);
    m_showFps = Settings::getBool("UI", "showFps", false);
    m_showMem = Settings::getBool("UI", "showMem", false);
    if (m_showFps) m_fpsLabel->setVisibility(brls::Visibility::VISIBLE);
    if (m_showMem) m_memLabel->setVisibility(brls::Visibility::VISIBLE);
}

// 框架自动调用：处理 <MyFrame> 标签内的子元素
void MyFrame::handleXMLElement(tinyxml2::XMLElement* element) {
    if (m_contentView) brls::fatal("MyFrame can only have one child XML element");

    // 把 XML 元素转换成视图对象，放到内容区
    brls::View* newView = brls::View::createFromXMLElement(element);
    setContentView(newView);
}

// 设置内容区域的视图
void MyFrame::setContentView(brls::View* newView) {
    if (m_contentView) {
        m_contentBox->removeView(m_contentView, false);
        m_contentView = nullptr;
    }

    if (!newView) return;

    // 设置新内容：自动尺寸 + 填充剩余空间
    m_contentView = newView;
    m_contentView->setDimensions(brls::View::AUTO, brls::View::AUTO);
    m_contentView->setGrow(1.0f); 
    m_contentBox->addView(m_contentView);
    
    // 更新标题栏的标题和图标
    updateFrameItem();
}

void MyFrame::setTitle(std::string title) {
    m_titleLabel->setText(title);
}

void MyFrame::setSubtitle(const std::string& text) {
    if (text.empty()) {
        m_subtitleLabel->setVisibility(brls::Visibility::GONE);
    } else {
        m_subtitleLabel->setText(text);
        m_subtitleLabel->setVisibility(brls::Visibility::VISIBLE);
    }
}

void MyFrame::setIcon(std::string path) {
    if (path.empty()) {
        m_iconImage->setVisibility(brls::Visibility::GONE);
    } else {
        m_iconImage->setVisibility(brls::Visibility::VISIBLE);
        m_iconImage->setImageFromFile(path);
    }
}

// 设置底部索引文本
void MyFrame::setIndexText(const std::string& text) {
    m_indexLabel->setText(text);
}

void MyFrame::setShowFps(bool show) {
    m_showFps = show;
    if (show) m_lastFpsUpdateUs = 0;
    m_fpsLabel->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void MyFrame::setShowMem(bool show) {
    m_showMem = show;
    if (show) m_lastMemUpdateUs = 0;
    m_memLabel->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

// 从内容视图获取标题和图标，更新到标题栏
void MyFrame::updateFrameItem() {
    if (!m_contentView) return;
    
    auto frameItem = m_contentView->getAppletFrameItem();
    setTitle(frameItem->title);
    setIcon(frameItem->iconPath);
}

// 每帧绘制时调用（用于更新时间和电量显示）
void MyFrame::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    updateStatusText();  // 先更新状态文本
    brls::Box::draw(vg, x, y, width, height, style, ctx);  // 再调用父类绘制
}

// 更新右上角状态显示
void MyFrame::updateStatusText() {
    brls::Time now = brls::getCPUTimeUsec();

    updateTimeStatus(now);
    updateBatteryStatus(now);
    updateFpsStatus(now);
    updateMemStatus(now);
}

void MyFrame::updateTimeStatus(brls::Time now) {
    if (m_lastTimeUpdateUs == 0 || now - m_lastTimeUpdateUs >= TIME_UPDATE_INTERVAL_US) {
        m_lastTimeUpdateUs = now;

        auto timeNow = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(timeNow);
        auto tm = *std::localtime(&in_time_t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M");
        // 只有时间变化时才更新（避免重复 setText 调用）
        if (ss.str() != m_timeText) {
            m_timeText = ss.str();
            m_timeLabel->setText(m_timeText);
        }
    }
}

void MyFrame::updateBatteryStatus(brls::Time now) {
    if (m_lastBatteryUpdateUs == 0 || now - m_lastBatteryUpdateUs >= BATTERY_UPDATE_INTERVAL_US) {
        m_lastBatteryUpdateUs = now;
        auto platform = brls::Application::getPlatform();
        if (platform->canShowBatteryLevel()) {
            int level = platform->getBatteryLevel();
            m_batteryPercentLabel->setText(std::to_string(level) + "%");
        }
    }
}

void MyFrame::updateFpsStatus(brls::Time now) {
    if (m_showFps && (m_lastFpsUpdateUs == 0 || now - m_lastFpsUpdateUs >= FPS_UPDATE_INTERVAL_US)) {
        m_lastFpsUpdateUs = now;

        size_t fps = brls::Application::getFPS();
        if (fps != m_lastFps) {
            m_lastFps = fps;
            m_fpsLabel->setText("FPS:" + std::to_string(fps));
        }
    }
}

void MyFrame::updateMemStatus(brls::Time now) {
    if (m_showMem && (m_lastMemUpdateUs == 0 || now - m_lastMemUpdateUs >= MEM_UPDATE_INTERVAL_US)) {
        m_lastMemUpdateUs = now;

        auto heap = deviceInfo::Memory::getHeapUsage();
        if (heap.usedMB != m_lastUsedMB || heap.totalMB != m_lastTotalMB) {
            m_lastUsedMB = heap.usedMB;
            m_lastTotalMB = heap.totalMB;
            m_memLabel->setText(std::to_string(heap.usedMB) + " / " + std::to_string(heap.totalMB) + " MB");
        }
    }
}

// 工厂函数：用于 XML 注册，让框架能通过 <MyFrame> 标签创建实例
brls::View* MyFrame::create() {
    return new MyFrame();
}
