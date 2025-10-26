#include <windows.h>
#include <stdio.h>

int main() {
    HANDLE hFile;
    DWORD dwBytesWritten;
    char message[] = "NoConsolation Memory Execution Test - SUCCESS!\r\n";
    
    // 创建测试文件
    hFile = CreateFileA(
        "C:\\Windows\\Temp\\noconsolation_test.txt",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return 1;
    }
    
    WriteFile(hFile, message, strlen(message), &dwBytesWritten, NULL);
    CloseHandle(hFile);
    
    return 0;
}

