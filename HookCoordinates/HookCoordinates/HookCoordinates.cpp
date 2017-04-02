
#include "stdafx.h"
#include <Windows.h>
#include <cmath>

HWND hwnd;
POINT ptPrev = {};
double distance = 0;
double maxDelta = 0;
const double goal = 500;
int steps = 0;

void TickSteps()
{
    steps++;
    if (steps % 10) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

double GetSpeedPercentage()
{
    int averageSpeed = (int)(distance / steps);
    return (averageSpeed > goal) ? 1 : (double)(averageSpeed / goal);
}

COLORREF rgbMin = RGB(0, 0, 255);
COLORREF rgbMiddle = RGB(255, 255, 0);
COLORREF rgbMax = RGB(255, 0, 0);

double InterpolateHelper(double d1, double d2, double fraction)
{
    return d1 + (d2 - d1) * fraction;
}

COLORREF Interpolate(COLORREF c1, COLORREF c2, double fraction)
{
    double r = InterpolateHelper(GetRValue(c1), GetRValue(c2), fraction);
    double g = InterpolateHelper(GetGValue(c1), GetGValue(c2), fraction);
    double b = InterpolateHelper(GetBValue(c1), GetBValue(c2), fraction);
    
    //printf("Interpolate(fraction: %.2f, returning RGB(%i, %i, %i)\n",
    //    fraction, (int)r, (int)g, (int)b);
    return RGB((int)r, (int)g, (int)b);
}

COLORREF GetPercentageColor()
{
    if (steps == 0) return rgbMin;

    double percentage = GetSpeedPercentage();
    printf("percentage: %.2f... \n", percentage);

    return (percentage < 50) ?
        Interpolate(rgbMax, rgbMiddle, percentage / 50.0) :
        Interpolate(rgbMiddle, rgbMin, (percentage - 50) / 50.0);
}

void Draw(HDC hdc, HWND hwnd)
{
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    HBRUSH hbr = CreateSolidBrush(GetPercentageColor());
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

HHOOK hMouseHook;

LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
#define DIST(pt0, pt1) \
    sqrt(pow((double)pt1.x - (double)pt0.x, 2) + \
    pow((double)pt1.y - (double)pt0.y, 2))

    MOUSEHOOKSTRUCT * pLLhook = (MOUSEHOOKSTRUCT *)lParam;
    if(wParam == WM_MOUSEMOVE && pLLhook) {
        if (ptPrev.x != 0 || ptPrev.y != 0) {
            double delta = DIST(ptPrev, pLLhook->pt);
            distance += delta;
        }
        TickSteps();
        ptPrev = pLLhook->pt;
    }

    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

int main()
{
    HINSTANCE hinst = GetModuleHandle(NULL);
    hMouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, mouseProc, hinst, NULL);
    
    if(InitWindow(hinst)) {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(hMouseHook);
    return 0;
}

