#pragma once

#include "VAImagen.h"
#include <stack>

class CVAHistograma
{
public:
	
	CVAHistograma(void);
	~CVAHistograma(void);

	struct FRECUENCIA
	{
		unsigned long FreqR;
		unsigned long FreqG;
		unsigned long FreqB;
		unsigned long FreqA;
	};

	FRECUENCIA m_Hist[256];				// 1 medida de frecuencia por intensidad

	struct FRECUENCIA_NORMALIZADA
	{
		float FreqR;
		float FreqG;
		float FreqB;
		float FreqA;
	};

	FRECUENCIA_NORMALIZADA m_HistNorm[256];
	FRECUENCIA_NORMALIZADA m_HistNormSuavizado[256];

	struct MINIMOS
	{
		stack<unsigned char> MinimoR;
		stack<unsigned char> MinimoG;
		stack<unsigned char> MinimoB;
		stack<unsigned char> MinimoA;
	};

	MINIMOS Minimos;

	void Normalizar(void);
	CVAImagen* CrearGrafico(int sx, int sy, int nColor);
	void SuavizarHistograma(int nSizeWindow, int nRepeatSmooting);
	void ObtenerMinimos(float fUmbral, int nColor);
};

