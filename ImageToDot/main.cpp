#include <windows.h>
#include <gdiplus.h>
#include "MainWindow.h"

#pragma comment(lib, "gdiplus.lib")

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // 1. GDI+ 초기화
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 2. 메인 윈도우 객체 생성 및 실행
    MainWindow mainWindow(hInstance);

    if (mainWindow.Create(1024, 768))
    {
        mainWindow.Show(nCmdShow);

        // 3. 메시지 루프
        MSG msg = { 0 };
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 4. GDI+ 종료
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}