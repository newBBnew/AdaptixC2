#include <windows.h>

__declspec(dllexport) void MyCustomFunction() {
    HANDLE hFile;
    DWORD dwBytesWritten;
    char message[] = "Custom DLL function executed!\r\n";
    
    hFile = CreateFileA(
        "C:\\Windows\\Temp\\dll_test.txt",
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, message, strlen(message), &dwBytesWritten, NULL);
        CloseHandle(hFile);
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    return TRUE;
}

