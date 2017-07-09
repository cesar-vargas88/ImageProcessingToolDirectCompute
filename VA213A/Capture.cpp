#include "StdAfx.h"
#include "Capture.h"

CCapture::CCapture(IAtWareVideoCapture* pVC,CVAImagen** ppImage, HWND hWnd, CDX11Manager g_Manager)
{
	m_pVC=pVC;
	m_ppImage=ppImage;
	m_hWnd=hWnd;
	m_Manager=g_Manager;
}
CCapture::~CCapture(void)
{
}

extern void Pipeline(void);
extern CRITICAL_SECTION g_cs;

HRESULT CCapture::SampleCB( double SampleTime, IMediaSample *pSample )				//Mejor para desempacar y tomar muestras
{
	//Interfaz de DirectShow

	AM_MEDIA_TYPE mt;
	m_pVC->GetMediaType(&mt);
	
	int y = 0;

	//Verificamos que formato viene, y usamos lo desempacamos...
	int x;
	if (mt.formattype==FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvih=(VIDEOINFOHEADER*)mt.pbFormat;
		if (pvih->bmiHeader.biCompression=='2YUY')
		{
			struct YUY2				//Macro PIXEL
			{
				unsigned char Y0;
				unsigned char U0;
				unsigned char Y1;
				unsigned char V0;
			};

			YUY2* pBuffer = 0;
			
			pSample->GetPointer((BYTE**)&pBuffer);
			EnterCriticalSection(&g_cs);
			if (*m_ppImage)
				CVAImagen::DestroyImage(*m_ppImage);
			*m_ppImage=CVAImagen::CreateNewImage(pvih->bmiHeader.biWidth,pvih->bmiHeader.biHeight);
			
			CVAImagen *m_pImageYUY2  = CVAImagen::CreateNewImage(pvih->bmiHeader.biWidth / 2 ,pvih->bmiHeader.biHeight);
			void* p_BackUpImageYUY2 = m_pImageYUY2->m_pBaseDirection ;			//Respaldo Puntero de m_pImage_YUY2
			m_pImageYUY2->m_pBaseDirection = (CVAImagen::PIXEL*)pBuffer;

			//////////////////////////////////////////////////////////////////////////////////
			//		Usamos el GPU. Indicamos a la tarjeta de video que use un shader		//
			//////////////////////////////////////////////////////////////////////////////////
			
			CGPUView* pViewInput=CGPUView::CreateView(&m_Manager, m_pImageYUY2);						//Creamos vista de lectura
			pViewInput->SetRead(0);
			CGPUView* pViewOutput=CGPUView::CreateView(&m_Manager,*m_ppImage);						//Creamos vista de escritura
			pViewOutput->SetReadWrite(0);
			m_Manager.GetContext()->CSSetShader(m_Manager.m_mapComputeShaders[102].m_pICS,NULL,NULL);
			m_Manager.GetContext()->Dispatch((pvih->bmiHeader.biWidth+15)/16,(pvih->bmiHeader.biHeight+15)/16,1);
			pViewOutput->Update();				//Este metodo llevo los datos de la vista de salida hasta la imagen
			CGPUView::DestroyView(pViewInput);
			CGPUView::DestroyView(pViewOutput);

			Pipeline();
			LeaveCriticalSection(&g_cs);
			InvalidateRect(m_hWnd,NULL,false);

			m_pImageYUY2->m_pBaseDirection = (CVAImagen::PIXEL*)p_BackUpImageYUY2;
			CVAImagen::DestroyImage(m_pImageYUY2);
			//Para CPU, esto sirve para liberar memoria
			m_Manager.GetContext()->Flush();
		}
		else if (pvih->bmiHeader.biCompression==0)			//RGB 24,32
		{
			if (mt.subtype==MEDIASUBTYPE_RGB24)
			{
				struct RGB24				//Macro PIXEL
				{
					unsigned char B;
					unsigned char G;
					unsigned char R;
				};

				RGB24* pBuffer = 0;
				
				EnterCriticalSection(&g_cs);

				pSample->GetPointer((BYTE**)&pBuffer);
				if (*m_ppImage)
					CVAImagen::DestroyImage(*m_ppImage);
				*m_ppImage=CVAImagen::CreateNewImage(pvih->bmiHeader.biWidth,pvih->bmiHeader.biHeight);
			
				for (int j=pvih->bmiHeader.biHeight-1;j>=0;j--)
				{
					for (int i=0;i<pvih->bmiHeader.biWidth;i++)
					{
						CVAImagen::PIXEL& p=(**m_ppImage)(i,j);

						p.r = pBuffer->R;		
						p.g = pBuffer->G;
						p.b = pBuffer->B;

						pBuffer++;
					}
				}

				Pipeline();
				LeaveCriticalSection(&g_cs);
				InvalidateRect(m_hWnd,NULL,false);
				//Para CPU, esto sirve para liberar memoria
				m_Manager.GetContext()->Flush();
			}		
			else if (mt.subtype==MEDIASUBTYPE_RGB32)
			{
				struct RGB32				//Macro PIXEL
				{
					unsigned char B;
					unsigned char G;
					unsigned char R;
					unsigned char A;
				};

				RGB32* pBuffer = 0;

				EnterCriticalSection(&g_cs);

				pSample->GetPointer((BYTE**)&pBuffer);
				if (*m_ppImage)
					CVAImagen::DestroyImage(*m_ppImage);
				*m_ppImage=CVAImagen::CreateNewImage(pvih->bmiHeader.biWidth,pvih->bmiHeader.biHeight);

				for (int j=pvih->bmiHeader.biHeight-1;j>=0;j--)
				{
					for (int i=0;i<pvih->bmiHeader.biWidth;i++)
					{
						CVAImagen::PIXEL& p=(**m_ppImage)(i,j);

						p.r = pBuffer->R;		
						p.g = pBuffer->G;
						p.b = pBuffer->B;
						p.a = pBuffer->A;

						pBuffer++;
					}
				}

				Pipeline();
				LeaveCriticalSection(&g_cs);
				InvalidateRect(m_hWnd,NULL,false);
				//Para CPU, esto sirve para liberar memoria
				m_Manager.GetContext()->Flush();
			}		
		}
		//Implementar un shader que haga la traducción a RGB
	}
	else if (mt.formattype==FORMAT_VideoInfo2)
	{
		x = 1;
	}

	//Delete the media type when you are done.
	
	if (mt.cbFormat != 0)
	{
		CoTaskMemFree((PVOID)mt.pbFormat);
		mt.cbFormat = 0;
		mt.pbFormat = NULL;
	}
	if (mt.pUnk !=NULL)
	{
		//pUnk should not be used
		mt.pUnk->Release();
		mt.pUnk = NULL;
	}
	
	return S_OK;
}
HRESULT CCapture::BufferCB( double SampleTime, BYTE *pBuffer, long BufferLen )		//Mejor cuando no hay cambio d formato "es más rapido"
{
	return S_OK;
}