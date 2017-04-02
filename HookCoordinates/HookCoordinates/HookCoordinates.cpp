
#include "stdafx.h"
#include <Windows.h>
#include <Windowsx.h>
#include <cmath>
#include <float.h>

HHOOK hMouseHook;
DWORD threadID;
HWND hwnd;

// Default window size
int defCX = 600, defCY = 400;
int defX = 500, defY = 500;

// Defines color range, and 'top' speed (100% speed)
const double goal = 300;
COLORREF rgbMin = RGB(0, 0, 255);       // blue (slowest)
COLORREF rgbMiddle = RGB(255, 255, 0);  // yellow
COLORREF rgbMax = RGB(255, 0, 0);       // red (fastest)

POINT ptPrev = {};
long double distance = 0;
double maxDelta = 0;
int steps = 0;
int averageSpeed = 0;

void PrintText(HDC hdc, RECT rcClient, int percentage)
{
    WCHAR buf[100];
#define STR (LPWSTR)&buf
#define PRINT_TEXT(prc, DT_flags, ...) \
    wsprintf(STR, __VA_ARGS__); \
    DrawText(hdc, STR, wcslen(STR), prc, DT_flags);
#define PRINT_CENTER(prc, ...) PRINT_TEXT(prc, DT_CENTER | DT_VCENTER | DT_SINGLELINE, __VA_ARGS__)
#define PRINT_LEFT(prc, ...) PRINT_TEXT(prc, DT_LEFT, __VA_ARGS__)

    SetBkMode(hdc, TRANSPARENT);
    //SetTextColor(hdc, RGB(0, 255, 0));

    static HFONT hfontLarge = CreateFont(
        45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"TimesNewRoman");
    static HFONT hfontSmall = CreateFont(
        15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"TimesNewRoman");

    SelectFont(hdc, hfontLarge);
    PRINT_CENTER(&rcClient, L"%i%%", percentage);

    SelectFont(hdc, hfontSmall);
    int inc = 15;
    rcClient.top = rcClient.bottom - (3* inc);
    rcClient.left += inc;

    PRINT_LEFT(&rcClient, L"average speed: %i (/%i)", averageSpeed, (int)goal);
    rcClient.top += inc;
    PRINT_LEFT(&rcClient, L"total distance: %i", (int)distance);
}

COLORREF GetAvgSpeedColor(int percentage)
{
    // Adapted from magic online code...
    COLORREF c1 = (percentage < 50) ? rgbMin : rgbMiddle;
    COLORREF c2 = (percentage < 50) ? rgbMiddle : rgbMax;
    double fraction = (percentage < 50) ?
        percentage / 50.0 : (percentage - 50) / 50.0;

#define CLRMATH(d1,d2,f) (int)(d1 + (d2 - d1) * (double)f)

    return RGB(CLRMATH((double)GetRValue(c1), (double)GetRValue(c2), fraction),
               CLRMATH((double)GetGValue(c1), (double)GetGValue(c2), fraction),
               CLRMATH((double)GetBValue(c1), (double)GetBValue(c2), fraction));
}

void Draw(HDC hdc, HWND hwnd)
{
    RECT rcClient = {};
    GetClientRect(hwnd, &rcClient);

    // Calculate the average speed as a percentage of the goal
    int percentage = (int)(((double)averageSpeed / goal) * 100);

    // Fill the client area with 'avg speed color'
    HBRUSH hbr = CreateSolidBrush(GetAvgSpeedColor(max(0, min(100, percentage))));
    FillRect(hdc, &rcClient, hbr);

    PrintText(hdc, rcClient, percentage);
}

__inline VOID ResizeRectAroundPoint(PRECT prc, UINT cx, UINT cy, POINT pt)
{
    prc->left = pt.x - MulDiv(pt.x - prc->left, cx, prc->right - prc->left);
    prc->top = pt.y - MulDiv(pt.y - prc->top, cy, (prc->bottom - prc->top));
    prc->right = prc->left + cx;
    prc->bottom = prc->top + cy;
}

void GrowShrink(int delta, POINT ptCursor, bool control, bool shift)
{
    RECT rcWindow = {};
    GetWindowRect(hwnd, &rcWindow);

    int cx = rcWindow.right - rcWindow.left;
    int cy = rcWindow.bottom - rcWindow.top;
    if (control) cx += delta;
    if (shift) cy += delta;

    if (PtInRect(&rcWindow, ptCursor)) {
        ResizeRectAroundPoint(&rcWindow, cx, cy, ptCursor);
    }

    SetWindowPos(hwnd, 0,
        rcWindow.left, rcWindow.top,
        rcWindow.right - rcWindow.left,
        rcWindow.bottom - rcWindow.top,
        SWP_FRAMECHANGED);

    InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        Draw(BeginPaint(hwnd, &ps), hwnd);
        EndPaint(hwnd, &ps);
        break;
    }

    // Let the user drag the window from anywhere
    case WM_NCHITTEST:
        return HTCAPTION;

    // Scrolling + control/shift adjust the window's width/height
    case WM_MOUSEWHEEL:
        if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL ||
            GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) {
            static int accum = 0;
            accum += GET_WHEEL_DELTA_WPARAM(wParam);
            if (abs(accum) >= WHEEL_DELTA) {
                POINT ptCursor = { GET_X_LPARAM(lParam) , GET_Y_LPARAM(lParam) };
                int speed = 4;
                GrowShrink((accum > 0) ? speed : -speed, ptCursor,
                    GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL,
                    GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT);
                accum = 0;
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

bool InitWindow(HINSTANCE hinst)
{
    LPCWSTR szWndClass = L"class", szWndTitle = L"title";

    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hinst;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = szWndClass;
    if (!RegisterClassEx(&wcex)) {
        printf("Failed to register window class!\n");
        return false;
    }

    hwnd = CreateWindowEx(0,
        szWndClass,
        szWndTitle,
        WS_POPUPWINDOW,
        500,500, defCX, defCY,
        NULL, nullptr, hinst, nullptr);

    if (!hwnd) {
        printf("Failed to create window!\n");
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

void LLNewPos(POINT pt)
{
#define DIST(pt0, pt1) \
    sqrt(pow((double)pt1.x - (double)pt0.x, 2) + \
    pow((double)pt1.y - (double)pt0.y, 2))

    // Update the total distance traveled
    if (ptPrev.x != 0 || ptPrev.y != 0) {
        double delta = DIST(ptPrev, pt);
        distance += delta;
    }
    ptPrev = pt;

    // Incrememnt steps and recompute average speed
    double averagePrev = averageSpeed;
    averageSpeed = (int)(distance / ++steps);

    // Whenever the average speed changes by .01, repaint the window
    if ((int)(averageSpeed * 100) != (int)(averagePrev * 100)) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

LRESULT CALLBACK LLMouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    MOUSEHOOKSTRUCT * pLLhook = (MOUSEHOOKSTRUCT *)lParam;
    if(wParam == WM_MOUSEMOVE && pLLhook) {
        LLNewPos(pLLhook->pt);
    }

    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

DWORD WINAPI LLMouseHookThread(void* /*data*/)
{
    threadID = GetCurrentThreadId();

    HINSTANCE hinst = GetModuleHandle(NULL);
    hMouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, LLMouseHook, hinst, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMouseHook);
    return 0;
}

int main()
{
    HANDLE hthread = CreateThread(NULL, 0, LLMouseHookThread, NULL, 0, NULL);
    if (!hthread) {
        printf("CreateThread failed.\n");
        return 1;
    }
    
    if (!InitWindow(GetModuleHandle(NULL))) {
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop worker thread and wait for it to finish
    PostThreadMessage(threadID, WM_QUIT, 0, 0);
    WaitForSingleObject(hthread, INFINITE);

    printf("exiting, total distance traveled: %.0Lf px\n", distance);
    return 0;
}

