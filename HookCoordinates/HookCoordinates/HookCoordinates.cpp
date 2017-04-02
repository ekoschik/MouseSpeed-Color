
#include "stdafx.h"
#include <Windows.h>
#include <Windowsx.h>
#include <cmath>

HHOOK hMouseHook;
DWORD threadID;
HWND hwnd;

const double goal = 300; // 'top' average distance per mouse move

POINT ptPrev = {};
double distance = 0;
double maxDelta = 0;
int steps = 0;
int averageSpeed = 0;

COLORREF Interpolate(int percentage)
{
    // Magic voodoo code adapted from something I found on the internet

    static const COLORREF rgbMin = RGB(0, 0, 255);       // blue
    static const COLORREF rgbMiddle = RGB(255, 255, 0);  // yellow
    static const COLORREF rgbMax = RGB(255, 0, 0);       // red

    COLORREF c1 = (percentage < 50) ? rgbMin : rgbMiddle;
    COLORREF c2 = (percentage < 50) ? rgbMiddle : rgbMax;
    double fraction = (percentage < 50) ?
        percentage / 50.0 : (percentage - 50) / 50.0;

#define CLRMATH(d1,d2,f) (int)(d1 + (d2 - d1) * (double)f)
    return RGB(CLRMATH((double)GetRValue(c1), (double)GetRValue(c2), fraction),
               CLRMATH((double)GetGValue(c1), (double)GetGValue(c2), fraction), 
               CLRMATH((double)GetBValue(c1), (double)GetBValue(c2), fraction));

    // http://stackoverflow.com/questions/6394304/algorithm-how-do-i-fade-from-red-to-green-via-yellow-using-rgb-values
}

void Draw(HDC hdc, HWND hwnd)
{
    RECT rcClient = {};
    GetClientRect(hwnd, &rcClient);

    // Calculate the average speed as a percentage of the goal
    int percentage = max(0, min(100,
        (int)(((double)averageSpeed / goal) * 100)));

    // Fill the client area with 'the Interpolate color'
    HBRUSH hbr = CreateSolidBrush(Interpolate(percentage));
    FillRect(hdc, &rcClient, hbr);
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

    int cx = 400, cy = 500;
    hwnd = CreateWindowEx(0,
        szWndClass,
        szWndTitle,
        WS_OVERLAPPEDWINDOW /*^ WS_THICKFRAME*/,
        CW_USEDEFAULT, CW_USEDEFAULT, cx, cy,
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
    printf("worker thread exiting.\n");
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

    printf("exiting.\n");
    return 0;
}

