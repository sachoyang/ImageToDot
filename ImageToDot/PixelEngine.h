#pragma once
#include <vector>
#include <windows.h>
#include <gdiplus.h>
#include <vector>

#include <opencv2/opencv.hpp>

class PixelEngine
{
public:
    PixelEngine();
    ~PixelEngine();

    // 상태 변경 메서드
    void SetTargetResolution(int res);
    void SetInterpolation(bool useNearest);
    void SetPaletteIndex(int index);
    void SetDithering(bool useDithering);

    // [MODIFIED] public 영역에서 함수 이름과 리턴 타입을 바꿉니다.
public:
    Gdiplus::Bitmap* CreateMonoproImage(int pixelSize, int colorCount, int blurStrength);
    void SetPixelatedImage(Gdiplus::Bitmap* newImg); // 새로 만든 그림을 안전하게 적용할 셋업 함수

public:
    // 현재 상태 가져오기
    int GetTargetResolution() const { return m_targetRes; }
    Gdiplus::Bitmap* GetPixelatedImage() const { return m_pPixelatedImg; }

    // 주요 기능
    bool LoadImageFile(const WCHAR* filePath);
    bool SaveImageFile(const WCHAR* filePath);

    // 렌더링 파이프라인
    void UpdateCanvasFromOriginal();
    void ApplyPaletteToCanvas();
    void DrawPixel(int gridX, int gridY, Gdiplus::Color color, bool isEraser);

private:
    Gdiplus::Color GetClosestColor(Gdiplus::Color target, const Gdiplus::Color* palette, int paletteSize);
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

private:
    // 데이터
    Gdiplus::Bitmap* m_pOriginalImg;
    Gdiplus::Bitmap* m_pCanvasImg;
    Gdiplus::Bitmap* m_pPixelatedImg;

    // K-Means 알고리즘이 찾아낸 동적 팔레트 배열
    std::vector<Gdiplus::Color> m_kMeansPalette;

    // 설정 상태
    int m_targetRes;
    bool m_useNearestNeighbor;
    int m_currentPaletteIndex;
    bool m_useDithering;

private:
    // GDI+ Bitmap과 OpenCV Mat을 서로 변환해주는 헬퍼 함수
    cv::Mat GdiplusBitmapToCvMat(Gdiplus::Bitmap* bmp);
    Gdiplus::Bitmap* CvMatToGdiplusBitmap(const cv::Mat& mat);


};