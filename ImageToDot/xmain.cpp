#include <windows.h>
#include <stdio.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <commdlg.h> // 파일 열기 대화상자용
#include <math.h>

using namespace Gdiplus;
//==========================================================================//
// 전역 변수
HINSTANCE hInst;
ULONG_PTR gdiplusToken;
// 전역 변수 추가
Gdiplus::Bitmap* g_pOriginalImg = nullptr;
Gdiplus::Bitmap* g_pCanvasImg = nullptr; // 편집 내역이 풀컬러로 보존되는 숨겨진 캔버스
Gdiplus::Bitmap* g_pPixelatedImg = nullptr;
int g_targetRes = 32; // 기본 32x32 도트 해상도

// --- Phase 2: 새로 추가할 변수들 ---
Gdiplus::Color g_currentColor = Color(255, 255, 50, 50); // 현재 선택된 펜 색상 (임시: 빨간색)
bool g_isLMousePressed = false; // 좌클릭 상태
bool g_isRMousePressed = false; // 우클릭 상태

// --- Phase 3: 색상 선택기용 변수 ---
COLORREF g_customColors[16] = { 0 }; // 유저가 커스텀 팔레트에 저장한 색상을 기억하는 배열

// --- Phase 4: UI 컨트롤 ID 및 변수 ---
#define IDC_BTN_OPEN   1001
#define IDC_BTN_SAVE   1002
#define IDC_BTN_COLOR  1003
#define IDC_COMBO_RES  1004

HWND g_hBtnOpen, g_hBtnSave, g_hBtnColor, g_hComboRes;

// 해상도 프리셋 (방향키와 콤보박스를 동기화하기 위함)
const int g_resOptions[] = { 8, 16, 24, 32, 64, 128 };
const int g_numResOptions = 6;
int g_currentResIndex = 3; // 기본값 32x32 (인덱스 3)

#define IDC_CHK_INTERPOLATION 1005

HWND g_hChkInterpolation;
bool g_useNearestNeighbor = true; // true: Nearest(선명), false: Bicubic(부드러움)

// --- Phase 4: UI 컨트롤 및 팔레트 변수 ---
#define IDC_COMBO_PALETTE 1006

HWND g_hComboPalette;
int g_currentPaletteIndex = 0; // 0: 제한 없음(원본), 1: 흑백(B&W), 2: GameBoy, 3: Pico-8

// 유명한 레트로 콘솔 팔레트 프리셋 정의
const Color g_palGameBoy[4] = {
    Color(255, 15, 56, 15),   // 가장 어두운 녹색
    Color(255, 48, 98, 48),
    Color(255, 139, 172, 15),
    Color(255, 155, 188, 15)  // 가장 밝은 녹색
};

// 인디 도트 게임에서 가장 많이 쓰이는 16색 팔레트 (Pico-8)
const Color g_palPico8[16] = {
    Color(255, 0, 0, 0),       Color(255, 29, 43, 83),    Color(255, 126, 37, 83),   Color(255, 0, 135, 81),
    Color(255, 171, 82, 54),   Color(255, 95, 87, 79),    Color(255, 194, 195, 199), Color(255, 255, 241, 232),
    Color(255, 255, 0, 77),    Color(255, 255, 163, 0),   Color(255, 255, 236, 39),  Color(255, 0, 228, 54),
    Color(255, 41, 173, 255),  Color(255, 131, 118, 156), Color(255, 255, 119, 168), Color(255, 255, 204, 170)
};

// --- Phase 4: UI 컨트롤 추가 (그리드) ---
#define IDC_CHK_GRID 1007

HWND g_hChkGrid;
bool g_showGrid = true; // 기본값: 그리드 켜짐

// --- Phase 4: UI 컨트롤 추가 (디더링) ---
#define IDC_CHK_DITHERING 1008

HWND g_hChkDithering;
bool g_useDithering = false; // 기본값: 디더링 꺼짐

//==========================================================================//
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // 1. GDI+ 초기화
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    hInst = hInstance;
    const wchar_t CLASS_NAME[] = L"DotConverterClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    // 배경 브러시를 설정하지 않습니다. (WM_ERASEBKGND에서 처리하여 깜빡임 방지)
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // 윈도우 생성 (다크 테마 느낌을 낼 메인 창)
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Pixel Art Converter Pro (Win32 API)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // 메시지 루프
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // GDI+ 종료
    GdiplusShutdown(gdiplusToken);

    return 0;
}

// 이미지 로드 함수
void LoadImageFile(HWND hwnd) 
{
    WCHAR szFile[MAX_PATH] = { 0 };
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Image Files\0*.png;*.jpg;*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        if (g_pOriginalImg) delete g_pOriginalImg;
        g_pOriginalImg = new Gdiplus::Bitmap(szFile);

        // 픽셀화 실행
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

// GDI+ 인코더 CLSID를 얻는 헬퍼 함수
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;          // 이미지 인코더 수
    UINT size = 0;         // 이미지 인코더 배열의 크기(바이트)

    ImageCodecInfo* pImageCodecInfo = NULL;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// 투명도 유지 및 도트 선명하게 확대하여 저장하는 함수
void SaveImageFile(HWND hwnd)
{
    if (!g_pPixelatedImg) return; // 변환된 이미지가 없으면 무시

    WCHAR szFile[MAX_PATH] = L"MySprite_Export.png"; // 기본 파일명
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    // 게임 엔진용이므로 투명도가 있는 PNG를 기본으로 설정합니다.
    ofn.lpstrFilter = L"PNG Image\0*.png\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"png";

    if (GetSaveFileName(&ofn)) {
        // 1. 확대 배율 지정 (예: 10배 확대, 32x32 -> 320x320)
        int scale = 10;
        int outSize = g_targetRes * scale;

        // 2. 내보낼 고해상도 비트맵 생성 (투명도 지원하는 PixelFormat32bppARGB)
        Gdiplus::Bitmap exportBitmap(outSize, outSize, PixelFormat32bppARGB);
        Graphics g(&exportBitmap);

        // 3. 도트가 깨지지 않게 렌더링 세팅 (이전에 캔버스에 적용했던 것과 동일)
        g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        // 4. 원본 작은 도트를 큰 비트맵에 선명하게 꽉 채워 그리기
        g.DrawImage(g_pPixelatedImg, 0, 0, outSize, outSize);

        // 5. PNG 코덱을 찾아서 파일로 저장
        CLSID pngClsid;
        GetEncoderClsid(L"image/png", &pngClsid);

        if (exportBitmap.Save(szFile, &pngClsid, NULL) == Ok) {
            MessageBox(hwnd, L"스프라이트가 성공적으로 저장되었습니다!", L"저장 완료", MB_OK | MB_ICONINFORMATION);
        }
        else {
            MessageBox(hwnd, L"저장에 실패했습니다.", L"오류", MB_OK | MB_ICONERROR);
        }
    }
}

// [NEW] RGB를 HSV(Hue, Saturation, Value)로 변환하는 헬퍼 함수
void RGBtoHSV(int r, int g, int b, float& h, float& s, float& v) {
    float fR = r / 255.0f;
    float fG = g / 255.0f;
    float fB = b / 255.0f;

    // 매크로 충돌 방지를 위해 수동으로 Min/Max 계산
    float cMax = fR;
    if (fG > cMax) cMax = fG;
    if (fB > cMax) cMax = fB;

    float cMin = fR;
    if (fG < cMin) cMin = fG;
    if (fB < cMin) cMin = fB;

    float delta = cMax - cMin;

    v = cMax;
    if (cMax > 0.0f) s = delta / cMax;
    else s = 0.0f;

    if (delta == 0.0f) h = 0.0f;
    else {
        if (cMax == fR) h = 60.0f * fmodf(((fG - fB) / delta), 6.0f);
        else if (cMax == fG) h = 60.0f * (((fB - fR) / delta) + 2.0f);
        else if (cMax == fB) h = 60.0f * (((fR - fG) / delta) + 4.0f);

        if (h < 0.0f) h += 360.0f;
    }
}

// [MODIFIED] 인간의 시각 인지(HSV)에 맞춘 색상 매칭 알고리즘
Color GetClosestColor(Color target, const Color* palette, int paletteSize)
{
    float minDistance = 99999999.0f;
    Color closestColor = target;

    float h1, s1, v1;
    RGBtoHSV(target.GetR(), target.GetG(), target.GetB(), h1, s1, v1);

    // HSV를 3차원 원뿔(Cone) 좌표계로 변환 (Hue를 각도로 사용)
    const float PI = 3.14159265f;
    float angle1 = h1 * PI / 180.0f;          // rad1 -> angle1 로 변경
    float cx1 = s1 * v1 * cosf(angle1);       // x1 -> cx1 로 변경
    float cy1 = s1 * v1 * sinf(angle1);       // y1 -> cy1 로 변경 (math.h y1() 함수 충돌 방지!)
    float cz1 = v1;                           // z1 -> cz1 로 변경

    for (int i = 0; i < paletteSize; ++i) {
        float h2, s2, v2;
        RGBtoHSV(palette[i].GetR(), palette[i].GetG(), palette[i].GetB(), h2, s2, v2);

        float angle2 = h2 * PI / 180.0f;      // rad2 -> angle2 로 변경
        float cx2 = s2 * v2 * cosf(angle2);
        float cy2 = s2 * v2 * sinf(angle2);   // y2 -> cy2 로 변경
        float cz2 = v2;

        float dx = cx1 - cx2;
        float dy = cy1 - cy2;
        float dz = cz1 - cz2;

        // 명도(z)에 가중치를 살짝 주어 실루엣이 뭉개지지 않게 방지합니다.
        float distance = (dx * dx) + (dy * dy) + (dz * dz) * 1.5f;

        if (distance < minDistance) {
            minDistance = distance;
            closestColor = palette[i];
        }
    }

    // 투명도는 원본 타겟의 알파값을 유지합니다
    return Color(target.GetA(), closestColor.GetR(), closestColor.GetG(), closestColor.GetB());
}
//// 픽셀화 핵심 알고리즘
//void ProcessPixelation()
//{
//    if (!g_pOriginalImg) return;
//
//    if (g_pPixelatedImg) delete g_pPixelatedImg;
//    g_pPixelatedImg = new Gdiplus::Bitmap(g_targetRes, g_targetRes, PixelFormat32bppARGB);
//
//    Graphics g(g_pPixelatedImg);
//
//    // [MODIFIED] 체크박스 상태에 따라 보간법(알고리즘) 스위칭
//    if (g_useNearestNeighbor) 
//    {
//        g.SetInterpolationMode(InterpolationModeNearestNeighbor); // 한 픽셀을 정확히 찝어옴 (거침)
//    }
//    else 
//    {
//        // 주변 픽셀들의 색상을 부드럽게 섞어서 평균값을 가져옴 (부드러움)
//        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
//    }
//
//    g.DrawImage(g_pOriginalImg, 0, 0, g_targetRes, g_targetRes);
//
//    // 팔레트 제한이 선택되어 있다면, 축소된 비트맵의 색상을 치환합니다.
//    if (g_currentPaletteIndex > 0) 
//    {
//        Color pixelColor;
//        for (int y = 0; y < g_targetRes; ++y) 
//        {
//            for (int x = 0; x < g_targetRes; ++x) 
//            {
//                g_pPixelatedImg->GetPixel(x, y, &pixelColor);
//
//                // 알파값이 0(완전 투명)이면 색상 계산을 건너뜁니다.
//                if (pixelColor.GetA() == 0) continue;
//
//                Color newColor = pixelColor;
//
//                if (g_currentPaletteIndex == 1) 
//                { // 1: 흑백 (Black & White 2색)
//                    int gray = (pixelColor.GetR() + pixelColor.GetG() + pixelColor.GetB()) / 3;
//                    newColor = (gray > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
//                }
//                else if (g_currentPaletteIndex == 2) 
//                { // 2: GameBoy (4색)
//                    newColor = GetClosestColor(pixelColor, g_palGameBoy, 4);
//                }
//                else if (g_currentPaletteIndex == 3) 
//                { // 3: Pico-8 (16색)
//                    newColor = GetClosestColor(pixelColor, g_palPico8, 16);
//                }
//
//                g_pPixelatedImg->SetPixel(x, y, newColor);
//            }
//        }
//    }
//}

// 1. 원본 이미지에서 타겟 해상도로 줄인 '풀컬러 캔버스'를 만듭니다. (해상도 변경/파일 열기 때만 호출)
void UpdateCanvasFromOriginal()
{
    if (!g_pOriginalImg) return;
    if (g_pCanvasImg) delete g_pCanvasImg;

    g_pCanvasImg = new Gdiplus::Bitmap(g_targetRes, g_targetRes, PixelFormat32bppARGB);
    Graphics g(g_pCanvasImg);

    if (g_useNearestNeighbor) g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    else g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    g.DrawImage(g_pOriginalImg, 0, 0, g_targetRes, g_targetRes);
}

// 2. '풀컬러 캔버스'를 읽어서 팔레트 제한을 적용한 최종 화면을 만듭니다.
void ApplyPaletteToCanvas()
{
    if (!g_pCanvasImg) return;
    if (g_pPixelatedImg) delete g_pPixelatedImg;

    g_pPixelatedImg = new Gdiplus::Bitmap(g_targetRes, g_targetRes, PixelFormat32bppARGB);
    Color pixelColor;

    for (int y = 0; y < g_targetRes; ++y) {
        for (int x = 0; x < g_targetRes; ++x) {
            g_pCanvasImg->GetPixel(x, y, &pixelColor);

            if (pixelColor.GetA() == 0) {
                g_pPixelatedImg->SetPixel(x, y, Color(0, 0, 0, 0));
                continue;
            }

            Color newColor = pixelColor;

            // 팔레트 제한이 걸려있을 때만 연산합니다.
            if (g_currentPaletteIndex > 0)
            {
                int r = pixelColor.GetR();
                int g = pixelColor.GetG();
                int b = pixelColor.GetB();

                // [NEW] 디더링 알고리즘 (Bayer Matrix)
                if (g_useDithering)
                {
                    static const int bayer[4][4] = {
                        {  0,  8,  2, 10 },
                        { 12,  4, 14,  6 },
                        {  3, 11,  1,  9 },
                        { 15,  7, 13,  5 }
                    };

                    // 0~15 값을 -8 ~ +7 범위로 내리고, 강도(Spread)를 곱합니다.
                    int strength = 4; // 값이 클수록 패턴이 넓게 퍼집니다.
                    int offset = (bayer[y % 4][x % 4] - 8) * strength;

                    // RGB 범위를 벗어나지 않게 자르기(Clamp)
                    r = max(0, min(255, r + offset));
                    g = max(0, min(255, g + offset));
                    b = max(0, min(255, b + offset));
                }

                Color ditheredColor(pixelColor.GetA(), r, g, b);

                if (g_currentPaletteIndex == 1) {
                    int gray = (r + g + b) / 3;
                    newColor = (gray > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
                }
                else if (g_currentPaletteIndex == 2) {
                    newColor = GetClosestColor(ditheredColor, g_palGameBoy, 4);
                }
                else if (g_currentPaletteIndex == 3) {
                    newColor = GetClosestColor(ditheredColor, g_palPico8, 16);
                }
            }

            g_pPixelatedImg->SetPixel(x, y, newColor);
        }
    }
}

void DrawPixelAtMouse(HWND hwnd, int mouseX, int mouseY, bool isEraser)
{
    if (!g_pPixelatedImg) return;

    // 현재 창 크기를 바탕으로 화면에 그려진 캔버스(drawSize) 크기 역추적
    RECT rc; GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int drawSize = (width < height ? width : height) - 100;

    // 1. 마우스 좌표를 캔버스 기준 상대 좌표로 변환 (캔버스가 (50, 50)에서 시작하므로)
    int relX = mouseX - 50;
    int relY = mouseY - 50;

    // 2. 캔버스 영역 안쪽을 클릭했을 때만 작동
    if (relX >= 0 && relX < drawSize && relY >= 0 && relY < drawSize)
    {
        // 3. 화면 좌표를 32x32 비트맵 인덱스(0~31)로 변환 (비례식 사용)
        int gridX = (relX * g_targetRes) / drawSize;
        int gridY = (relY * g_targetRes) / drawSize;

        // 4. 비트맵 픽셀 색상 변경 (지우개면 투명하게, 아니면 현재 색상으로)
        if (isEraser) {
            g_pPixelatedImg->SetPixel(gridX, gridY, Color(0, 0, 0, 0)); // 알파(A)값을 0으로 주어 투명 처리
        }
        else {
            g_pPixelatedImg->SetPixel(gridX, gridY, g_currentColor);
        }

        Color paintColor = isEraser ? Color(0, 0, 0, 0) : g_currentColor;

        // [핵심] 화면(g_pPixelatedImg)에 그리기 전에, 편집 내역 보존용 캔버스에 원본 색상을 먼저 저장합니다.
        if (g_pCanvasImg) g_pCanvasImg->SetPixel(gridX, gridY, paintColor);

        // 방금 찍은 점에 대해서만 현재 팔레트 계산을 해서 화면에 바로 보여줍니다.
        if (g_pPixelatedImg) {
            Color mappedColor = paintColor;
            if (!isEraser && g_currentPaletteIndex > 0)
            {
                int r = paintColor.GetR();
                int g = paintColor.GetG();
                int b = paintColor.GetB();

                // [NEW] 실시간 그리기 디더링
                if (g_useDithering) {
                    static const int bayer[4][4] = {
                        {  0,  8,  2, 10 }, { 12,  4, 14,  6 },
                        {  3, 11,  1,  9 }, { 15,  7, 13,  5 }
                    };
                    int strength = 4;
                    int offset = (bayer[gridY % 4][gridX % 4] - 8) * strength;

                    r = max(0, min(255, r + offset));
                    g = max(0, min(255, g + offset));
                    b = max(0, min(255, b + offset));
                }

                Color ditheredColor(paintColor.GetA(), r, g, b);

                if (g_currentPaletteIndex == 1) {
                    int gray = (r + g + b) / 3;
                    mappedColor = (gray > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
                }
                else if (g_currentPaletteIndex == 2) {
                    mappedColor = GetClosestColor(ditheredColor, g_palGameBoy, 4);
                }
                else if (g_currentPaletteIndex == 3) {
                    mappedColor = GetClosestColor(ditheredColor, g_palPico8, 16);
                }
            }
            g_pPixelatedImg->SetPixel(gridX, gridY, mappedColor);
        }

        // 화면 갱신 요청
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

// 컬러 피커 열기 함수
void OpenColorPicker(HWND hwnd)
{
    CHOOSECOLOR cc = { 0 };
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;

    // Gdiplus::Color(ARGB)를 Win32 API용 COLORREF(BGR)로 변환하여 초기 선택값으로 설정
    cc.rgbResult = RGB(g_currentColor.GetR(), g_currentColor.GetG(), g_currentColor.GetB());
    cc.lpCustColors = g_customColors;
    cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;

    // 윈도우 기본 색상 선택창 열기
    if (ChooseColor(&cc)) {
        // 선택된 COLORREF(BGR)를 다시 Gdiplus::Color(ARGB)로 변환 (알파값은 255 고정)
        g_currentColor = Color(255, GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));

        // 색상이 바뀌었으니 화면(우측 상단 미리보기) 갱신
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 버튼 생성
        g_hBtnOpen = CreateWindow(L"BUTTON", L"Open Image (O)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, hwnd, (HMENU)IDC_BTN_OPEN, hInst, NULL);
        g_hBtnSave = CreateWindow(L"BUTTON", L"Export PNG (S)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, hwnd, (HMENU)IDC_BTN_SAVE, hInst, NULL);
        g_hBtnColor = CreateWindow(L"BUTTON", L"Color Picker (C)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 30, hwnd, (HMENU)IDC_BTN_COLOR, hInst, NULL);

        // 해상도 선택 콤보박스 (드롭다운) 생성
        g_hComboRes = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 130, 200, hwnd, (HMENU)IDC_COMBO_RES, hInst, NULL);

        // 콤보박스에 리스트 추가
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"8 x 8");
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"16 x 16");
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"24 x 24");
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"32 x 32");
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"64 x 64");
        SendMessage(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"128 x 128");

        // 초기 선택값 설정
        SendMessage(g_hComboRes, CB_SETCURSEL, g_currentResIndex, 0);

        g_hChkInterpolation = CreateWindow(L"BUTTON", L"Sharp Sampling",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 130, 30, hwnd, (HMENU)IDC_CHK_INTERPOLATION, hInst, NULL);

        // 체크박스 기본 상태를 '체크됨(Nearest)'으로 설정
        SendMessage(g_hChkInterpolation, BM_SETCHECK, BST_CHECKED, 0);

        // 해상도 체크박스 아래에 팔레트 선택 콤보박스 생성
        g_hComboPalette = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, 130, 200, hwnd, (HMENU)IDC_COMBO_PALETTE, hInst, NULL);

        SendMessage(g_hComboPalette, CB_ADDSTRING, 0, (LPARAM)L"No Palette (Full)");
        SendMessage(g_hComboPalette, CB_ADDSTRING, 0, (LPARAM)L"1-Bit (B&W)");
        SendMessage(g_hComboPalette, CB_ADDSTRING, 0, (LPARAM)L"GameBoy (4 Colors)");
        SendMessage(g_hComboPalette, CB_ADDSTRING, 0, (LPARAM)L"Pico-8 (16 Colors)");
        SendMessage(g_hComboPalette, CB_SETCURSEL, 0, 0);

        // 그리드 On/Off 체크박스 생성
        g_hChkGrid = CreateWindow(L"BUTTON", L"Show Grid",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 130, 30, hwnd, (HMENU)IDC_CHK_GRID, hInst, NULL);

        // 기본 상태를 '체크됨'으로 설정
        SendMessage(g_hChkGrid, BM_SETCHECK, BST_CHECKED, 0);

        // 디더링 On/Off 체크박스 생성
        g_hChkDithering = CreateWindow(L"BUTTON", L"Enable Dithering",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 130, 30, hwnd, (HMENU)IDC_CHK_DITHERING, hInst, NULL);

        return 0;
    }

    case WM_SIZE:
    {
        // 창 크기가 바뀔 때 UI들을 우측 패널 위치로 자동 정렬합니다.
        int width = LOWORD(lParam);
        int uiX = width - 150; // 우측 여백
        int uiY = 120; // 상단 여백 (색상 상자 아래)

        MoveWindow(g_hBtnOpen, uiX, uiY, 130, 30, TRUE);
        MoveWindow(g_hBtnSave, uiX, uiY + 40, 130, 30, TRUE);
        MoveWindow(g_hBtnColor, uiX, uiY + 80, 130, 30, TRUE);
        MoveWindow(g_hComboRes, uiX, uiY + 120, 130, 200, TRUE); // 콤보박스는 드롭다운 길이를 위해 높이를 200으로 줌

        MoveWindow(g_hChkInterpolation, uiX, uiY + 160, 130, 30, TRUE);

        MoveWindow(g_hComboPalette, uiX, uiY + 200, 130, 200, TRUE);

        // 팔레트 콤보박스 아래에 배치 (간격 40 추가)
        MoveWindow(g_hChkGrid, uiX, uiY + 240, 130, 30, TRUE);

        // 그리드 체크박스 아래에 배치 (간격 40 추가)
        MoveWindow(g_hChkDithering, uiX, uiY + 280, 130, 30, TRUE);

        return 0;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId) 
        {
        case IDC_BTN_OPEN:
            LoadImageFile(hwnd);
            //ProcessPixelation();
            UpdateCanvasFromOriginal();
            ApplyPaletteToCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            SetFocus(hwnd); // [핵심] 단축키 유지를 위해 포커스 반환
            break;

        case IDC_BTN_SAVE:
            SaveImageFile(hwnd);
            SetFocus(hwnd);
            break;

        case IDC_BTN_COLOR:
            OpenColorPicker(hwnd);
            SetFocus(hwnd);
            break;

        case IDC_COMBO_RES:
            if (wmEvent == CBN_SELCHANGE) {
                // 콤보박스에서 마우스로 해상도를 골랐을 때의 처리
                g_currentResIndex = SendMessage(g_hComboRes, CB_GETCURSEL, 0, 0);
                g_targetRes = g_resOptions[g_currentResIndex];
                //ProcessPixelation();
                UpdateCanvasFromOriginal();
                ApplyPaletteToCanvas();
                InvalidateRect(hwnd, NULL, FALSE);
                SetFocus(hwnd);
            }
            break;

        case IDC_CHK_INTERPOLATION:
            // 체크박스가 눌렸을 때, 현재 상태를 읽어와서 전역 변수에 저장
            g_useNearestNeighbor = (SendMessage(g_hChkInterpolation, BM_GETCHECK, 0, 0) == BST_CHECKED);

            // 알고리즘이 바뀌었으니 원본 이미지에서 다시 도트 추출
            //ProcessPixelation();
            UpdateCanvasFromOriginal();
            ApplyPaletteToCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            SetFocus(hwnd); // 단축키 유지
            break;

        case IDC_COMBO_PALETTE:
            if (wmEvent == CBN_SELCHANGE) 
            {
                g_currentPaletteIndex = SendMessage(g_hComboPalette, CB_GETCURSEL, 0, 0);
                //ProcessPixelation(); // 팔레트가 바뀌었으니 새로 계산
                //UpdateCanvasFromOriginal(); // 원본에서 다시 가져오지 않고 팔레트만 덧씌워야 하므로 ApplyPaletteToCanvas(); 만 호출합니다!
                ApplyPaletteToCanvas();
                InvalidateRect(hwnd, NULL, FALSE);
                SetFocus(hwnd);
            }
            break;

        case IDC_CHK_GRID:
            // 체크박스 상태를 읽어와서 변수에 저장
            g_showGrid = (SendMessage(g_hChkGrid, BM_GETCHECK, 0, 0) == BST_CHECKED);

            // 화면 갱신 (이미지 변환 로직은 안 건드려도 됨!)
            InvalidateRect(hwnd, NULL, FALSE);
            SetFocus(hwnd); // 단축키 유지
            break;

        case IDC_CHK_DITHERING:
            g_useDithering = (SendMessage(g_hChkDithering, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ApplyPaletteToCanvas(); // 팔레트+디더링 연산 다시 수행
            InvalidateRect(hwnd, NULL, FALSE);
            SetFocus(hwnd);
            break;
        }
        return 0;
    }
        // --- 키보드 입력 처리 (마우스 대신 O키로 이미지 열기) ---
    case WM_KEYDOWN:
        if (wParam == 'O') { // 영어 'O' 키를 누르면
            LoadImageFile(hwnd);
            //ProcessPixelation();
            UpdateCanvasFromOriginal();
            ApplyPaletteToCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == 'C') { // 영어 'C' 키를 누르면 색상 변경
            OpenColorPicker(hwnd);
        }
        else if (wParam == 'S') { // 영어 'S' 키를 누르면 저장
            SaveImageFile(hwnd);
        }
        // 방향키 위(Up)를 누르면 해상도 증가
        else if (wParam == VK_UP) {
            if (g_currentResIndex < g_numResOptions - 1) {
                g_currentResIndex++;
                g_targetRes = g_resOptions[g_currentResIndex];
                SendMessage(g_hComboRes, CB_SETCURSEL, g_currentResIndex, 0); // UI 동기화
                //ProcessPixelation();
                UpdateCanvasFromOriginal();
                ApplyPaletteToCanvas();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        // 방향키 아래(Down)를 누르면 해상도 감소
        else if (wParam == VK_DOWN) {
            if (g_currentResIndex > 0) {
                g_currentResIndex--;
                g_targetRes = g_resOptions[g_currentResIndex];
                SendMessage(g_hComboRes, CB_SETCURSEL, g_currentResIndex, 0); // UI 동기화
                //ProcessPixelation();
                UpdateCanvasFromOriginal();
                ApplyPaletteToCanvas();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;

        // --- 마우스 그리기 상호작용 ---
    case WM_LBUTTONDOWN: // 좌클릭 (그리기)
        g_isLMousePressed = true;
        DrawPixelAtMouse(hwnd, LOWORD(lParam), HIWORD(lParam), false);
        return 0;

    case WM_RBUTTONDOWN: // 우클릭 (지우개/투명화)
        g_isRMousePressed = true;
        DrawPixelAtMouse(hwnd, LOWORD(lParam), HIWORD(lParam), true);
        return 0;

    case WM_MOUSEMOVE: // 마우스 드래그
        if (g_isLMousePressed) DrawPixelAtMouse(hwnd, LOWORD(lParam), HIWORD(lParam), false);
        else if (g_isRMousePressed) DrawPixelAtMouse(hwnd, LOWORD(lParam), HIWORD(lParam), true);
        return 0;

    case WM_LBUTTONUP: g_isLMousePressed = false; return 0;
    case WM_RBUTTONUP: g_isRMousePressed = false; return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
        SelectObject(memDC, memBitmap);

        Graphics graphics(memDC);
        graphics.Clear(Color(255, 30, 30, 30));

        // [NEW] 해상도 텍스트 UI 그리기 (GDI+ 폰트 렌더링)
        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 16, FontStyleBold, UnitPixel);
        SolidBrush textBrush(Color(255, 220, 220, 220)); // 밝은 회색 텍스트

        WCHAR resText[128];
        swprintf_s(resText, L"Current Resolution: %d x %d   [ ↑ / ↓ keys to change ]", g_targetRes, g_targetRes);

        // 텍스트를 화면 좌측 상단 (50, 20) 위치에 출력
        graphics.DrawString(resText, -1, &font, PointF(50.0f, 20.0f), &textBrush);

        if (g_pPixelatedImg)
        {
            int drawSize = (width < height ? width : height) - 100;
            float step = (float)drawSize / g_targetRes;

            // 투명도 확인용 체크무늬 배경 그리기 (포토샵처럼)
            SolidBrush grayBrush(Color(255, 100, 100, 100));
            SolidBrush darkBrush(Color(255, 60, 60, 60));
            for (int y = 0; y < g_targetRes; ++y) {
                for (int x = 0; x < g_targetRes; ++x) {
                    SolidBrush* targetBrush = ((x + y) % 2 == 0) ? &grayBrush : &darkBrush;
                    graphics.FillRectangle(targetBrush,
                        (int)(50 + x * step), (int)(50 + y * step),
                        (int)step + 1, (int)step + 1);
                }
            }

            // 픽셀화된 이미지 출력
            graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
            // [핵심 해결책] 이 한 줄을 추가하여 GDI+의 반 칸 어긋남(Half-pixel shift)을 교정합니다.
            graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
            graphics.DrawImage(g_pPixelatedImg, 50, 50, drawSize, drawSize);

            // [MODIFIED] 그리드 그리기 (g_showGrid가 true일 때만!)
            if (g_showGrid)
            {
                Pen gridPen(Color(100, 255, 255, 255), 1);
                for (int i = 0; i <= g_targetRes; ++i) {
                    graphics.DrawLine(&gridPen, (int)(50 + i * step), 50, (int)(50 + i * step), 50 + drawSize);
                    graphics.DrawLine(&gridPen, 50, (int)(50 + i * step), 50 + drawSize, (int)(50 + i * step));
                }
            }

            // 현재 선택된 펜 색상 미리보기 UI (우측 상단)
            SolidBrush currentBrush(g_currentColor);
            Pen whitePen(Color(255, 255, 255, 255), 2);

            // 화면 우측 여백에 50x50 크기의 색상 상자 그리기
            int uiX = width - 80;
            int uiY = 50;
            graphics.FillRectangle(&currentBrush, uiX, uiY, 50, 50);
            graphics.DrawRectangle(&whitePen, uiX, uiY, 50, 50);
        }

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}