#pragma once
#include <switch.h>

extern AppletType __nx_applet_type;

class HomeBtnBlock {
public:
    static void disable() {
        if (__nx_applet_type == AppletType_Application) {
            appletBeginBlockingHomeButtonShortAndLongPressed(0);
        } else {
            hiddbgInitialize();
            hidsysInitialize();
            hiddbgDeactivateHomeButton();
        }
    }

    static void enable() {
        if (__nx_applet_type == AppletType_Application) {
            appletEndBlockingHomeButtonShortAndLongPressed();
        } else {
            hidsysActivateHomeButton();
            hidsysExit();
            hiddbgExit();
        }
    }
};
