/**
 * MyFrame - 自定义应用框架
 * 
 * 结构：
 *   - Header：标题栏（左侧标题 + 右侧时间/电量）
 *   - Content：内容区域（用户的子视图放这里）
 *   - Footer：底部按钮提示栏
 */

#include "view/myframe.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <switch.h>
#include <malloc.h>

// 构造函数：加载布局 + 注册按键
MyFrame::MyFrame() {
    // 从资源文件加载 XML 布局
    inflateFromXMLRes("xml/view/myframe.xml");
    
    // 返回按钮处理
    auto backAction = [this](...) {
        if (!brls::Application::popActivity()) {
            auto dialog = new brls::Dialog("确定要退出吗？");
            dialog->addButton("取消", []() {});
            dialog->addButton("确定", []() {
                brls::Application::quit();
            });
            dialog->open();
        }
        return true;
    };
    registerAction("返回", brls::BUTTON_B, backAction);

#ifdef NXLINK
    brls::Application::setFPSStatus(true);
    m_fpsLabel->setVisibility(brls::Visibility::VISIBLE);
    m_memLabel->setVisibility(brls::Visibility::VISIBLE);
#endif
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

// 更新右上角的时间和电量显示
void MyFrame::updateStatusText() {
    auto timeNow = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(timeNow);
    auto tm = *std::localtime(&in_time_t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M");
    // 只有时间变化时才更新（避免每帧都更新）
    if (ss.str() != m_timeText) {
        m_timeText = ss.str();
        m_timeLabel->setText(m_timeText);
        
        auto platform = brls::Application::getPlatform();
        if (platform->canShowBatteryLevel()) {
            int level = platform->getBatteryLevel();
            m_batteryPercentLabel->setText(std::to_string(level) + "%");
        }
    }

#ifdef NXLINK
    size_t fps = brls::Application::getFPS();
    if (fps != m_lastFps) {
        m_lastFps = fps;
        m_fpsLabel->setText("FPS:" + std::to_string(fps));
    }
    extern char* fake_heap_start;
    extern char* fake_heap_end;
    struct mallinfo mi = mallinfo();
    uint64_t usedMB = mi.uordblks / (1024 * 1024);
    uint64_t totalMB = (fake_heap_end - fake_heap_start) / (1024 * 1024);
    if (usedMB != m_lastUsedMB) {
        m_lastUsedMB = usedMB;
        m_memLabel->setText(std::to_string(usedMB) + " / " + std::to_string(totalMB) + " MB");
    }
#endif
}

// 工厂函数：用于 XML 注册，让框架能通过 <MyFrame> 标签创建实例
brls::View* MyFrame::create() {
    return new MyFrame();
}
