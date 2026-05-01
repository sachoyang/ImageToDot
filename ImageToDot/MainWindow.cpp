#include "MainWindow.h"
#include <commdlg.h>
#include <stdio.h>
#include <thread>

#pragma comment(lib, "comctl32.lib") // 탭 컨트롤을 그리기 위한 라이브러리
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
    m_showGrid(true), m_isLMousePressed(false), m_isRMousePressed(false), m_isProcessing(false)
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
    case WM_NOTIFY: OnNotify(lParam); return 0; // 탭 클릭 이벤트 수신
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
    // 백그라운드 작업이 끝났다는 신호를 받으면 실행!
    case WM_PROCESS_COMPLETE:
    {
        m_isProcessing = false; // 처리 끝!
        Gdiplus::Bitmap* resultBmp = (Gdiplus::Bitmap*)wParam;

        if (resultBmp) {
            m_pEngine->SetPixelatedImage(resultBmp); // 이제 메인 스레드가 안전하게 이미지를 끼웁니다.
        }

        SetWindowText(m_hBtnMApply, L"Apply OpenCV Filter");
        EnableWindow(m_hBtnMApply, TRUE);
        InvalidateRect(m_hwnd, NULL, FALSE);
        SetFocus(m_hwnd);
        return 0;
    }
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MainWindow::OnCreate() {

    // 공용 컨트롤(탭 등) 초기화
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    // 1. 탭 컨트롤 생성
    m_hTab = CreateWindow(WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 0, 0, m_hwnd, (HMENU)IDC_TAB_MAIN, m_hInstance, NULL);

    // 탭 아이템 추가 (0: Retro, 1: Monopro)
    TCITEM tie = { 0 };
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"Retro Mode";
    TabCtrl_InsertItem(m_hTab, 0, &tie);
    tie.pszText = (LPWSTR)L"Monopro Mode";
    TabCtrl_InsertItem(m_hTab, 1, &tie);
    m_currentTab = 0;

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
    const wchar_t* palList[] = { L"No Palette", L"1-Bit (B&W)", L"GameBoy", L"Pico-8", L"Auto (K-Means 8)" };
    for (int i = 0; i < 5; ++i) SendMessage(m_hComboPalette, CB_ADDSTRING, 0, (LPARAM)palList[i]);
    SendMessage(m_hComboPalette, CB_SETCURSEL, 0, 0);

    m_hChkGrid = CreateWindow(L"BUTTON", L"Show Grid", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_CHK_GRID, m_hInstance, NULL);
    SendMessage(m_hChkGrid, BM_SETCHECK, BST_CHECKED, 0);

    m_hChkDithering = CreateWindow(L"BUTTON", L"Enable Dithering", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 130, 30, m_hwnd, (HMENU)IDC_CHK_DITHERING, m_hInstance, NULL);

    // --- [NEW] Monopro UI 생성 코드 ---
    m_hComboMPixel = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 130, 200, m_hwnd, (HMENU)IDC_COMBO_M_PIXEL, m_hInstance, NULL);
    const wchar_t* mPixList[] = { L"Pixel Size: 2", L"Pixel Size: 4", L"Pixel Size: 8", L"Pixel Size: 16" };
    for (int i = 0; i < 4; ++i) SendMessage(m_hComboMPixel, CB_ADDSTRING, 0, (LPARAM)mPixList[i]);
    SendMessage(m_hComboMPixel, CB_SETCURSEL, 1, 0); // 기본 4

    m_hComboMColor = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 130, 200, m_hwnd, (HMENU)IDC_COMBO_M_COLOR, m_hInstance, NULL);
    const wchar_t* mColList[] = { L"Colors: 2", L"Colors: 4", L"Colors: 8", L"Colors: 16", L"Colors: 32" };
    for (int i = 0; i < 5; ++i) SendMessage(m_hComboMColor, CB_ADDSTRING, 0, (LPARAM)mColList[i]);
    SendMessage(m_hComboMColor, CB_SETCURSEL, 2, 0); // 기본 8색

    m_hComboMBlur = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 130, 200, m_hwnd, (HMENU)IDC_COMBO_M_BLUR, m_hInstance, NULL);
    const wchar_t* mBlurList[] = { L"Blur: Off", L"Blur: Weak (5)", L"Blur: Mid (15)", L"Blur: Strong (30)" };
    for (int i = 0; i < 4; ++i) SendMessage(m_hComboMBlur, CB_ADDSTRING, 0, (LPARAM)mBlurList[i]);
    SendMessage(m_hComboMBlur, CB_SETCURSEL, 2, 0); // 기본 15

    m_hBtnMApply = CreateWindow(L"BUTTON", L"Apply OpenCV Filter", WS_CHILD | BS_PUSHBUTTON, 0, 0, 130, 40, m_hwnd, (HMENU)IDC_BTN_M_APPLY, m_hInstance, NULL);

    // 초기 화면 세팅 (Retro 모드 UI만 켜기)
    UpdateUIState();
}

void MainWindow::OnSize(int width, int height) {
    int uiWidth = 150;
    int uiX = width - uiWidth;

    // 1. 공통 버튼 (Open, Save, Color)은 탭 밖에 항상 보이게 배치
    MoveWindow(m_hBtnOpen, uiX + 10, 20, 130, 30, TRUE);
    MoveWindow(m_hBtnSave, uiX + 10, 60, 130, 30, TRUE);
    MoveWindow(m_hBtnColor, uiX + 10, 100, 130, 30, TRUE);

    // 2. 탭 컨트롤 배경 배치 (버튼들 아래부터 화면 끝까지)
    MoveWindow(m_hTab, uiX, 150, uiWidth, height - 150, TRUE);

    // 3. 탭 내부 UI 배치 좌표 (상대 좌표가 아닌 절대 좌표로 계산)
    int tabInnerX = uiX + 10;
    int tabInnerY = 190; // 탭 이름 텍스트보다 살짝 아래

    // Retro UI 
    MoveWindow(m_hComboRes, tabInnerX, tabInnerY, 130, 200, TRUE);
    MoveWindow(m_hChkInterpolation, tabInnerX, tabInnerY + 40, 130, 30, TRUE);
    MoveWindow(m_hComboPalette, tabInnerX, tabInnerY + 80, 130, 200, TRUE);
    MoveWindow(m_hChkGrid, tabInnerX, tabInnerY + 120, 130, 30, TRUE);
    MoveWindow(m_hChkDithering, tabInnerX, tabInnerY + 160, 130, 30, TRUE);

    // Monopro UI
    MoveWindow(m_hComboMPixel, tabInnerX, tabInnerY, 130, 200, TRUE);
    MoveWindow(m_hComboMColor, tabInnerX, tabInnerY + 40, 130, 200, TRUE);
    MoveWindow(m_hComboMBlur, tabInnerX, tabInnerY + 80, 130, 200, TRUE);
    MoveWindow(m_hBtnMApply, tabInnerX, tabInnerY + 130, 130, 40, TRUE);
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

    case IDC_BTN_M_APPLY:
    {
        m_isProcessing = true; // 처리 시작!
        EnableWindow(m_hBtnMApply, FALSE);
        SetWindowText(m_hBtnMApply, L"Processing... Wait!");
        InvalidateRect(m_hwnd, NULL, FALSE); // 강제로 화면을 갱신해 로딩 화면 띄우기!

        int pIdx = (int)SendMessage(m_hComboMPixel, CB_GETCURSEL, 0, 0);
        int cIdx = (int)SendMessage(m_hComboMColor, CB_GETCURSEL, 0, 0);
        int bIdx = (int)SendMessage(m_hComboMBlur, CB_GETCURSEL, 0, 0);

        int pixelSizes[] = { 2, 4, 8, 16 };
        int colorCounts[] = { 2, 4, 8, 16, 32 };
        int blurStrengths[] = { 0, 5, 15, 30 };

        int pixelSize = pixelSizes[pIdx];
        int colorCount = colorCounts[cIdx];
        int blurStrength = blurStrengths[bIdx];

        std::thread([this, pixelSize, colorCount, blurStrength]() {
            // 결과를 받아서
            Gdiplus::Bitmap* resultBmp = m_pEngine->CreateMonoproImage(pixelSize, colorCount, blurStrength);
            // 메인 스레드에 소포(포인터)로 전달!
            PostMessage(m_hwnd, WM_PROCESS_COMPLETE, (WPARAM)resultBmp, 0);
            }).detach();

        break;
    }
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

        if (m_isProcessing) {
            SolidBrush overlayBrush(Color(200, 0, 0, 0)); // 진한 반투명 검정색 막
            graphics.FillRectangle(&overlayBrush, 50, 50, drawSize, drawSize);

            FontFamily fontFamily(L"Arial");
            Font font(&fontFamily, 36, FontStyleBold, UnitPixel);
            SolidBrush textBrush(Color(255, 255, 200, 0)); // 경고성 노란색 글씨

            graphics.DrawString(L"PROCESSING IMAGE...", -1, &font, PointF(50.0f + drawSize / 2.0f - 180.0f, 50.0f + drawSize / 2.0f), &textBrush);
        }

        SolidBrush currentBrush(m_currentColor);
        Pen whitePen(Color(255, 255, 255, 255), 2);

        // 버튼 시작 X좌표가 (width - 140)이므로, 넉넉하게 (width - 210)으로 뺍니다.
        int colorBoxX = width - 210;
        int colorBoxY = 20; // Open 버튼과 같은 높이로 정렬

        graphics.FillRectangle(&currentBrush, colorBoxX, colorBoxY, 50, 50);
        graphics.DrawRectangle(&whitePen, colorBoxX, colorBoxY, 50, 50);
    }
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    DeleteObject(memBitmap); DeleteDC(memDC); EndPaint(m_hwnd, &ps);
}

void MainWindow::OnMouseMove(int mouseX, int mouseY) {
    // [MODIFIED] 처리 중이거나 이미지가 없으면 마우스 입력을 아예 차단해버립니다.
    if (m_isProcessing || !m_pEngine->GetPixelatedImage()) return;
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

// 탭 스위칭 로직
void MainWindow::OnNotify(LPARAM lParam) {
    LPNMHDR pnmh = (LPNMHDR)lParam;
    if (pnmh->hwndFrom == m_hTab && pnmh->code == TCN_SELCHANGE) {
        m_currentTab = TabCtrl_GetCurSel(m_hTab);
        UpdateUIState();
    }
}

// 현재 탭에 맞춰서 UI들을 숨기거나 보여주는 마법의 함수
void MainWindow::UpdateUIState() {
    int showRetro = (m_currentTab == 0) ? SW_SHOW : SW_HIDE;
    int showMono = (m_currentTab == 1) ? SW_SHOW : SW_HIDE;

    // Retro 숨기기/보이기
    ShowWindow(m_hComboRes, showRetro);
    ShowWindow(m_hChkInterpolation, showRetro);
    ShowWindow(m_hComboPalette, showRetro);
    ShowWindow(m_hChkGrid, showRetro);
    ShowWindow(m_hChkDithering, showRetro);

    // Monopro 숨기기/보이기
    ShowWindow(m_hComboMPixel, showMono);
    ShowWindow(m_hComboMColor, showMono);
    ShowWindow(m_hComboMBlur, showMono);
    ShowWindow(m_hBtnMApply, showMono);
}