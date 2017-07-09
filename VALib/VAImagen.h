#pragma once
#include <Windows.h>
#include <iostream>
#include <stack>

using namespace std;
class CVAHistograma;
class CVAImagen
{
public:
	struct PIXEL
	{
		union
		{
			struct
			{
				unsigned char b,g,r,a;
			};
			unsigned char v[4];
			unsigned long c;
		};
	};

	PIXEL* m_pBaseDirection;
	unsigned long m_ulSizeX;
	unsigned long m_ulSizeY;
	stack <double> m_ulMostShinyPixel;

protected:

	CVAImagen(void);
	~CVAImagen(void);

public:

	PIXEL& operator()(unsigned long i, unsigned long j);
	static CVAImagen* CreateNewImage(unsigned long X, unsigned long Y);
	static CVAImagen* CloneImage(CVAImagen* pOriginalImage);
	static void DestroyImage(CVAImagen* pImage);
	// Dibuja la imagen al macenada en el contexto de dispositivo especificado
	void DrawImage(HDC hdc, int x0, int y0, int cx, int cy);
	static CVAImagen* LoadBMP(const char* pszFileName);
	static CVAImagen* CVAImagen::LoadFromFile(const char* pszFileName);
	static bool CVAImagen::SaveToFile(CVAImagen *pImage,const char* pszFileName);
	static void CVAImagen::BuildContrastTable(unsigned char* pTable,float Potencia);
	CVAHistograma* BuildHistogram();
	void ApplyFunctionTransfer(unsigned char* TransferTable);
	void BuildDepthMap(void);
	static CVAImagen* MostShinyPixelColumn(CVAImagen* pImage, CVAImagen* pScan);
	//static CVAImagen* FillDepthMap(CVAImagen* pDepthMap, unsigned long j,stack <PIXEL> Color);
	static CVAImagen* MultipleThreshold(CVAImagen* pImage, CVAImagen* pSegmentation, stack<unsigned char> sMinima);

protected:
	
	void Serialize(iostream& io,bool bSave);

public:

	CVAImagen* ConvertCMY();
	CVAImagen* ConvertBW();
};

