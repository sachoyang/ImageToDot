#pragma once
#include <windows.h>
#include <gdiplus.h>
#include "PixelEngine.h"

// UI 컨트롤 ID 정의
#define IDC_BTN_OPEN   1001
#define IDC_BTN_SAVE   1002
#define IDC_BTN_COLOR  1003
#define IDC_COMBO_RES  1004
#define IDC_CHK_INTERPOLATION 1005
#define IDC_COMBO_PALETTE 1006
#define IDC_CHK_GRID 1007
#define IDC_CHK_DITHERING 1008

class MainWindow
{
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();

    bool Create(int width, int height);
    void Show(int nCmdShow);
    HWND GetHWND() const { return m_hwnd; }

private:
    // [핵심] Win32 C++ 클래스 래핑을 위한 정적 콜백 함수
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 실제 객체 내부에서 처리될 프로시저
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 이벤트 핸들러 분리
    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(int wmId, int wmEvent);
    void OnPaint();
    void OnKeyDown(WPARAM key);
    void OnMouseMove(int mouseX, int mouseY);

    // UI 동작
    void OpenImageDialog();
    void SaveImageDialog();
    void OpenColorPicker();

private:
    HINSTANCE m_hInstance;
    HWND m_hwnd;

    // 핵심 그래픽 엔진 인스턴스
    PixelEngine* m_pEngine;

    // UI 컨트롤 핸들
    HWND m_hBtnOpen, m_hBtnSave, m_hBtnColor, m_hComboRes;
    HWND m_hChkInterpolation, m_hComboPalette, m_hChkGrid, m_hChkDithering;

    // UI 및 에디터 상태
    Gdiplus::Color m_currentColor;
    COLORREF m_customColors[16];
    bool m_showGrid;
    bool m_isLMousePressed;
    bool m_isRMousePressed;
};