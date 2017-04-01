
#include "stdafx.h"
#include <Windows.h>
#include <cmath>

POINT ptPrev = {};
double distance = 0;
double maxDelta = 0;
double steps = 0;
const double deltaGoal = 1000;

#define DIST(pt0, pt1) \
    sqrt(pow((double)pt1.x - (double)pt0.x, 2) + \
         pow((double)pt1.y - (double)pt0.y, 2))

void MouseMove(POINT pt)
{
    if (ptPrev.x != 0 || ptPrev.y != 0) {
        double delta = DIST(ptPrev, pt);
        maxDelta = max(maxDelta, delta);
        distance += delta;
        printf("mouse input: pt[%i x %i],\tdelta %i\n",
            pt.x, pt.y, (int)delta);

        if (delta > deltaGoal) {
            static double max = 0;
            if (delta > max) {
                printf("%s: %i\n",
                    (max == 0) ? "GOAL!": "NEW FASTEST MOVE",
                    (int)delta);
                max = delta;
            }
        }
    }

    steps++;
    ptPrev = pt;
}

HHOOK hMouseHook;

LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    MOUSEHOOKSTRUCT * pLLhook = (MOUSEHOOKSTRUCT *)lParam;
    if (wParam == WM_LBUTTONDOWN) {
        printf("exiting...\n");
        printf("--> total distance:\t%i\n", (int)distance);
        printf("--> largest delta:\t%i\n", (int)maxDelta);
        printf("--> average delta:\t%i\n", (int)(distance / steps));
        PostQuitMessage(1);
    } else if(wParam == WM_MOUSEMOVE && pLLhook) {
        MouseMove(pLLhook->pt);
    }

    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

int main()
{
    hMouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, mouseProc, GetModuleHandle(NULL), NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMouseHook);
    return 0;
}

