/**
 * CapsuleHints - 胶囊样式的全局按键提示栏
 */

#include "ui/view/shell/capsuleHints.hpp"

CapsuleHints::CapsuleHints() = default;

void CapsuleHints::addView(brls::View* view) {
    auto theme = brls::Application::getTheme();

    view->setHeight(46);
    view->setCornerRadius(23);
    view->setBackgroundColor(theme["app/shellCapsuleBg"]);
    view->setBorderColor(theme["app/shellCapsuleBorder"]);
    view->setBorderThickness(1.5f);
    if (!getChildren().empty()) view->setMarginLeft(8);

    auto* box = dynamic_cast<brls::Box*>(view);
    if (box) {
        box->setPadding(6, 18, 6, 18);
        for (auto* child : box->getChildren()) {
            auto* label = dynamic_cast<brls::Label*>(child);
            if (label) label->setFontSize(label->getFontSize() - 2);
        }
    }

    brls::Box::addView(view);
}

brls::View* CapsuleHints::create() {
    return new CapsuleHints();
}
