#pragma once
#include "..\videocapture\atwarevideocapture.h"
#include "..\VALib\VAImagen.h"
#include "..\VALib\GPUView.h"
#include "..\VALib\DX11Manager.h"

class CCapture :
	public IAtWareSampleGrabberCB
{
	IAtWareVideoCapture* m_pVC;
	CVAImagen** m_ppImage;
	HWND m_hWnd;
	CDX11Manager m_Manager;
public:
	CCapture(IAtWareVideoCapture* m_pVC,CVAImagen** ppImage, HWND hWnd, CDX11Manager g_Manager);
	~CCapture(void);
	virtual HRESULT SampleCB( double SampleTime, IMediaSample *pSample );				//Mejor para desempacar y tomar muestras
	virtual HRESULT BufferCB( double SampleTime, BYTE *pBuffer, long BufferLen );		//Mejor cuando no hay cambio d formato "es más rapido"
};

