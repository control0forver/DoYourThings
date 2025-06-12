#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include <string>
#include <iostream>
#include <filesystem>

#define GWL_WNDPROC (-4)

#ifdef _DEBUG
#define DEBUG_OUTPUTS
// #define DEBUG_BREAKPOINTS_WNDPROC
#endif

namespace std {
    typedef
#ifdef _UNICODE
        ::std::wstring
#else
        ::std::string
#endif
        tstring;
}


#ifdef DEBUG_OUTPUTS
#define DBG_OUTSTR(s) OutputDebugString(s)
#else
#define DBG_OUTSTR(s) NOP_FUNCTION
#endif

#ifdef DEBUG_BREAKPOINTS_WNDPROC
#define DBG_BREAK _CrtDbgBreak()
#else
#define DBG_BREAK NOP_FUNCTION
#endif

DWORD g_pid = NULL;
HMODULE g_hModule = NULL;
WNDPROC originalWndProc = NULL;
HWND targetHwnd = NULL;

bool g_Disabled = false;

void Unload();

std::tstring GetProcessName(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (hProcess != NULL) {
        TCHAR filename[MAX_PATH];
        if (GetModuleFileNameEx(hProcess, NULL, filename, MAX_PATH)) {
            CloseHandle(hProcess);
            std::tstring fullPath(filename);
            size_t pos = fullPath.find_last_of(TEXT("\\"));
            if (pos != std::tstring::npos) {
                return fullPath.substr(pos + 1);
            }
            return fullPath;
        }
        CloseHandle(hProcess);
    }
    return TEXT("noprocname");
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    std::tstring procName = GetProcessName(processId);
    std::tstring* targetName = (std::tstring*)lParam;

    if (procName.find(*targetName) != std::tstring::npos) {
        targetHwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

LRESULT CALLBACK HookedWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    TCHAR msg_info[250]{};
    wsprintf(msg_info, TEXT("HWnd: 0x%p, MsgId: %X, wp: %u, lp: %u\n"), hwnd, uMsg, wParam, lParam);
    DBG_OUTSTR(msg_info);

    // Unload disabled beacause module is going to be manual mapped by DYT_Handler
    //if (uMsg == WM_KEYDOWN && wParam == VK_INSERT ) {
    //    Unload(); // Unload DYT
    //    return 0;
    //}

    /*if (uMsg == WM_SYSCOMMAND && wParam == SC_KEYMENU) {
        DBG_OUTSTR(TEXT("用户正在管理窗口，暂停拦截\n"));
        g_Disabled = true;
    }*/
    // Alt + Tab or other user actions
    if (
        (uMsg == WM_KEYDOWN && wParam == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000))||
        (uMsg == WM_SYSKEYDOWN && wParam == VK_TAB)||
        (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_TAB) & 0x8000)
        ) {
        DBG_OUTSTR(TEXT("用户正在使用组合键Alt+Tab，暂停拦截\n"));
        g_Disabled = true;
    }
    // Window activated
    if (uMsg == WM_ACTIVATE && wParam != WA_INACTIVE)
    {
        g_Disabled = false;
    }

    if (g_Disabled) {
        return CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
    }

    if (hwnd == targetHwnd) {
        if ((uMsg == WM_ACTIVATEAPP && wParam == 0) ||
            (uMsg == WM_ACTIVATE && wParam == WA_INACTIVE) ||
            (uMsg == WM_MOVE && lParam == 2197848832) ||
            (uMsg == WM_NCACTIVATE && lParam == 0 && wParam == 0) ||
            (uMsg == WM_SIZE && wParam == SIZE_MINIMIZED && lParam == 0) ||
            uMsg == WM_KILLFOCUS) {
            DBG_OUTSTR(TEXT("即将失去焦点，阻止最小化！\n"));

            return 0; // Intercept
        }
        if (uMsg == WM_WINDOWPOSCHANGING)
        {
            WINDOWPOS& var = *(LPWINDOWPOS)lParam;
            DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);

            return r;
        }
        if (uMsg == WM_WINDOWPOSCHANGED)
        {
            const WINDOWPOS& var = *(LPWINDOWPOS)lParam; // Readonly
            DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);

            return r;
        }


        if (uMsg == WM_GETMINMAXINFO)
        {
            ///  lParam
            ///    指向 MINMAXINFO 结构的指针，该结构包含默认的最大化位置和尺寸，以及默认的最小和最大跟踪大小。 应用程序可以通过设置此结构的成员来替代默认值。

            ///  返回值
            ///    类型： LRESULT
            ///
            ///    如果应用程序处理此消息，则它应返回零。

            ///  备注
            ///    最大跟踪大小是使用边框调整窗口大小可以生成的最大窗口大小。 最小跟踪大小是通过使用边框调整窗口大小可以生成的最小窗口大小。
            ///
            MINMAXINFO& var = *(LPMINMAXINFO)lParam;
            DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
            DBG_BREAK;

            return r;
        }

        if (uMsg == WM_NCCALCSIZE && wParam == TRUE)
        {
            // https://learn.microsoft.com/zh-cn/windows/win32/winmsg/wm-nccalcsize

            ///  如果 wParam 为 TRUE，则应用程序应返回零或以下值的组合。
            ///  如果 wParam 为 TRUE 且应用程序返回零，则会保留旧工作区，并与新工作区的左上角对齐。
            /* WVR_ALIGNTOP
               0x0010
                指定保留窗口的工作区，并与窗口新位置的顶部对齐。 例如，若要将工作区与左上角对齐，请返回WVR_ALIGNTOP和 WVR_ALIGNLEFT 值。
               WVR_ALIGNRIGHT
               0x0080
                指定保留窗口的工作区，并与窗口新位置的右侧对齐。 例如，若要将工作区与右下角对齐，请返回 WVR_ALIGNRIGHT 和WVR_ALIGNBOTTOM值。
               WVR_ALIGNLEFT
               0x0020
                指定保留窗口的工作区，并与窗口新位置的左侧对齐。 例如，若要将工作区与左下角对齐，请返回 WVR_ALIGNLEFT 和 WVR_ALIGNBOTTOM 值。
               WVR_ALIGNBOTTOM
               0x0040
                指定保留窗口的工作区，并与窗口新位置的底部对齐。 例如，若要将工作区与左上角对齐，请返回WVR_ALIGNTOP和 WVR_ALIGNLEFT 值。
               WVR_HREDRAW
               0x0100
                与除 WVR_VALIDRECTS以外的任何其他值结合使用，如果客户端矩形水平更改大小，则窗口将完全重新绘制。 此值类似于 CS_HREDRAW 类样式
               WVR_VREDRAW
               0x0200
                如果客户端矩形垂直更改大小，则与其他任何值（ WVR_VALIDRECTS除外）结合使用会导致完全重绘窗口。 此值类似于 CS_VREDRAW 类样式
               WVR_REDRAW
               0x0300
                此值会导致重新绘制整个窗口。 它是 WVR_HREDRAW 和 WVR_VREDRAW 值的组合。
               WVR_VALIDRECTS
               0x0400
                此值指示从WM_NCCALCSIZE返回时，由NCCALCSIZE_PARAMS结构的rgrc[1] 和 rgrc[2] 成员指定的矩形分别包含有效的目标矩形和源区域矩形。 系统将合并这些矩形，以计算要保留的窗口面积。 系统会复制源矩形内窗口图像的任何部分，并将图像剪辑到目标矩形。 这两个矩形都采用父相对坐标或屏幕相对坐标。 此标志不能与任何其他标志组合使用。
                此返回值允许应用程序实现更详细的工作区保留策略，例如居中或保留工作区的子集。*/
                ///  备注
                ///    根据是指定 CS_HREDRAW 还是CS_VREDRAW类样式，可以重新绘制窗口。 这是除上表) 所述的常规客户端矩形计算外， DefWindowProc 函数(对此消息的默认向后兼容处理。
                ///    当 wParam 为 TRUE 时，只需返回 0 而不处理 NCCALCSIZE_PARAMS 矩形将导致工作区调整为窗口大小（包括窗口框架）。 这将删除窗口框架并从窗口中描述文字项，只显示工作区。
                ///    从 Windows Vista 开始，只需在 wParam 为 TRUE 时返回 0 即可删除标准帧，不会影响使用 DwmExtendFrameIntoClientArea 函数扩展到工作区的帧。 只会删除标准帧。

            const NCCALCSIZE_PARAMS& var = *(LPNCCALCSIZE_PARAMS)wParam; // Readonly
            DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
            DBG_BREAK; // TODO: Remove if no changes

            return r;
        }

        if (uMsg == WM_DISPLAYCHANGE)
        {
            UINT bpp = (UINT)wParam;
            UINT width = LOWORD(lParam), height = HIWORD(lParam);
            DBG_BREAK;

            UINT desktop_width = GetSystemMetrics(SM_CXSCREEN), desktop_height = GetSystemMetrics(SM_CYSCREEN);
            DBG_BREAK;

            if (lParam == MAKELONG(desktop_width, desktop_height))
            {
                TCHAR msg_info[250]{};
                wsprintf(msg_info, TEXT("Windows正在切换顶部窗口的分辨率至：%d × %d(桌面)，拦截中\n"), desktop_width, desktop_height);
                DBG_OUTSTR(msg_info);

                return 0; // Intercept
            }

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);

            return r;
        }

        if (uMsg == WM_STYLECHANGING)
        {
            STYLESTRUCT& var = *(LPSTYLESTRUCT)lParam;
            DBG_BREAK;

            if (wParam == GWL_STYLE)
                DBG_BREAK;
            if (wParam == GWL_EXSTYLE)
                DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);

            return r;
        }

        if (uMsg == WM_STYLECHANGED)
        {
            const STYLESTRUCT& var = *(LPSTYLESTRUCT)lParam; // Readonly
            DBG_BREAK;

            if (wParam == GWL_STYLE)
                DBG_BREAK;
            if (wParam == GWL_EXSTYLE)
                DBG_BREAK;

            LRESULT r = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);

            return r;
        }
    }

    return CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
}

// 安装钩子
BOOL InstallHook(const wchar_t* targetProcess) {
    std::tstring targetName(targetProcess);

    // 查找目标窗口
    EnumWindows(EnumWindowsProc, (LPARAM)&targetName);

    if (!targetHwnd) {
        DBG_OUTSTR(TEXT("未找到目标窗口！\n"));
        DBG_OUTSTR(targetProcess);
        return FALSE;
    }

    wchar_t debugMsg[256];
    swprintf_s(debugMsg, TEXT("已找到目标窗口 (句柄: 0x%p)\n"), targetHwnd);
    DBG_OUTSTR(debugMsg);

    // Hook WndProc
    originalWndProc = (WNDPROC)SetWindowLongPtr(targetHwnd, GWL_WNDPROC, (LONG_PTR)HookedWndProc);

    if (!originalWndProc) {
        DBG_OUTSTR(TEXT("无法设置窗口钩子！\n"));
        return FALSE;
    }

    DBG_OUTSTR(TEXT("已安装钩子，目标窗口现在不会因副屏点击而最小化！\n"));
    return TRUE;
}

// 卸载钩子
void UninstallHook() {
    if (originalWndProc) {
        // Restore original WndProc
        SetWindowLongPtr(targetHwnd, GWL_WNDPROC, (LONG_PTR)originalWndProc);
        DBG_OUTSTR(TEXT("已卸载钩子\n"));
    }
}

DWORD WINAPI UnloadProc(PVOID param)
{
    FreeLibrary(g_hModule);
    return 0;
}

void Unload() {
    HANDLE hThread = CreateThread(NULL, 0, UnloadProc, NULL, 0, NULL);
    if (hThread)
        CloseHandle(hThread);
}

extern "C" __declspec(dllexport) void Initialize(void* pData)
{
}

extern "C" __declspec(dllexport)
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
    case 11: // PID
        g_pid = (DWORD)lpReserved;
        break;
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;

        if (!InstallHook(GetProcessName(g_pid).c_str()))
        {
            // Uninstall module if hook installation fails
            MessageBox(NULL, TEXT("Failed to install hook."), TEXT("DYT_Module"), MB_OK | MB_ICONERROR);
            Unload();
        }
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        UninstallHook();
        break;
    }
    return TRUE;
}