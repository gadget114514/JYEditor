#include <windows.h>
#include "EditorWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    EditorWindow window;
    if (!window.Create(L"JYEditor", WS_OVERLAPPEDWINDOW)) {
        return 0;
    }

    ShowWindow(window.Window(), nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
