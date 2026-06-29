/**
 * ProgressDialog - 进度对话框实现
 */

#include "view/progressDialog.hpp"
#include <borealis/core/application.hpp>
#include <cstdio>

namespace {

brls::Label*     s_title      = nullptr;
brls::Label*     s_left       = nullptr;
brls::Label*     s_right      = nullptr;
brls::Rectangle* s_mainFill   = nullptr;
brls::Label*     s_percentage = nullptr;
brls::Rectangle* s_subFill    = nullptr;

} // anonymous namespace

namespace ProgressDialog {

void show(const std::string& title,
    std::vector<CustomDialog::ButtonConfig> buttons,
    std::function<void()> bAction)
{
    auto* content = dynamic_cast<brls::Box*>(brls::View::createFromXMLResource("view/progressDialog.xml"));
    if (!content) return;

    s_title      = dynamic_cast<brls::Label*>(content->getView("progress_dialog/text"));
    s_left       = dynamic_cast<brls::Label*>(content->getView("progress_dialog/leftText"));
    s_right      = dynamic_cast<brls::Label*>(content->getView("progress_dialog/rightText"));
    s_mainFill   = dynamic_cast<brls::Rectangle*>(content->getView("progress_dialog/mainFill"));
    s_percentage = dynamic_cast<brls::Label*>(content->getView("progress_dialog/percentage"));
    s_subFill    = dynamic_cast<brls::Rectangle*>(content->getView("progress_dialog/subFill"));
    if (s_title) s_title->setText(title);
    resetMainProgressColor();

    auto* dlg = new CustomDialog(content);
    for (auto& btn : buttons) {
        dlg->addButton(std::move(btn.label), std::move(btn.cb));
    }
    if (bAction) dlg->onB(std::move(bAction));
    dlg->open();
}

void setTitle(const std::string& text) {
    if (s_title) s_title->setText(text);
}

void setLeftText(const std::string& text) {
    if (s_left) s_left->setText(text);
}

void setRightText(const std::string& text) {
    if (s_right) s_right->setText(text);
}

void setMainProgress(float percent) {
    if (s_mainFill) s_mainFill->setWidthPercentage(percent);
    if (s_percentage) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f%%", percent);
        s_percentage->setText(buf);
    }
}

void setMainProgressColor(NVGcolor color) {
    if (s_mainFill) s_mainFill->setColor(color);
}

void resetMainProgressColor() {
    setMainProgressColor(brls::Application::getTheme().getColor("app/progressFill"));
}

void setSubProgress(int64_t written, int64_t total) {
    if (!s_subFill) return;
    if (written > 0 && total > 0) {
        s_subFill->setVisibility(brls::Visibility::VISIBLE);
        s_subFill->setWidthPercentage(written * 100.0f / total);
    } else {
        s_subFill->setVisibility(brls::Visibility::INVISIBLE);
    }
}

void hideSubProgress() {
    if (s_subFill) s_subFill->setVisibility(brls::Visibility::INVISIBLE);
}

void hideButtons() {
    CustomDialog::hideButtons();
}

void close(std::function<void()> cb) {
    CustomDialog::close(std::move(cb));
}

} // namespace ProgressDialog
