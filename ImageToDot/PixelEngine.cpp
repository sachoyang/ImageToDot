#include "PixelEngine.h"
#include <math.h>
#include <algorithm>

using namespace Gdiplus;
using namespace std;

// --- 프리셋 데이터 (cpp 내부에 숨김) ---
const Color g_palGameBoy[4] = {
    Color(255, 15, 56, 15), Color(255, 48, 98, 48), Color(255, 139, 172, 15), Color(255, 155, 188, 15)
};
const Color g_palPico8[16] = {
    Color(255, 0, 0, 0), Color(255, 29, 43, 83), Color(255, 126, 37, 83), Color(255, 0, 135, 81),
    Color(255, 171, 82, 54), Color(255, 95, 87, 79), Color(255, 194, 195, 199), Color(255, 255, 241, 232),
    Color(255, 255, 0, 77), Color(255, 255, 163, 0), Color(255, 255, 236, 39), Color(255, 0, 228, 54),
    Color(255, 41, 173, 255), Color(255, 131, 118, 156), Color(255, 255, 119, 168), Color(255, 255, 204, 170)
};

static void RGBtoHSV(int r, int g, int b, float& h, float& s, float& v) {
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
    s = (cMax > 0.0f) ? (delta / cMax) : 0.0f;

    if (delta == 0.0f) h = 0.0f;
    else {
        if (cMax == fR) h = 60.0f * fmodf(((fG - fB) / delta), 6.0f);
        else if (cMax == fG) h = 60.0f * (((fB - fR) / delta) + 2.0f);
        else if (cMax == fB) h = 60.0f * (((fR - fG) / delta) + 4.0f);
        if (h < 0.0f) h += 360.0f;
    }
}

PixelEngine::PixelEngine() : m_pOriginalImg(nullptr), m_pCanvasImg(nullptr), m_pPixelatedImg(nullptr),
m_targetRes(32), m_useNearestNeighbor(true), m_currentPaletteIndex(0), m_useDithering(false) {
}

PixelEngine::~PixelEngine() {
    if (m_pOriginalImg) delete m_pOriginalImg;
    if (m_pCanvasImg) delete m_pCanvasImg;
    if (m_pPixelatedImg) delete m_pPixelatedImg;
}

void PixelEngine::SetTargetResolution(int res) { m_targetRes = res; }
void PixelEngine::SetInterpolation(bool useNearest) { m_useNearestNeighbor = useNearest; }
void PixelEngine::SetPaletteIndex(int index) { m_currentPaletteIndex = index; }
void PixelEngine::SetDithering(bool useDithering) { m_useDithering = useDithering; }

bool PixelEngine::LoadImageFile(const WCHAR* filePath) {
    if (m_pOriginalImg) delete m_pOriginalImg;
    m_pOriginalImg = new Bitmap(filePath);
    return (m_pOriginalImg->GetLastStatus() == Ok);
}

int PixelEngine::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
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

bool PixelEngine::SaveImageFile(const WCHAR* filePath) {
    if (!m_pPixelatedImg) return false;
    int scale = 10;
    int outSize = m_targetRes * scale;
    Bitmap exportBitmap(outSize, outSize, PixelFormat32bppARGB);
    Graphics g(&exportBitmap);
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.DrawImage(m_pPixelatedImg, 0, 0, outSize, outSize);

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
        return (exportBitmap.Save(filePath, &pngClsid, NULL) == Ok);
    }
    return false;
}

Color PixelEngine::GetClosestColor(Color target, const Color* palette, int paletteSize) {
    float minDistance = 99999999.0f;
    Color closestColor = target;
    float h1, s1, v1;
    RGBtoHSV(target.GetR(), target.GetG(), target.GetB(), h1, s1, v1);

    const float PI = 3.14159265f;
    float angle1 = h1 * PI / 180.0f;
    float cx1 = s1 * v1 * cosf(angle1), cy1 = s1 * v1 * sinf(angle1), cz1 = v1;

    for (int i = 0; i < paletteSize; ++i) {
        float h2, s2, v2;
        RGBtoHSV(palette[i].GetR(), palette[i].GetG(), palette[i].GetB(), h2, s2, v2);
        float angle2 = h2 * PI / 180.0f;
        float cx2 = s2 * v2 * cosf(angle2), cy2 = s2 * v2 * sinf(angle2), cz2 = v2;
        float dx = cx1 - cx2, dy = cy1 - cy2, dz = cz1 - cz2;
        float distance = (dx * dx) + (dy * dy) + (dz * dz) * 1.5f;

        if (distance < minDistance) {
            minDistance = distance;
            closestColor = palette[i];
        }
    }
    return Color(target.GetA(), closestColor.GetR(), closestColor.GetG(), closestColor.GetB());
}

void PixelEngine::UpdateCanvasFromOriginal() {
    if (!m_pOriginalImg) return;
    if (m_pCanvasImg) delete m_pCanvasImg;
    m_pCanvasImg = new Bitmap(m_targetRes, m_targetRes, PixelFormat32bppARGB);
    Graphics g(m_pCanvasImg);
    g.SetInterpolationMode(m_useNearestNeighbor ? InterpolationModeNearestNeighbor : InterpolationModeHighQualityBicubic);
    g.DrawImage(m_pOriginalImg, 0, 0, m_targetRes, m_targetRes);
}

void PixelEngine::ApplyPaletteToCanvas() {
    if (!m_pCanvasImg) return;
    if (m_pPixelatedImg) delete m_pPixelatedImg;
    m_pPixelatedImg = new Bitmap(m_targetRes, m_targetRes, PixelFormat32bppARGB);
    Color pixelColor;

    static const int bayer[4][4] = { {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5} };

    for (int y = 0; y < m_targetRes; ++y) {
        for (int x = 0; x < m_targetRes; ++x) {
            m_pCanvasImg->GetPixel(x, y, &pixelColor);
            if (pixelColor.GetA() == 0) {
                m_pPixelatedImg->SetPixel(x, y, Color(0, 0, 0, 0));
                continue;
            }

            Color newColor = pixelColor;
            if (m_currentPaletteIndex > 0) {
                int r = pixelColor.GetR(), g = pixelColor.GetG(), b = pixelColor.GetB();
                if (m_useDithering) {
                    int offset = (bayer[y % 4][x % 4] - 8) * 4;
                    r = max(0, min(255, r + offset));
                    g = max(0, min(255, g + offset));
                    b = max(0, min(255, b + offset));
                }
                Color ditheredColor(pixelColor.GetA(), r, g, b);

                if (m_currentPaletteIndex == 1) {
                    int gray = (r + g + b) / 3;
                    newColor = (gray > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
                }
                else if (m_currentPaletteIndex == 2) newColor = GetClosestColor(ditheredColor, g_palGameBoy, 4);
                else if (m_currentPaletteIndex == 3) newColor = GetClosestColor(ditheredColor, g_palPico8, 16);
            }
            m_pPixelatedImg->SetPixel(x, y, newColor);
        }
    }
}

void PixelEngine::DrawPixel(int gridX, int gridY, Color color, bool isEraser) {
    Color paintColor = isEraser ? Color(0, 0, 0, 0) : color;
    if (m_pCanvasImg) m_pCanvasImg->SetPixel(gridX, gridY, paintColor);

    // 점 하나 찍었을 때 전체가 아닌 해당 픽셀만 갱신 (최적화)
    if (m_pPixelatedImg) {
        Color mappedColor = paintColor;
        if (!isEraser && m_currentPaletteIndex > 0) {
            int r = paintColor.GetR(), g = paintColor.GetG(), b = paintColor.GetB();
            if (m_useDithering) {
                static const int bayer[4][4] = { {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5} };
                int offset = (bayer[gridY % 4][gridX % 4] - 8) * 4;
                r = max(0, min(255, r + offset)); g = max(0, min(255, g + offset)); b = max(0, min(255, b + offset));
            }
            Color ditheredColor(paintColor.GetA(), r, g, b);
            if (m_currentPaletteIndex == 1) mappedColor = (((r + g + b) / 3) > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
            else if (m_currentPaletteIndex == 2) mappedColor = GetClosestColor(ditheredColor, g_palGameBoy, 4);
            else if (m_currentPaletteIndex == 3) mappedColor = GetClosestColor(ditheredColor, g_palPico8, 16);
        }
        m_pPixelatedImg->SetPixel(gridX, gridY, mappedColor);
    }
}