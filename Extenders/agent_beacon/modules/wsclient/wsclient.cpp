#include "wsclient.h"

#include <windows.h>
#include <cstring>

static WS_CHANNEL_CONTEXT g_context = {0};

static void reset_context() {
    ZeroMemory(&g_context, sizeof(g_context));
}

BOOL __stdcall StartWsChannel(const WS_CHANNEL_CONTEXT* ctx) {
    if (ctx == nullptr || ctx->session_key == nullptr) {
        return FALSE;
    }
    if (ctx->url == nullptr || ctx->url[0] == '\0') {
        return FALSE;
    }

    g_context = *ctx;
    return TRUE;
}

void __stdcall StopWsChannel(void) {
    reset_context();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_DETACH) {
        reset_context();
    }
    return TRUE;
}
