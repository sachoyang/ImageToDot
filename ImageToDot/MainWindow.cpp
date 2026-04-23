#include "MainWindow.h"
#include <commdlg.h>
#include <stdio.h>

using namespace Gdiplus;

// [핵심] C 스타일의 콜백을 C++ 클래스 메서드로 연결해주는 마법의 래퍼 함수
LRESULT CALLBACK MainWindow::StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    }
    else {
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) return pThis->HandleMessage(uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

MainWindow::MainWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwnd(NULL), m_currentColor(Color(255, 255, 50, 50)),
    m_showGrid(true), m_isLMousePressed(false), m_isRMousePressed(false)
{
    ZeroMemory(m_customColors, sizeof(m_customColors));
    m_pEngine = new PixelEngine(); // 엔진 객체 생성
}

MainWindow::~MainWindow() {
    delete m_pEngine;
}

bool MainWindow::Create(int width, int height) {
    const wchar_t CLASS_NAME[] = L"DotConverterClass";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = MainWindow::StaticWndProc; // 정적 콜백 등록
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Pixel Art Converter Pro (Refactored)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, m_hInstance, this // 마지막 인자로 this 포인터 전달!
    );
    return (hwnd != NULL);
}

void MainWindow::Show(int nCmdShow) { ShowWindow(m_hwnd, nCmdShow); }

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: OnCreate(); return 0;
    case WM_SIZE: OnSize(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_COMMAND: OnCommand(LOWORD(wParam), HIWORD(wParam)); return 0;
    case WM_PAINT: OnPaint(); return 0;
    case WM_ERASEBKGND: return 1; // 깜빡임 방지
    case WM_KEYDOWN: OnKeyDown(wParam); return 0;
    case WM_LBUTTONDOWN: m_isLMousePressed = true; OnMouseMove(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_RBUTTONDOWN: m_isRMousePressed = true; OnMouseMove(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_MOUSEMOVE:
        if (m_isLMousePressed || m_isRMousePressed) OnMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_LBUTTONUP: m_isLMousePressed = false; return 0;
    case WM_RBUTTONUP: m_isRMousePressed = false; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MainWindow::OnCreate() {
    m_hBtnOpen = CreateWindow(L"BUTTON", L"Open Image (O)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_BTN_OPEN, m_hInstance, NULL);
    m_hBtnSave = CreateWindow(L"BUTTON", L"Export PNG (S)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_BTN_SAVE, m_hInstance, NULL);
    m_hBtnColor = CreateWindow(L"BUTTON", L"Color Picker (C)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_BTN_COLOR, m_hInstance, NULL);

    m_hComboRes = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 130, 200, m_hwnd, (HMENU)IDC_COMBO_RES, m_hInstance, NULL);
    const wchar_t* resList[] = { L"8 x 8", L"16 x 16", L"24 x 24", L"32 x 32", L"64 x 64", L"128 x 128" };
    for (int i = 0; i < 6; ++i) SendMessage(m_hComboRes, CB_ADDSTRING, 0, (LPARAM)resList[i]);
    SendMessage(m_hComboRes, CB_SETCURSEL, 3, 0); // 32x32 기본

    m_hChkInterpolation = CreateWindow(L"BUTTON", L"Sharp Sampling", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_CHK_INTERPOLATION, m_hInstance, NULL);
    SendMessage(m_hChkInterpolation, BM_SETCHECK, BST_CHECKED, 0);

    m_hComboPalette = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 130, 200, m_hwnd, (HMENU)IDC_COMBO_PALETTE, m_hInstance, NULL);
    const wchar_t* palList[] = { L"No Palette", L"1-Bit (B&W)", L"GameBoy", L"Pico-8" };
    for (int i = 0; i < 4; ++i) SendMessage(m_hComboPalette, CB_ADDSTRING, 0, (LPARAM)palList[i]);
    SendMessage(m_hComboPalette, CB_SETCURSEL, 0, 0);

    m_hChkGrid = CreateWindow(L"BUTTON", L"Show Grid", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_CHK_GRID, m_hInstance, NULL);
    SendMessage(m_hChkGrid, BM_SETCHECK, BST_CHECKED, 0);

    m_hChkDithering = CreateWindow(L"BUTTON", L"Enable Dithering", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_CHK_DITHERING, m_hInstance, NULL);
}

void MainWindow::OnSize(int width, int height) {
    int uiX = width - 150, uiY = 120;
    MoveWindow(m_hBtnOpen, uiX, uiY, 130, 30, TRUE);
    MoveWindow(m_hBtnSave, uiX, uiY + 40, 130, 30, TRUE);
    MoveWindow(m_hBtnColor, uiX, uiY + 80, 130, 30, TRUE);
    MoveWindow(m_hComboRes, uiX, uiY + 120, 130, 200, TRUE);
    MoveWindow(m_hChkInterpolation, uiX, uiY + 160, 130, 30, TRUE);
    MoveWindow(m_hComboPalette, uiX, uiY + 200, 130, 200, TRUE);
    MoveWindow(m_hChkGrid, uiX, uiY + 240, 130, 30, TRUE);
    MoveWindow(m_hChkDithering, uiX, uiY + 280, 130, 30, TRUE);
}

void MainWindow::OnCommand(int wmId, int wmEvent) {
    const int resOptions[] = { 8, 16, 24, 32, 64, 128 };
    switch (wmId) {
    case IDC_BTN_OPEN:
        OpenImageDialog();
        SetFocus(m_hwnd); // 작업 완료 후 포커스 복귀
        break;
    case IDC_BTN_SAVE:
        SaveImageDialog();
        SetFocus(m_hwnd);
        break;
    case IDC_BTN_COLOR:
        OpenColorPicker();
        SetFocus(m_hwnd);
        break;
    case IDC_COMBO_RES:
        // 선택이 완전히 끝났을 때만 처리하고 포커스를 가져옵니다.
        if (wmEvent == CBN_SELCHANGE) {
            int idx = (int)SendMessage(m_hComboRes, CB_GETCURSEL, 0, 0);
            m_pEngine->SetTargetResolution(resOptions[idx]);
            m_pEngine->UpdateCanvasFromOriginal();
            m_pEngine->ApplyPaletteToCanvas();
            InvalidateRect(m_hwnd, NULL, FALSE);
            SetFocus(m_hwnd);
        }
        break;
    case IDC_CHK_INTERPOLATION:
        m_pEngine->SetInterpolation(SendMessage(m_hChkInterpolation, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_pEngine->UpdateCanvasFromOriginal();
        m_pEngine->ApplyPaletteToCanvas();
        InvalidateRect(m_hwnd, NULL, FALSE);
        SetFocus(m_hwnd);
        break;
    case IDC_COMBO_PALETTE:
        if (wmEvent == CBN_SELCHANGE) {
            m_pEngine->SetPaletteIndex((int)SendMessage(m_hComboPalette, CB_GETCURSEL, 0, 0));
            m_pEngine->ApplyPaletteToCanvas();
            InvalidateRect(m_hwnd, NULL, FALSE);
            SetFocus(m_hwnd);
        }
        break;
    case IDC_CHK_GRID:
        m_showGrid = (SendMessage(m_hChkGrid, BM_GETCHECK, 0, 0) == BST_CHECKED);
        InvalidateRect(m_hwnd, NULL, FALSE);
        SetFocus(m_hwnd);
        break;
    case IDC_CHK_DITHERING:
        m_pEngine->SetDithering(SendMessage(m_hChkDithering, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_pEngine->ApplyPaletteToCanvas();
        InvalidateRect(m_hwnd, NULL, FALSE);
        SetFocus(m_hwnd);
        break;
    }
    // 여기에 있던 무조건적인 SetFocus(m_hwnd); 는 삭제했습니다!
}

void MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    RECT rc; GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left, height = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(memDC, memBitmap);
    Graphics graphics(memDC);
    graphics.Clear(Color(255, 30, 30, 30));

    FontFamily fontFamily(L"Arial");
    Font font(&fontFamily, 16, FontStyleBold, UnitPixel);
    SolidBrush textBrush(Color(255, 220, 220, 220));
    WCHAR resText[128];
    int curRes = m_pEngine->GetTargetResolution();
    swprintf_s(resText, L"Current Resolution: %d x %d   [ ↑ / ↓ keys to change ]", curRes, curRes);
    graphics.DrawString(resText, -1, &font, PointF(50.0f, 20.0f), &textBrush);

    if (m_pEngine->GetPixelatedImage()) {
        int drawSize = (width < height ? width : height) - 100;
        float step = (float)drawSize / curRes;

        SolidBrush grayBrush(Color(255, 100, 100, 100)), darkBrush(Color(255, 60, 60, 60));
        for (int y = 0; y < curRes; ++y) {
            for (int x = 0; x < curRes; ++x) {
                graphics.FillRectangle(((x + y) % 2 == 0) ? &grayBrush : &darkBrush, (int)(50 + x * step), (int)(50 + y * step), (int)step + 1, (int)step + 1);
            }
        }

        graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
        graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
        graphics.DrawImage(m_pEngine->GetPixelatedImage(), 50, 50, drawSize, drawSize);

        if (m_showGrid) {
            Pen gridPen(Color(100, 255, 255, 255), 1);
            for (int i = 0; i <= curRes; ++i) {
                graphics.DrawLine(&gridPen, (int)(50 + i * step), 50, (int)(50 + i * step), 50 + drawSize);
                graphics.DrawLine(&gridPen, 50, (int)(50 + i * step), 50 + drawSize, (int)(50 + i * step));
            }
        }

        SolidBrush currentBrush(m_currentColor);
        Pen whitePen(Color(255, 255, 255, 255), 2);
        graphics.FillRectangle(&currentBrush, width - 80, 50, 50, 50);
        graphics.DrawRectangle(&whitePen, width - 80, 50, 50, 50);
    }
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    DeleteObject(memBitmap); DeleteDC(memDC); EndPaint(m_hwnd, &ps);
}

void MainWindow::OnMouseMove(int mouseX, int mouseY) {
    if (!m_pEngine->GetPixelatedImage()) return;
    RECT rc; GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left, height = rc.bottom - rc.top;
    int drawSize = (width < height ? width : height) - 100;
    int relX = mouseX - 50, relY = mouseY - 50;

    if (relX >= 0 && relX < drawSize && relY >= 0 && relY < drawSize) {
        int curRes = m_pEngine->GetTargetResolution();
        int gridX = (relX * curRes) / drawSize;
        int gridY = (relY * curRes) / drawSize;
        m_pEngine->DrawPixel(gridX, gridY, m_currentColor, m_isRMousePressed);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnKeyDown(WPARAM key) {
    if (key == 'O') { OpenImageDialog(); }
    else if (key == 'S') { SaveImageDialog(); }
    else if (key == 'C') { OpenColorPicker(); }
    else if (key == VK_UP || key == VK_DOWN) {
        int idx = SendMessage(m_hComboRes, CB_GETCURSEL, 0, 0);
        if (key == VK_UP && idx < 5) idx++;
        else if (key == VK_DOWN && idx > 0) idx--;
        SendMessage(m_hComboRes, CB_SETCURSEL, idx, 0);
        const int resOptions[] = { 8, 16, 24, 32, 64, 128 };
        m_pEngine->SetTargetResolution(resOptions[idx]);
        m_pEngine->UpdateCanvasFromOriginal();
        m_pEngine->ApplyPaletteToCanvas();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OpenImageDialog() {
    WCHAR szFile[MAX_PATH] = { 0 }; OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.bmp\0"; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileName(&ofn)) {
        m_pEngine->LoadImageFile(szFile);
        m_pEngine->UpdateCanvasFromOriginal();
        m_pEngine->ApplyPaletteToCanvas();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::SaveImageDialog() {
    WCHAR szFile[MAX_PATH] = L"Export.png"; OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"PNG\0*.png\0"; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT; ofn.lpstrDefExt = L"png";
    if (GetSaveFileName(&ofn)) m_pEngine->SaveImageFile(szFile);
}

void MainWindow::OpenColorPicker() {
    CHOOSECOLOR cc = { 0 }; cc.lStructSize = sizeof(cc); cc.hwndOwner = m_hwnd;
    cc.rgbResult = RGB(m_currentColor.GetR(), m_currentColor.GetG(), m_currentColor.GetB());
    cc.lpCustColors = m_customColors; cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) {
        m_currentColor = Color(255, GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}