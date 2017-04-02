
#include "stdafx.h"
#include <Windows.h>
#include <Windowsx.h>
#include <cmath>
#include <float.h>

HINSTANCE hInst;
HHOOK hMouseHook;
DWORD threadID;

// Window info
HWND hwnd;
int defCX = 600, defCY = 400;
int defX = 0, defY = 0;

// Define color range
const double goal = 300; // 'top' speed (100% speed)
COLORREF rgbMin = RGB(0, 0, 255);       // blue (slowest)
COLORREF rgbMiddle = RGB(255, 255, 0);  // yellow
COLORREF rgbMax = RGB(255, 0, 0);       // red (fastest)

// Bookkeeping
POINT ptPrev = {};
long double distance = 0;
double maxDelta = 0;
int steps = 0;
int averageSpeed = 0;

// Position and 'hover state' of close button
RECT rcCloseButton = {};
bool bHoverCloseButton = false;

COLORREF GetAvgSpeedColor(int percentage)
{
    percentage = max(0, min(100, percentage));

    // Condensed down from magic online code... picks a color based on a 1-100 value
#define CLRMATH(d1,d2,f) (int)(d1 + (d2 - d1) * (double)f)
    COLORREF c1 = (percentage < 50) ? rgbMin : rgbMiddle;
    COLORREF c2 = (percentage < 50) ? rgbMiddle : rgbMax;
    double fraction = (percentage < 50) ?
        percentage / 50.0 : (percentage - 50) / 50.0;
    return RGB(CLRMATH((double)GetRValue(c1), (double)GetRValue(c2), fraction),
               CLRMATH((double)GetGValue(c1), (double)GetGValue(c2), fraction),
               CLRMATH((double)GetBValue(c1), (double)GetBValue(c2), fraction));
}

void Draw(HDC hdc, HWND hwnd)
{
    RECT rcClient = {};
    GetClientRect(hwnd, &rcClient);

    // Calculate the current average speed as a percentage of the goal
    int percentage = (int)(((double)averageSpeed / goal) * 100);

    // Fill the client area with 'avg speed color'
    HBRUSH hbr = CreateSolidBrush(GetAvgSpeedColor(percentage));
    FillRect(hdc, &rcClient, hbr);

    WCHAR buf[100];
#define STR (LPWSTR)&buf
#define PRINT_TEXT(prc, DT_flags, ...) \
    wsprintf(STR, __VA_ARGS__); \
    DrawText(hdc, STR, wcslen(STR), prc, DT_flags);
#define PRINT_CENTER(prc, ...) \
    PRINT_TEXT(prc, DT_CENTER | DT_VCENTER | DT_SINGLELINE, __VA_ARGS__)
#define PRINT_LEFT(prc, ...) PRINT_TEXT(prc, DT_LEFT, __VA_ARGS__)

    SetBkMode(hdc, TRANSPARENT);

#define FONT_NAME   \
    L"Comic Sans MS"
    //L"Courier New"
    //L"Arial Black"
    //L"Consolas"

    static HFONT hfontLarge = CreateFont(
        45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FONT_NAME);
    static HFONT hfontSmall = CreateFont(
        20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FONT_NAME);

    // Large text in the center for percentage

    SelectFont(hdc, hfontLarge);
    RECT rcTxt = rcClient;
    PRINT_CENTER(&rcTxt, L"%i%%", percentage);

    // Small text in bottom left for avg speed and distance

    SelectFont(hdc, hfontSmall);

    int inc = 20;
    rcTxt.top = rcTxt.bottom - (3 * inc);
    rcTxt.left += inc;

    PRINT_LEFT(&rcTxt, L"average speed: %i, top speed: %i",
        averageSpeed, (int)maxDelta);
    rcTxt.top += inc;
    const int distDivBy = 1000;
    PRINT_LEFT(&rcTxt, L"total distance: %i (x%i px)",
        (int)(distance / distDivBy), distDivBy);

    // Close button

    int width = 28, height = 23, textbuf = 8;
    SetRect(&rcTxt, rcClient.right - width,
        rcClient.top + textbuf,
        rcClient.right - textbuf,
        rcClient.top + textbuf + height);
    CopyRect(&rcCloseButton, &rcTxt); // Update rcCloseButton for hit test

    if (bHoverCloseButton) {
        // Pick a number 50 away from the current percentage to get a sufficiently
        // different color for the close button compared to the background.
        HBRUSH hbrCloseHover = CreateSolidBrush(
            GetAvgSpeedColor((percentage + 50) % 100));
        FillRect(hdc, &rcTxt, hbrCloseHover);
    }

    PRINT_CENTER(&rcTxt, L"X");
}

__inline VOID ResizeRectAroundPoint(PRECT prc, UINT cx, UINT cy, POINT pt)
{
    prc->left = pt.x - MulDiv(pt.x - prc->left, cx, prc->right - prc->left);
    prc->top = pt.y - MulDiv(pt.y - prc->top, cy, prc->bottom - prc->top);
    prc->right = prc->left + cx;
    prc->bottom = prc->top + cy;
}

void SWP(HWND hwnd, RECT rc)
{
    SetWindowPos(hwnd, 0,
        rc.left, rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        SWP_SHOWWINDOW);

    InvalidateRect(hwnd, NULL, TRUE);
}

void GrowShrink(int delta, POINT ptCursor, bool control, bool shift)
{
    if (!control && !shift) {
        return;
    }

    RECT rcWindow = {};
    GetWindowRect(hwnd, &rcWindow);

    int cx = rcWindow.right - rcWindow.left;
    int cy = rcWindow.bottom - rcWindow.top;
    if (control) cx += delta;
    if (shift) cy += delta;

    if (PtInRect(&rcWindow, ptCursor)) {
        ResizeRectAroundPoint(&rcWindow, cx, cy, ptCursor);
    }

    SWP(hwnd, rcWindow);
}

bool CheckMoveToMonitorCorner(int x, int y, WINDOWPOS* pwp, POINT ptCursor)
{
    // If the window at this origin and the current size still is under
    // the monitor, adjust the POSCHANGING origin and return true.
    RECT rc = { x, y, x + pwp->cx, y + pwp->cy };
    if (PtInRect(&rc, ptCursor)) {
        pwp->x = rc.left;
        pwp->y = rc.top;
        return true;
    }

    return false;
}

RECT GetWorkArea(HWND hwnd)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcWork;
}

bool bTrackSizeMove = false;

bool AdjustPosChangingRect(WINDOWPOS* pwp, HWND hwnd)
{
    // If shift is down, skip snap to corner
    if (bTrackSizeMove && ((GetKeyState(VK_SHIFT)) >> 15)) {
        return false;
    }

    POINT ptCursor;
    GetCursorPos(&ptCursor);
    RECT rcWork = GetWorkArea(hwnd);

    // Check each corner, stopping if one causes us to snap the window
    return CheckMoveToMonitorCorner(rcWork.left, rcWork.top, pwp, ptCursor) ||
           CheckMoveToMonitorCorner(rcWork.right - pwp->cx, rcWork.top, pwp, ptCursor) ||
           CheckMoveToMonitorCorner(rcWork.right - pwp->cx, rcWork.bottom - pwp->cy, pwp, ptCursor) ||
           CheckMoveToMonitorCorner(rcWork.left, rcWork.bottom - pwp->cy, pwp, ptCursor);
}

void CenterWindow(HWND hwnd)
{
    RECT rcWork = GetWorkArea(hwnd);
    POINT ptCenter = {
        (rcWork.right - rcWork.left) / 2 + rcWork.left,
        (rcWork.bottom - rcWork.top) / 2 + rcWork.top };
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    int cx = rcWindow.right - rcWindow.left;
    int cy = rcWindow.bottom - rcWindow.top;

    RECT rc = {
        ptCenter.x - (cx / 2),
        ptCenter.y - (cy / 2),
        ptCenter.x + (cx / 2),
        ptCenter.y + (cy / 2) };

    SWP(hwnd, rc);
}

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CenterWindow(hwnd);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        Draw(BeginPaint(hwnd, &ps), hwnd);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_NCHITTEST:
    {
        // Get cursor pos in client coordinates
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);

        // Update bHoverCloseButton and repaint if it changed
        bool bHovPrev = bHoverCloseButton;
        bHoverCloseButton = !!PtInRect(&rcCloseButton, pt);
        if (bHoverCloseButton != bHovPrev) {
            InvalidateRect(hwnd, NULL, TRUE);
        }

        // If we're over the client area, and not over the close button,
        // pretend this is the caption area (to let the user drag the window)
        LRESULT res = DefWindowProc(hwnd, message, wParam, lParam);
        if (res == HTCLIENT && !bHoverCloseButton) {
            res = HTCAPTION;
        }

        return res;
    }

    case WM_WINDOWPOSCHANGING:
        if(AdjustPosChangingRect((WINDOWPOS*)lParam, hwnd)) {
            break;
        }

    // Scrolling + control/shift adjust the window's width/height
    case WM_MOUSEWHEEL:
    {
        static int accum = 0;
        accum += GET_WHEEL_DELTA_WPARAM(wParam);
        if (abs(accum) >= WHEEL_DELTA) {
            POINT ptCursor = { GET_X_LPARAM(lParam) , GET_Y_LPARAM(lParam) };
            int speed = 4;
            GrowShrink((accum > 0) ? speed : -speed, ptCursor,
                !!(GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL),
                !!(GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT));
            accum = 0;
        }
        break;
    }
    
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
        bTrackSizeMove = (message == WM_ENTERSIZEMOVE);
        break;
    case WM_LBUTTONUP:
        if (!bHoverCloseButton) {
            break;
        }

        // Button up on the close button, exit.
        __fallthrough;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

bool InitWindow()
{
    LPCWSTR szWndClass = L"class", szWndTitle = L"title";

    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = szWndClass;
    if (!RegisterClassEx(&wcex)) {
        return false;
    }

    hwnd = CreateWindowEx(0,
        szWndClass,
        szWndTitle,
        WS_POPUPWINDOW,
        defX, defY, defCX, defCY,
        NULL, nullptr, hInst, nullptr);

    if (!hwnd) {
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
    long double distPrev = distance;
    if (ptPrev.x != 0 || ptPrev.y != 0) {
        double delta = DIST(ptPrev, pt);
        distance += delta;
        maxDelta = max(maxDelta, delta);
    }
    ptPrev = pt;

    // Incrememnt steps and recompute average speed
    double averagePrev = averageSpeed;
    averageSpeed = (int)(distance / ++steps);

    // Repaint the window if something has changed enough to effect
    // a number displayed somewhere on the window
    if(abs(averageSpeed - averagePrev) >= .01 ||
        abs(distance - distPrev) > 500) {
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

DWORD WINAPI LLMouseHookThread(void*)
{
    threadID = GetCurrentThreadId();

    hMouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, LLMouseHook, hInst, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMouseHook);
    return 0;
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR    /*lpCmdLine*/,
    _In_ int       /*nCmdShow*/)
{
    hInst = hInstance;

    // Start worker thread
    HANDLE hthread = CreateThread(NULL, 0, LLMouseHookThread, NULL, 0, NULL);
    if (!hthread) {
        return 1;
    }

    if (InitWindow()) {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Stop worker thread and wait for it to finish
    PostThreadMessage(threadID, WM_QUIT, 0, 0);
    WaitForSingleObject(hthread, INFINITE);
    
    return 0;
}

