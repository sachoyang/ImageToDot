#pragma once
#include <windows.h>
#include <gdiplus.h>

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

    // 설정 상태
    int m_targetRes;
    bool m_useNearestNeighbor;
    int m_currentPaletteIndex;
    bool m_useDithering;
};