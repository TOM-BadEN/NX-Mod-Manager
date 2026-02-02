/*
    Copyright 2019-2021 natinusala
    Copyright 2021 XITRIX

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/button.hpp>
#include <borealis/views/rectangle.hpp>

namespace brls
{

// TODO: Add the blurred dialog type once the blur is finished

class DialogButton
{
  public:
    std::string label;
    VoidEvent::Callback cb;
};

// A modal dialog with zero to three buttons
// and anything as content
// Create the dialog then use open() and close()
class Dialog : public Box
{
  private:
    BRLS_BIND(Box, container, "brls/dialog/container");
    BRLS_BIND(AppletFrame, appletFrame, "brls/dialog/applet");

    unsigned frameX, frameY, frameWidth, frameHeight;

    std::vector<DialogButton*> buttons;

    void rebuildButtons();
    void buttonClick(DialogButton* button);

    bool cancelable = true;

  protected:
    BRLS_BIND(Button, button1, "brls/dialog/button1");
    BRLS_BIND(Button, button2, "brls/dialog/button2");
    BRLS_BIND(Button, button3, "brls/dialog/button3");

    BRLS_BIND(Rectangle, button2separator, "brls/dialog/button2/separator");
    BRLS_BIND(Rectangle, button3separator, "brls/dialog/button3/separator");

  public:
    Dialog(std::string text);
    Dialog(Box* contentView);
    ~Dialog();

    AppletFrame* getAppletFrame() override;

    /**
     * Adds a button to this dialog, with a maximum of three
     * The position depends on the add order
     *
     * Adding a button after the dialog has been opened is
     * NOT SUPPORTED
     */
    void addButton(std::string label, VoidEvent::Callback cb);

    /**
     * A cancelable dialog is closed when
     * the user presses B (defaults to true)
     *
     * A dialog without any buttons cannot
     * be cancelable
     */
    void setCancelable(bool cancelable);

    virtual void open();
    void close(std::function<void(void)> cb = [] {});

    bool isTranslucent() override
    {
        return true;
    }
};

} // namespace brls
