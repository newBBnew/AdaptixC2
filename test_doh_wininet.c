#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#pragma comment(lib, "wininet.lib")

void log_line(const char* msg)
{
    FILE* f = fopen("wininet_test.log", "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ep)
{
    char buf[256];
    _snprintf(buf, sizeof(buf),
              "[EXCEPTION] Code=0x%08X, Addr=%p",
              ep->ExceptionRecord->ExceptionCode,
              ep->ExceptionRecord->ExceptionAddress);
    log_line(buf);
    MessageBoxA(NULL, buf, "WinInet Test Crash", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(void)
{
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

    log_line("=== WinInet test start ===");

    const char* ua  = "Mozilla/5.0";
    const char* url = "https://dns.google/dns-query"; // 可改为你的 DoH URL

    log_line("[STEP] Calling InternetOpenA");
    HINTERNET hInternet = InternetOpenA(
        ua,
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0);
    if (!hInternet) {
        DWORD err = GetLastError();
        char buf[128];
        _snprintf(buf, sizeof(buf),
                  "[ERROR] InternetOpenA failed, GetLastError=%lu", err);
        log_line(buf);
        printf("%s\n", buf);
        return 1;
    }
    log_line("[OK] InternetOpenA succeeded");

    log_line("[STEP] Parsing URL host/path");
    char host[256] = {0};
    char path[256] = {0};
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    BOOL isSecure = TRUE;

    const char* p = url;
    if (_strnicmp(p, "https://", 8) == 0) {
        p += 8;
        isSecure = TRUE;
    } else if (_strnicmp(p, "http://", 7) == 0) {
        p += 7;
        isSecure = FALSE;
        port = INTERNET_DEFAULT_HTTP_PORT;
    }

    const char* slash = strchr(p, '/');
    if (slash) {
        int hlen = (int)(slash - p);
        if (hlen > 255) hlen = 255;
        memcpy(host, p, hlen);
        host[hlen] = 0;
        _snprintf(path, sizeof(path), "%s", slash);
    } else {
        _snprintf(host, sizeof(host), "%s", p);
        _snprintf(path, sizeof(path), "/");
    }

    log_line("[STEP] Calling InternetConnectA");
    HINTERNET hConnect = InternetConnectA(
        hInternet,
        host,
        port,
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0,
        0);
    if (!hConnect) {
        DWORD err = GetLastError();
        char buf[128];
        _snprintf(buf, sizeof(buf),
                  "[ERROR] InternetConnectA failed, GetLastError=%lu", err);
        log_line(buf);
        printf("%s\n", buf);
        InternetCloseHandle(hInternet);
        return 1;
    }
    log_line("[OK] InternetConnectA succeeded");

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
    if (isSecure) flags |= INTERNET_FLAG_SECURE;

    log_line("[STEP] Calling HttpOpenRequestA");
    HINTERNET hRequest = HttpOpenRequestA(
        hConnect,
        "POST",
        path,
        NULL,
        NULL,
        NULL,
        flags,
        0);
    if (!hRequest) {
        DWORD err = GetLastError();
        char buf[128];
        _snprintf(buf, sizeof(buf),
                  "[ERROR] HttpOpenRequestA failed, GetLastError=%lu", err);
        log_line(buf);
        printf("%s\n", buf);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return 1;
    }
    log_line("[OK] HttpOpenRequestA succeeded");

    const char* headers = "Content-Type: application/dns-message\r\n"
                          "Accept: application/dns-message";
    BYTE dummy[12] = {0};

    log_line("[STEP] Calling HttpSendRequestA");
    BOOL res = HttpSendRequestA(
        hRequest,
        headers,
        (DWORD)strlen(headers),
        dummy,
        sizeof(dummy));
    if (!res) {
        DWORD err = GetLastError();
        char buf[128];
        _snprintf(buf, sizeof(buf),
                  "[ERROR] HttpSendRequestA failed, GetLastError=%lu", err);
        log_line(buf);
        printf("%s\n", buf);
    } else {
        log_line("[OK] HttpSendRequestA succeeded");
    }

    log_line("[STEP] Cleaning up");
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    log_line("=== WinInet test end ===");
    printf("Done. See wininet_test.log for details.\n");
    Sleep(2000);
    return 0;
}
