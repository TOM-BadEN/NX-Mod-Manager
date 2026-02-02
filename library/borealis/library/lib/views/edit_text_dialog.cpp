#include <borealis/views/edit_text_dialog.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/application.hpp>

#ifdef __PSV__
#define EDIT_TEXT_DIALOG_POP_ANIMATION TransitionAnimation::NONE
#define EDIT_TEXT_DIALOG_BACKGROUND_TRANSLUCENT false
#include <SDL2/SDL.h>
#include "borealis/platforms/sdl/sdl_video.hpp"
#else
#define EDIT_TEXT_DIALOG_POP_ANIMATION TransitionAnimation::FADE
#define EDIT_TEXT_DIALOG_BACKGROUND_TRANSLUCENT true
#endif

namespace brls
{

    const std::string editTextDialogXML = R"xml(
    <brls:Box
        id="brls/container"
        width="auto"
        height="auto"
        axis="column"
        justifyContent="flexStart"
        alignItems="center"
        focusable="true"
        hideHighlight="true"
        backgroundColor="@theme/brls/backdrop">

        <brls:Label
            id="brls/dialog/header"
            fontSize="24")xml"
#ifdef __PSV__
            R"xml(marginTop="30")xml"
#else
            R"xml(marginTop="50")xml"
#endif
      R"xml(marginBottom="30"
            textColor="#FFFFFF"/>

        <brls:Box
            id="brls/dialog/applet"
            width="720"
            cornerRadius="4"
            alignItems="flexEnd"
            axis="column"
            backgroundColor="@theme/brls/background">

            <brls:Label
                id="brls/dialog/label"
                grow="1"
                width="680"
                cursor="-1"
                minHeight="30"
                margin="20"
                autoAnimate="false"
                verticalAlign="top"/>

            <brls:Label
                id="brls/dialog/count"
                width="680"
                horizontalAlign="right"
                fontSize="18"
                textColor="@theme/brls/text_disabled"
                marginRight="30"
                marginBottom="10"/>

            <brls:Hints
                allowAButtonTouch="true"
                forceShown="true"
                addBaseAction="false"
                marginBottom="20"
                marginRight="10"
                width="auto"
                height="auto"/>
        </brls:Box>

    </brls:Box>
    )xml";

    EditTextDialog::EditTextDialog()
    {
        this->inflateFromXMLString(editTextDialogXML);

        // submit text
        this->registerAction(
            "hints/ok"_i18n, BUTTON_A, [this](...)
            {
                Application::popActivity(EDIT_TEXT_DIALOG_POP_ANIMATION, [this](){
                        this->summitEvent.fire();
                    });
                return true; });
        this->registerAction(
            "hints/ok"_i18n, BUTTON_START, [this](...)
            {
                Application::popActivity(EDIT_TEXT_DIALOG_POP_ANIMATION, [this](){
                        this->summitEvent.fire();
                    });
                return true; }, true);

        // register paste action
        int pasteKey = BRLS_KBD_MODIFIER_CTRL;
#ifdef __APPLE__
        pasteKey = BRLS_KBD_MODIFIER_META;
#endif
        this->registerAction({ BRLS_KBD_KEY_V, pasteKey }, [this](...)
        {
            this->clipboardEvent.fire(Application::getPlatform()->pasteFromClipboard());
            return true;
        });

        // register delete action
        this->registerAction({ BRLS_KBD_KEY_DELETE }, [this](...)
        {
            // This is to ensure that the delete operation of the PSV ime will not be ignored
            this->backspaceEvent.fire();
            return true;
        }, true);
        this->registerAction({ BRLS_KBD_KEY_BACKSPACE }, [this](...)
        {
            this->backspaceEvent.fire();
            return true;
        }, true);

        this->registerAction("hints/delete"_i18n, BUTTON_BACK, [this](...)
            {
                this->backspaceEvent.fire();
                return true; }, true, true);

        // cancel input
        this->registerAction(
            "hints/back"_i18n, BUTTON_B, [this](...)
            {
                Application::popActivity(EDIT_TEXT_DIALOG_POP_ANIMATION, [this](){
                        this->cancelEvent.fire();
                    });
                return true; });

#if defined(__PSV__) and !defined(BOREALIS_USE_GXM)
        // After turning off the on-screen keyboard, tap on the input area to reopen
        this->addGestureRecognizer(new brls::TapGestureRecognizer([](brls::TapGestureStatus status, brls::Sound*) {
            if (status.state == brls::GestureState::END) {
                auto* videoContext = (SDLVideoContext*)Application::getPlatform()->getVideoContext();
                if (!SDL_IsScreenKeyboardShown(videoContext->getSDLWindow()))
                {
                    SDL_StopTextInput();
                    SDL_StartTextInput();
                }
                Application::setInputType(InputType::GAMEPAD);
            }
        }));
        // Force to set to gamepad mode to avoid missing the `Enter` keyboard event
        Application::setInputType(InputType::GAMEPAD);
#endif

        this->init = true;
    }

    EditTextDialog::~EditTextDialog() = default;

    void EditTextDialog::open()
    {
        Application::pushActivity(new Activity(this));
    }

    void EditTextDialog::setText(const std::string& value)
    {
        this->content = value;
        this->updateUI();
    }

    void EditTextDialog::setHeaderText(const std::string& value)
    {
        this->header->setText(value);
    }

    void EditTextDialog::setHintText(const std::string& value)
    {
        if (value.empty())
        {
            this->hint = "hints/input"_i18n;
        }
        else
        {
            this->hint = value;
        }
        this->updateUI();
    }

    void EditTextDialog::setCountText(const std::string& value)
    {
        this->count->setText(value);
    }

    bool EditTextDialog::isTranslucent()
    {
        return EDIT_TEXT_DIALOG_BACKGROUND_TRANSLUCENT;
    }

    void EditTextDialog::onLayout()
    {
        if (!init)
            return;
        this->layoutEvent.fire(Point { this->label->getX(),
            this->label->getY() + this->label->getHeight() });
    }

    Event<Point>* EditTextDialog::getLayoutEvent()
    {
        return &this->layoutEvent;
    }

    Event<>* EditTextDialog::getBackspaceEvent()
    {
        return &this->backspaceEvent;
    }

    Event<>* EditTextDialog::getCancelEvent()
    {
        return &this->cancelEvent;
    }

    Event<>* EditTextDialog::getSubmitEvent()
    {
        return &this->summitEvent;
    }

    Event<std::string>* EditTextDialog::getClipboardEvent()
    {
        return &this->clipboardEvent;
    }

    void EditTextDialog::updateUI()
    {
        if (content.empty())
        {
            label->setTextColor(Application::getTheme().getColor("brls/text_disabled"));
            label->setText(hint);
            label->setCursor((int)CursorPosition::START);
        }
        else
        {
            label->setTextColor(Application::getTheme().getColor("brls/text"));
            label->setText(content);
        }
    }

    void EditTextDialog::setCursor(int cursor) {
        label->setCursor(cursor);
    }
}
