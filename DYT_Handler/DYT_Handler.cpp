#include "injector.h"

#include "DYT_Module_dll.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <string>


/// <summary>
/// 
/// </summary>
/// <param name="hProc"></param>
/// <returns>>TRUE if process is WOW64, FALSE if not, -1 if error.</returns>
BOOL IsProcessWow64(HANDLE hProc) {
    BOOL bWow64 = FALSE;
    if (!IsWow64Process(hProc, &bWow64)) {
        printf("Can't confirm process architecture: 0x%X\n", GetLastError());
        return -1;
    }
    return bWow64;
}

/// <summary>
/// 
/// </summary>
/// <param name="hProc"></param>
/// <returns>TRUE if current process and target process have the same architecture, FALSE if not, -1 if error.</returns>
BOOL IsCorrectTargetArchitecture(HANDLE hProc) {
    BOOL bTarget = IsProcessWow64(hProc);
    if (bTarget == -1) {
        return -1;
    }

    BOOL bHost = IsProcessWow64(GetCurrentProcess());
    if (bHost == -1) {
        return -1;
    }

    return (bTarget == bHost);
}

DWORD GetProcessIdByName(LPCTSTR name) {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry) == TRUE) {
        while (Process32Next(snapshot, &entry) == TRUE) {
            if (lstrcmpi(entry.szExeFile, name) == 0) {
                CloseHandle(snapshot); //thanks to Pvt Comfy
                return entry.th32ProcessID;
            }
        }
    }

    CloseHandle(snapshot);
    return 0;
}

bool GetProcessNameById(DWORD processId, LPTSTR buffer, DWORD size) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) {
        return false;
    }

    bool success = GetModuleBaseName(hProcess, NULL, buffer, size) != 0;
    CloseHandle(hProcess);
    return success;
}

bool IsModuleLoaded(DWORD processId, const wchar_t* moduleName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32W);

    if (!Module32FirstW(hSnapshot, &moduleEntry)) {
        CloseHandle(hSnapshot);
        return false;
    }

    do {
        if (_wcsicmp(moduleEntry.szModule, moduleName) == 0) {
            CloseHandle(hSnapshot);
            return true;
        }
    } while (Module32NextW(hSnapshot, &moduleEntry));

    CloseHandle(hSnapshot);
    return false;
}


/// <summary>
/// 
/// </summary>
/// <param name="processName">Module name like "cs2.exe".</param>
/// <returns>0 - Unknown, 1 - Ready, 2, Not yet, -1 - Process not found.</returns>
int IsAppReady(DWORD processId) {
    TCHAR processName[MAX_PATH];
    if (!GetProcessNameById(processId, processName, MAX_PATH))
        return -1;

    if (lstrcmp(processName, TEXT("cs2.exe")) == 0)
    {
        // Updated on 6-12-2025
        const wchar_t* requiredModules[] = {
            TEXT("matchmaking.dll"),
            TEXT("navsystem.dll"),
            TEXT("client.dll")
        };

        for (const auto& module : requiredModules) {
            if (!IsModuleLoaded(processId, module)) {
                return 2;
            }
        }

        printf("\nCS2 will be ready in a few seconds...\n");
        Sleep(7777 * 1.35); // A lucky guess, CS2 takes a while to finish window initialization. (This may be slower on HDDs or slower systems)

        return 1;
    }

    return 0;
}

void AnimateWait(int maxPoints = 5, int delay = 500) {
    static int pointsPut = 0;

    if (maxPoints == 0) {
        pointsPut = 0;
        return;
    }

    if (pointsPut >= maxPoints) {
        printf("\033[%dD", pointsPut);
        printf("\033[0K");
        fflush(stdout);
        pointsPut = 0;
    }
    else
    {
        putchar('.');
        pointsPut++;
    }
    Sleep(delay);
}
void AnimateWaitReset() {
    AnimateWait(0);
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
start_get_proc_name:
    std::string pname = "";
    printf("Process Name: ");
    std::getline(std::cin, pname);

    char* vIn = (char*)pname.c_str();
    wchar_t* vOut = new wchar_t[strlen(vIn) + 1];
    mbstowcs_s(NULL, vOut, strlen(vIn) + 1, vIn, strlen(vIn));

    //_wsystem((L"start " + std::wstring(vOut)).c_str());

start_get_pid:
    DWORD PID = 0;
    printf("Trying to get process.");
    AnimateWaitReset();
    while (true) {
        PID = GetProcessIdByName(vOut);
        if (PID) {
            putchar('\n');
            break;
        }

        AnimateWait();
        Sleep(250);
    }

    printf("Process pid: %d\n", PID);

    printf("Waiting for process to be ready.");
    AnimateWaitReset();
    for (int isAppReady = 2; ; isAppReady = IsAppReady(PID))
    {
        if (isAppReady == 1) { // Ready
            putchar('\n');
            break;
        }
        if (isAppReady == 2) { // Not yet ready
            AnimateWait();
        }
        else
        {
            if (isAppReady == -1) { // Pid is not valid
                printf("Process is not valid anymore, retrying...\n");
                goto start_get_pid;
            }
            if (isAppReady == 0) { // Unknwon process
                putchar('\n');
                break;
            }
        }

        Sleep(250);
    }

    printf("Starting injection...\n");

    TOKEN_PRIVILEGES priv = { 0 };
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        priv.PrivilegeCount = 1;
        priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
            AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);

        CloseHandle(hToken);
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
    if (!hProc) {
        DWORD Err = GetLastError();
        printf("OpenProcess failed: 0x%X\n", Err);
        system("PAUSE");
        goto start_get_pid; // start;
        //return -2;
    }

    BOOL vIsCorrectTargetArchitecture = IsCorrectTargetArchitecture(hProc);
    if (vIsCorrectTargetArchitecture == -1) {
        printf("Unknwon Process Architecture.\n");
        CloseHandle(hProc);
        system("PAUSE");
        goto start_get_pid; // start;
        //return -3;
    }
    if (!vIsCorrectTargetArchitecture)
    {
        printf("Process architecture is not the same as current process, you should try %s\n", IsProcessWow64(GetCurrentProcess()) ? "DYT_Handler-x86.exe" : "DYT_Handler-x86_64.exe");
        CloseHandle(hProc);
        system("PAUSE");
        goto start_get_pid; // start;
        //return -3;
    }

     /*auto dllPath = LR"(C:\Users\ASUS\source\repos\DoYourThings\DYT_Module\build\bin\x64_Debug\DYT_Module.dll)";
     if (GetFileAttributes(dllPath) == INVALID_FILE_ATTRIBUTES) {
         printf("Dll file doesn't exist\n");
         CloseHandle(hProc);
         system("PAUSE");
         return -4;
     }
    
     std::ifstream File(dllPath, std::ios::binary | std::ios::ate);
    
     if (File.fail()) {
         printf("Opening the file failed: %X\n", (DWORD)File.rdstate());
         File.close();
         CloseHandle(hProc);
         system("PAUSE");
         return -5;
     }
    
     auto FileSize = File.tellg();
     if (FileSize < 0x1000) {
         printf("Filesize invalid.\n");
         File.close();
         CloseHandle(hProc);
         system("PAUSE");
         return -6;
     }
    
     BYTE* pSrcData = new BYTE[(UINT_PTR)FileSize];
     if (!pSrcData) {
         printf("Can't allocate dll file.\n");
         File.close();
         CloseHandle(hProc);
         system("PAUSE");
         return -7;
     }
    
     File.seekg(0, std::ios::beg);
     File.read((char*)(pSrcData), FileSize);
     File.close();*/
    
    auto FileSize = (SIZE_T)DYT_Module_dll_size;
    if (FileSize < 0x1000) {
        printf("Filesize invalid.\n");
        CloseHandle(hProc);
        system("PAUSE");
        return -6;
    }
    BYTE* pSrcData = new BYTE[FileSize];
    if (!pSrcData) {
        printf("Can't allocate dll file.\n");
        CloseHandle(hProc);
        system("PAUSE");
        return -7;
    }
    memcpy(pSrcData, DYT_Module_dll, FileSize);

    printf("Mapping...\n");
    if (!ManualMapDll(hProc, (BYTE*)pSrcData, FileSize, true,true,true,true, DLL_PROCESS_ATTACH, NULL, 11, (LPVOID)PID)) {
        CloseHandle(hProc);
        printf("Error while mapping.\n");
        system("PAUSE");
        goto start_get_pid; // start;
        //return -8;
    }

    printf("OK\n");

    WaitForSingleObject(hProc, INFINITE);
    CloseHandle(hProc);

    system("CLS");
    goto start_get_pid;
}
