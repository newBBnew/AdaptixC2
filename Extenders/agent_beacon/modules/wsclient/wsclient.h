#pragma once

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct _WS_CHANNEL_CONTEXT {
    const char* agent_id;
    const char* token;
    const char* url;
    const uint8_t* session_key; // 16 bytes
    const uint8_t* cert_data;   // optional PEM blob
    uint32_t cert_size;
} WS_CHANNEL_CONTEXT;
#pragma pack(pop)

__declspec(dllexport) BOOL __stdcall StartWsChannel(const WS_CHANNEL_CONTEXT* ctx);
__declspec(dllexport) void __stdcall StopWsChannel(void);

#ifdef __cplusplus
}
#endif
