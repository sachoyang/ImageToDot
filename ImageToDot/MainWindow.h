#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h> // 탭 컨트롤을 위한 공용 컨트롤 헤더
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

// --- 탭 및 Monopro 모드 컨트롤 ID ---
#define IDC_TAB_MAIN       2000
#define IDC_COMBO_M_PIXEL  2001
#define IDC_COMBO_M_COLOR  2002
#define IDC_COMBO_M_BLUR   2003
#define IDC_BTN_M_APPLY    2004

// 백그라운드 연산 완료를 알리는 커스텀 윈도우 메시지
#define WM_PROCESS_COMPLETE (WM_APP + 1)

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

    // 탭 클릭 이벤트를 받을 Notify 함수와 UI 스위칭 함수
    void OnNotify(LPARAM lParam);
    void UpdateUIState();

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

    // --- Tab 및 Monopro UI 핸들 ---
    HWND m_hTab;
    HWND m_hComboMPixel, m_hComboMColor, m_hComboMBlur, m_hBtnMApply;
    int m_currentTab; // 0: Retro, 1: Monopro

    // UI 및 에디터 상태
    Gdiplus::Color m_currentColor;
    COLORREF m_customColors[16];
    bool m_showGrid;
    bool m_isLMousePressed;
    bool m_isRMousePressed;
    bool m_isProcessing; // 연산 중인지 체크하는 깃발
};