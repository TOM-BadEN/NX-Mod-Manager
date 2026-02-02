#pragma once
#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>
#include <borealis/core/bind.hpp>

namespace brls
{
class EditTextDialog : public Box
{
  public:
    EditTextDialog();
    ~EditTextDialog() override;
    void open();
    void setText(const std::string& value);
    void setHeaderText(const std::string& value);
    void setHintText(const std::string& value);
    void setCountText(const std::string& value);
    void setCursor(int cursor);
    bool isTranslucent() override;
    void onLayout() override;
    Event<Point>* getLayoutEvent();
    Event<>* getBackspaceEvent();
    Event<>* getCancelEvent();
    Event<>* getSubmitEvent();
    Event<std::string>* getClipboardEvent();
    void updateUI();
  private:
    std::string content;
    std::string hint;
    Event<Point> layoutEvent;
    Event<> backspaceEvent, cancelEvent, summitEvent;
    Event<std::string> clipboardEvent;
    bool init = false;

    BRLS_BIND(brls::Label, header, "brls/dialog/header");
    BRLS_BIND(brls::Label, label, "brls/dialog/label");
    BRLS_BIND(brls::Label, count, "brls/dialog/count");
    BRLS_BIND(brls::Box, container, "brls/container");
};
}