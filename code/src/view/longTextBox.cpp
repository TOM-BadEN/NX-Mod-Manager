/**
 * LongTextBox - 长文本内容 Box 生成模块实现
 */

#include "view/longTextBox.hpp"
#include "view/qrCode.hpp"

brls::Box* LongTextBox::create(const LongTextBoxConfig& content) {
    auto* root = new brls::Box(brls::Axis::COLUMN);
    root->setAlignItems(brls::AlignItems::STRETCH);

    auto theme = brls::Application::getTheme();
    brls::Box* previousGroup = nullptr;

    for (const auto& entry : content.entries) {
        bool hasTitle = !entry.title.empty();
        bool hasBody = !entry.body.empty();
        bool hasQr = !entry.qrItems.empty();
        if (!hasTitle && !hasBody && !hasQr) continue;

        if (previousGroup) previousGroup->setMarginBottom(28);

        auto* group = new brls::Box(brls::Axis::COLUMN);
        group->setAlignItems(brls::AlignItems::STRETCH);
        root->addView(group);
        previousGroup = group;

        if (hasTitle) {
            auto* title = new brls::Label();
            title->setText(entry.title);
            title->setFontSize(24);
            title->setTextColor(theme.getColor("brls/accent"));
            title->setSingleLine(true);
            if (hasBody || hasQr) title->setMarginBottom(28);
            group->addView(title);
        }

        if (hasBody) {
            auto* body = new brls::Label();
            body->setText(entry.body);
            body->setFontSize(20);
            body->setLineHeight(entry.bodyLineHeight);
            body->setSingleLine(false);
            body->setHorizontalAlign(brls::HorizontalAlign::LEFT);
            if (hasQr) body->setMarginBottom(28);
            group->addView(body);
        }

        if (!hasQr) continue;

        auto* qrRow = new brls::Box(brls::Axis::ROW);
        qrRow->setAlignItems(brls::AlignItems::CENTER);
        group->addView(qrRow);

        for (std::size_t i = 0; i < entry.qrItems.size(); i++) {
            const auto& item = entry.qrItems[i];

            auto* card = new brls::Box(brls::Axis::COLUMN);
            card->setWidth(156);
            card->setAlignItems(brls::AlignItems::CENTER);
            card->setBackgroundColor(theme.getColor("app/qrCardBg"));
            card->setCornerRadius(12);
            card->setPadding(12, 12, 16, 12);
            if (i > 0) card->setMarginLeft(33);
            qrRow->addView(card);

            auto* qr = new QrCode();
            qr->setWidth(120);
            qr->setHeight(120);
            card->addView(qr);
            qr->setText(item.url);

            if (!item.label.empty()) {
                auto* label = new brls::Label();
                label->setText(item.label);
                label->setFontSize(18);
                label->setTextColor(theme.getColor("app/textDark"));
                label->setMarginTop(8);
                label->setSingleLine(true);
                card->addView(label);
            }
        }
    }

    return root;
}
