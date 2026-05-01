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

    // [NEW] 4번(K-Means)이 선택되었을 때, 사진을 분석하여 8개의 동적 팔레트 추출
    if (m_currentPaletteIndex == 4) {
        int k = 8;
        m_kMeansPalette.assign(k, Color(0, 0, 0, 0));

        // 초기 중심값 설정 (대각선으로 색상을 랜덤하게 픽업)
        for (int i = 0; i < k; ++i) {
            int rx = (m_targetRes * i) / k;
            m_pCanvasImg->GetPixel(rx, rx, &m_kMeansPalette[i]);
        }

        // K-Means 군집화 (10번 반복하여 정확한 대표색 도출)
        for (int iter = 0; iter < 10; ++iter) {
            std::vector<long> sumR(k, 0), sumG(k, 0), sumB(k, 0), counts(k, 0);

            for (int y = 0; y < m_targetRes; ++y) {
                for (int x = 0; x < m_targetRes; ++x) {
                    Color c;
                    m_pCanvasImg->GetPixel(x, y, &c);
                    if (c.GetA() == 0) continue;

                    int bestK = 0; long minDist = 2147483647;
                    for (int i = 0; i < k; ++i) {
                        long dR = c.GetR() - m_kMeansPalette[i].GetR();
                        long dG = c.GetG() - m_kMeansPalette[i].GetG();
                        long dB = c.GetB() - m_kMeansPalette[i].GetB();
                        long dist = dR * dR + dG * dG + dB * dB;
                        if (dist < minDist) { minDist = dist; bestK = i; }
                    }
                    sumR[bestK] += c.GetR(); sumG[bestK] += c.GetG(); sumB[bestK] += c.GetB();
                    counts[bestK]++;
                }
            }
            // 평균값으로 중심점 이동 (새로운 대표색 찾기)
            for (int i = 0; i < k; ++i) {
                if (counts[i] > 0) m_kMeansPalette[i] = Color(255, sumR[i] / counts[i], sumG[i] / counts[i], sumB[i] / counts[i]);
            }
        }
    }

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
                    r = max(0, min(255, r + offset)); g = max(0, min(255, g + offset)); b = max(0, min(255, b + offset));
                }
                Color ditheredColor(pixelColor.GetA(), r, g, b);

                if (m_currentPaletteIndex == 1) {
                    int gray = (r + g + b) / 3;
                    newColor = (gray > 127) ? Color(255, 255, 255, 255) : Color(255, 0, 0, 0);
                }
                else if (m_currentPaletteIndex == 2) newColor = GetClosestColor(ditheredColor, g_palGameBoy, 4);
                else if (m_currentPaletteIndex == 3) newColor = GetClosestColor(ditheredColor, g_palPico8, 16);

                // [NEW] 방금 생성된 K-Means 8색 팔레트 적용!
                else if (m_currentPaletteIndex == 4) newColor = GetClosestColor(ditheredColor, m_kMeansPalette.data(), m_kMeansPalette.size());
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
            else if (m_currentPaletteIndex == 4) mappedColor = GetClosestColor(ditheredColor, m_kMeansPalette.data(), m_kMeansPalette.size());
        }
        m_pPixelatedImg->SetPixel(gridX, gridY, mappedColor);
    }
}

// ==============================================================================
// [NEW] OpenCV & Monopro 스타일 고급 알고리즘 구현부
// ==============================================================================

// 헬퍼 1: GDI+ Bitmap -> OpenCV Mat 변환
cv::Mat PixelEngine::GdiplusBitmapToCvMat(Gdiplus::Bitmap* bmp) {
    Gdiplus::Rect rect(0, 0, bmp->GetWidth(), bmp->GetHeight());
    Gdiplus::BitmapData bmpData;
    bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData);
    cv::Mat mat(bmp->GetHeight(), bmp->GetWidth(), CV_8UC4, bmpData.Scan0, bmpData.Stride);
    cv::Mat matCopy;
    mat.copyTo(matCopy); // 깊은 복사 (메모리 충돌 방지)
    bmp->UnlockBits(&bmpData);
    return matCopy;
}

// 헬퍼 2: OpenCV Mat -> GDI+ Bitmap 변환 (메모리 충돌 완벽 해결 버전)
Gdiplus::Bitmap* PixelEngine::CvMatToGdiplusBitmap(const cv::Mat& mat) {
    cv::Mat bgra;
    if (mat.channels() == 3) cv::cvtColor(mat, bgra, cv::COLOR_BGR2BGRA);
    else bgra = mat;

    // 1. GDI+의 꼼수(Lazy Copy)를 막기 위해 완전히 빈 도화지를 새로 만듭니다.
    Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(bgra.cols, bgra.rows, PixelFormat32bppARGB);

    Gdiplus::Rect rect(0, 0, bgra.cols, bgra.rows);
    Gdiplus::BitmapData bmpData;

    // 2. 도화지 메모리를 잠그고 쓰기 권한을 얻어옵니다.
    bmp->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);

    // 3. OpenCV의 픽셀 데이터를 GDI+ 메모리에 안전하게 직접 덮어씌웁니다.
    for (int y = 0; y < bgra.rows; ++y) {
        byte* dst = (byte*)bmpData.Scan0 + y * bmpData.Stride;
        const byte* src = bgra.ptr<byte>(y);
        memcpy(dst, src, bgra.cols * 4); // ARGB 4바이트(32bpp) 복사
    }

    // 4. 잠금 해제
    bmp->UnlockBits(&bmpData);

    // 이제 함수가 끝나고 OpenCV Mat이 소멸되더라도 절대 터지지 않습니다!
    return bmp;
}

// 완성된 이미지만 조심스럽게 리턴합니다 (기존 ApplyMonoproStyle 교체)
Gdiplus::Bitmap* PixelEngine::CreateMonoproImage(int pixelSize, int colorCount, int blurStrength) {
    if (!m_pOriginalImg) return nullptr;

    cv::Mat inputMat = GdiplusBitmapToCvMat(m_pOriginalImg);
    cv::Mat bgrMat;
    cv::cvtColor(inputMat, bgrMat, cv::COLOR_BGRA2BGR);

    cv::Mat smallMat;
    int newWidth = max(1, bgrMat.cols / pixelSize);
    int newHeight = max(1, bgrMat.rows / pixelSize);
    cv::resize(bgrMat, smallMat, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);

    cv::Mat filteredMat;
    cv::bilateralFilter(smallMat, filteredMat, 9, blurStrength, blurStrength);

    cv::Mat data = filteredMat.reshape(1, filteredMat.total());
    data.convertTo(data, CV_32F);

    // K-Means 충돌 방지: 총 픽셀 수보다 요구하는 색상 수가 많으면 픽셀 수로 제한합니다.
    int actualColorCount = std::min(colorCount, data.rows);

    cv::Mat labels, centers;
    // [MODIFIED] colorCount 대신 안전하게 actualColorCount를 넣습니다.
    cv::kmeans(data, actualColorCount, labels,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0),
        3, cv::KMEANS_PP_CENTERS, centers);

    centers.convertTo(centers, CV_8U);
    cv::Mat quantizedMat(filteredMat.size(), filteredMat.type());
    for (int i = 0; i < filteredMat.rows; ++i) {
        for (int j = 0; j < filteredMat.cols; ++j) {
            int cluster_idx = labels.at<int>(i * filteredMat.cols + j, 0);
            quantizedMat.at<cv::Vec3b>(i, j)[0] = centers.at<cv::Vec3b>(cluster_idx, 0)[0];
            quantizedMat.at<cv::Vec3b>(i, j)[1] = centers.at<cv::Vec3b>(cluster_idx, 0)[1];
            quantizedMat.at<cv::Vec3b>(i, j)[2] = centers.at<cv::Vec3b>(cluster_idx, 0)[2];
        }
    }

    cv::Mat outputMat;
    cv::resize(quantizedMat, outputMat, bgrMat.size(), 0, 0, cv::INTER_NEAREST);

    // [핵심] 기존 캔버스를 지우지 않고 새 결과물만 던집니다! (충돌 원인 제거)
    return CvMatToGdiplusBitmap(outputMat);
}

// 메인 UI 스레드가 안전하게 이미지를 갈아끼우는 함수
void PixelEngine::SetPixelatedImage(Gdiplus::Bitmap* newImg) {
    if (m_pPixelatedImg) delete m_pPixelatedImg;
    m_pPixelatedImg = newImg;
}