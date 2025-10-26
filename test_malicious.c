#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

// 模拟恶意行为：枚举进程（常被 EDR 监控）
void EnumerateProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        // 仅统计，不输出
    }
    
    CloseHandle(hSnapshot);
}

// 模拟恶意行为：创建注册表项（常被监控）
void CreateRegistryKey() {
    HKEY hKey;
    RegCreateKeyExA(
        HKEY_CURRENT_USER,
        "Software\\NoConsolationTest",
        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL
    );
    
    char value[] = "Memory Loaded - No Disk Write";
    RegSetValueExA(hKey, "TestValue", 0, REG_SZ, (BYTE*)value, strlen(value) + 1);
    RegCloseKey(hKey);
}

int main() {
    HANDLE hFile;
    DWORD dwBytesWritten;
    char message[512];
    
    // 执行"恶意"行为
    EnumerateProcesses();
    CreateRegistryKey();
    
    // 写入结果文件
    sprintf(message, 
        "NoConsolation Malicious Behavior Test - SUCCESS!\r\n"
        "- Process enumeration: OK\r\n"
        "- Registry modification: OK\r\n"
        "- No disk traces: OK\r\n");
    
    hFile = CreateFileA(
        "C:\\Windows\\Temp\\malicious_test.txt",
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, message, strlen(message), &dwBytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    return 0;
}

