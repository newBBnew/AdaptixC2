#include <windows.h>
#include <stdio.h>

__declspec(dllimport) void MyCustomFunction();

int main() {
    HANDLE hFile;
    DWORD dwBytesWritten;
    char message[] = "Main EXE started, calling custom DLL...\r\n";
    
    hFile = CreateFileA(
        "C:\\Windows\\Temp\\main_test.txt",
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, message, strlen(message), &dwBytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    // 调用自定义 DLL 函数
    MyCustomFunction();
    
    return 0;
}

