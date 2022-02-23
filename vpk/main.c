#include <stdio.h>
#include <psp2/ctrl.h>
#include <taihen.h>
#include "debugScreen.h"
#include <psp2/sysmodule.h>
#include <psp2/libime.h>
#include <psp2/kernel/error.h> 
#include "hidkeyboard_uapi.h"
#include "layouts.h"

#define printf(...) psvDebugScreenPrintf(__VA_ARGS__)

SceUInt32 libime_work[SCE_IME_WORK_BUFFER_SIZE / sizeof(SceInt32)];
SceWChar16 libime_out[SCE_IME_MAX_PREEDIT_LENGTH + SCE_IME_MAX_TEXT_LENGTH + 1];
char libime_initval[8] = { 1 };
SceImeCaret caret_rev;
int ime_just_closed = 0;

static void WaitKeyPress();
SceUID LoadModule(const char *path, int flags, int type);
void ImeEventHandler(void* arg, const SceImeEventData* e);
int ImeInit();
int HidKeyboardInit();

int main (int argc, char **argv)
{
    int ret;
    SceCtrlData pad;

    psvDebugScreenInit();

    printf("VitaKeyboard by MarkOfTheLand\n");
    printf("starting the module... ");

    // stops before starting in case app was reset
    hidkeyboard_user_stop();
    ret = hidkeyboard_user_start();
    if (ret >= 0) {
        printf("hidkeyboard started successfully!\n");
    }
    else if (ret == HIDKEYBOARD_ERROR_DRIVER_ALREADY_ACTIVATED) {
        printf("hidkeyboard is already active!\n");
    }
    else if (ret < 0) {
        printf("Error on hidkeyboard_user_start(): 0x%08X\n", ret);
        WaitKeyPress();
        return -1;
    }

    printf("\nClose the virtual keyboard and press START to properly\nclose the application.\n");

    ret = ImeInit();
    if (ret < 0) {
        printf("Failed opening virtual keyboard.\n");
        WaitKeyPress();
        return -1;
    }

    psvDebugScreenCoordX = 10 * SCREEN_GLYPH_W * PSV_DEBUG_SCALE;
    psvDebugScreenCoordY = 25 * SCREEN_GLYPH_H * PSV_DEBUG_SCALE;
    printf("Press X to reopen the virtual keyboard.\n");
    while (1) {

        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_CROSS) {
            ret = ImeInit();
            if (ret < 0 && ret != SCE_IME_ERROR_ALREADY_OPENED) {
                printf("Error on opening virtual keyboard: 0x%08X\n", ret);
                WaitKeyPress();
                break;
            }
        }
        if (pad.buttons & SCE_CTRL_START) {
            break;
        }
        sceKernelDelayThread(10 * 1000); // about 100 fps

        sceImeUpdate();
    }

    hidkeyboard_user_stop();

    return 0;
}

void WaitKeyPress()
{
    SceCtrlData pad;

    printf("Press START to exit.\n");

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
        break;
        sceKernelDelayThread(100 * 1000);
    }
}

int ImeInit()
{
    sceSysmoduleLoadModule(SCE_SYSMODULE_IME);

    memset(libime_out, 0, ((SCE_IME_MAX_PREEDIT_LENGTH + SCE_IME_MAX_TEXT_LENGTH + 1) * sizeof(SceWChar16)));

    SceImeParam param;
    SceInt32 res;

    sceImeParamInit(&param);
    param.supportedLanguages = 0;
    param.languagesForced = SCE_TRUE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.option = SCE_IME_OPTION_NO_ASSISTANCE | SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
    param.inputTextBuffer = libime_out;
    param.maxTextLength = SCE_IME_MAX_TEXT_LENGTH;
    param.handler = ImeEventHandler;
    param.filter = NULL;
    param.initialText = (SceWChar16*)libime_initval;
    param.arg = NULL;
    param.work = libime_work;
    res = sceImeOpen(&param);

    return res;
}

void ImeEventHandler(void* arg, const SceImeEventData* e)
{
    switch (e->id) {
    case SCE_IME_EVENT_UPDATE_TEXT:
        if (e->param.text.caretIndex == 0) {
            // when you reopen the ime keyboard it updates text with an empty initial value
            if (ime_just_closed) {
                ime_just_closed = 0;
            }
            else {
                // backspace
                HidKeyBoardSendModifierAndKey(0x00, 0x2a);
                sceImeSetText((SceWChar16*)libime_initval, 4);
            }
        }
        else {
            // new character
            utf16_to_hid_mapping map;
            map = getLayoutMappingFromUtf16((unsigned short int)libime_out[1], pt_BR_layout, sizeof(pt_BR_layout) / sizeof(utf16_to_hid_mapping));

            HidKeyBoardSendModifierAndKey(map.hid_modifiers1, map.hid_key1);

            if (map.hid_key2 != 0x00) {
                sceKernelDelayThread(10 * 1000); // 0.01s
                HidKeyBoardSendModifierAndKey(map.hid_modifiers2, map.hid_key2);
            }

            memset(&caret_rev, 0, sizeof(SceImeCaret));
            memset(libime_out, 0, ((SCE_IME_MAX_PREEDIT_LENGTH + SCE_IME_MAX_TEXT_LENGTH + 1) * sizeof(SceWChar16)));
            caret_rev.index = 1;
            sceImeSetCaret(&caret_rev);
            sceImeSetText((SceWChar16*)libime_initval, 4);
        }
        break;
    case SCE_IME_EVENT_PRESS_ENTER:
        // enter
        HidKeyBoardSendModifierAndKey(0x00, 0x58);
        break;
    case SCE_IME_EVENT_PRESS_CLOSE:
        ime_just_closed = 1;
        sceImeClose();
        break;
    }
}