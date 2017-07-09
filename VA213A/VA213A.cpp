// VA213A.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Command.h"
#include "VA213A.h"
#include "Capture.h"
#include <string>
#include <Windows.h>
#include <iostream>
#include <CommDlg.h>
#include <io.h>
#include <fcntl.h>
#include "..\VALib\VAImagen.h"
#include "..\VALib\DX11Manager.h"
#include "..\VALib\GPUView.h"
#include "..\VALib\VAHistograma.h"
#include "..\VideoCapture\AtWareVideoCapture.h"

#include <D3DX10.h>
#include <math.h>

using namespace std;

//	Objetos

CDX11Manager g_Manager;							// Administración de DirectCompute
IAtWareVideoCapture* g_pIVC;					// Interfaz al Video Manager

//	Tuberia 

CVAImagen* g_pImage;							// Imagen Inicial
CVAImagen* g_pCopiaImagen;						// Copia de Imagen Inicial			
CVAImagen*	g_pGraficoHistograma;				// Imagen Histograma
CVAImagen* g_pTransformedImage;					// Imagen Transformaciones
CVAImagen* g_pConvolvedImage;					// Imagen Filtros Convolución
CVAImagen* g_pSegmentation;						// Imagen Segmentacion
CVAHistograma* g_pHistograma;					// Objeto Histograma
CCapture *g_pCapture;							// Receptaculo de cuadros

CVAImagen* g_pDepthMap;							// Imagen Mapa Profundad
CVAImagen* g_pScan;								// Imagen Escaneado

CRITICAL_SECTION g_cs;

void EnumVideoFormatsAndConfigure(IAtWareVideoCapture *pVC)
{
	IAMVideoProcAmp *pVideoProcAmp = 0;				//Interfaz de DorectShow Ayuda a controlar caracteristicas del sensor CCD de la camara
	IAMCameraControl *pCamControl = 0;				//Permite controlar la camara
	IAMStreamConfig *pStreamConfig = 0;				//Configuración del flujo de lo que se transmite
	pVC->GetCaptureDeviceControllers(&pVideoProcAmp, &pCamControl, &pStreamConfig);

	int iCount = 0;
	int iSize = 0;

	HRESULT hr = pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);

	// Check the size to make sure we pass in the correct structure
	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		//Use the video capabilities structure.
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE *pmt;							//Directiva de DirectShow que permite elegir un formato en particular
			hr = pStreamConfig->GetStreamCaps(iFormat, &pmt, (BYTE*)&scc);		//GetStreamCaps, permite seleccionar un formato
			if (SUCCEEDED(hr))
			{
				//Examine the format, and possibly use it.

				//Delete the media type when you are done.

				if (pmt->cbFormat != 0)
				{
					CoTaskMemFree((PVOID)pmt->pbFormat);
					pmt->cbFormat = 0;
					pmt->pbFormat = NULL;
				}
				if (pmt->pUnk !=NULL)
				{
					//pUnk should not be used
					pmt->pUnk->Release();
					pmt->pUnk = NULL;
				}
			}
		}
	}
	SAFE_RELEASE(pVideoProcAmp);
	SAFE_RELEASE(pCamControl);				
	SAFE_RELEASE(pStreamConfig);	
}
//////////////////////////////////
//		Transformaciones		//
//////////////////////////////////

D3DXMATRIX g_Mt;								//Transformación afin total

D3DXMATRIX g_R;									//Matriz de rotación
float g_Theta;									//Anglo de giro de la imagen

D3DXMATRIX g_T;									//MAtriz de traslación
D3DXMATRIX g_T_UpLeft;							//MAtriz de que traslada arriba a la izquierda
D3DXMATRIX g_T_DownRight;						//MAtriz de que traslada abajo a la derecha

float g_dx=0;										//Coordenada X traslación
float g_dy=0;										//Coordenada Y traslación
	
D3DXMATRIX g_S;									//Matriz de escalación
float g_s=1.0f;									//Proporción de escalación

void AplicarTransformacionesAfines(void)
{
	//////////////////////////////////////////
	//	  Se unsa para transformación		//
	//////////////////////////////////////////

	if(g_pTransformedImage)
		CVAImagen::DestroyImage(g_pTransformedImage);
	g_pTransformedImage=CVAImagen::CreateNewImage(g_pImage->m_ulSizeX,g_pImage->m_ulSizeY);
	
	//Aplicar transformación afin en GPU
	CGPUView* pSource=CGPUView::CreateView(&g_Manager,g_pImage);
	CGPUView* pDest=CGPUView::CreateView(&g_Manager,g_pTransformedImage);
	pSource->SetRead(0);
	pDest->SetReadWrite(0);

	// Matrices para transformaciones
	D3DXMatrixRotationZ(&g_R,g_Theta);
	D3DXMatrixTranslation(&g_T,g_dx,g_dy,0);
	D3DXMatrixScaling(&g_S,g_s,g_s,1.0f);
	D3DXMatrixTranslation(&g_T,g_dx,g_dy,0);

	D3DXMatrixTranslation(&g_T_UpLeft,(-(float)(g_pImage->m_ulSizeX)/2),(-(float)(g_pImage->m_ulSizeY)/2),0);
	D3DXMatrixTranslation(&g_T_DownRight,((float)(g_pImage->m_ulSizeX)/2),((float)(g_pImage->m_ulSizeY)/2),0);

	//Transformación a fin total
	//g_T_UpLeft        Desplaza el centro de la imagen a la esquina superior izquierda
	//g_R				Realiza rotación
	//g_S				Realiza escalamiento
	//g_T				Realiza Traslación
	//g_T_DownRight		Desplaza la imagen a su posición original

	g_Mt = g_T* g_T_UpLeft * g_R * g_S * g_T_DownRight;

	//DirectCompute me pide la matriz inversa y transpuesta ara control inverso, con el fin de procesar la información más rápido

	D3DXMATRIX Tr;				//Matriz Transpuesta
	D3DXMATRIX Inv;				//Matriz Inversa

	float det;					//Determinante

	D3DXMatrixInverse(&Inv,&det,&g_Mt);					//Si el determinante es 0, no se pudo invertir, si es 1 si
	D3DXMatrixTranspose(&Tr,&Inv);						//DirectCompute me pide una matriz transpuesta

	g_Manager.UpdateCSConstantBuffer(&Tr,sizeof(D3DXMATRIX));
	g_Manager.GetContext()->CSSetShader(g_Manager.m_mapComputeShaders[103].m_pICS,NULL,0);
	g_Manager.GetContext()->CSSetConstantBuffers(0,1,&g_Manager.m_pICSConstantBuffer);
	g_Manager.GetContext()->Dispatch((g_pImage->m_ulSizeX+15)/16,(g_pImage->m_ulSizeY+15)/16,1);
	pDest->Update();
	CGPUView::DestroyView(pDest);
	CGPUView::DestroyView(pSource);		

	//Para CPU, esto sirve para liberar memoria
	g_Manager.GetContext()->Flush();
}
//////////////////////////////
//		Convolución			//
//////////////////////////////

D3DXMATRIX g_kernel  (	0,0,0,0,				
						0,1,0,0,
						0,0,0,0,
						0,0,0,0);

D3DXVECTOR4 g_C(0,0,0,0);

struct PARAMS
{
	D3DXMATRIX M;
	D3DXVECTOR4 C;
}g_P;
void Convolucion(void)
{
	//////////////////////////////////////
	//	  Se unsa para convolución		//
	//////////////////////////////////////

	if(g_pConvolvedImage)
		CVAImagen::DestroyImage(g_pConvolvedImage);
	g_pConvolvedImage=CVAImagen::CreateNewImage(g_pImage->m_ulSizeX,g_pImage->m_ulSizeY);
	
	//Aplicar transformación afin en GPU
	CGPUView* pSource=CGPUView::CreateView(&g_Manager,g_pTransformedImage);
	CGPUView* pDest=CGPUView::CreateView(&g_Manager,g_pConvolvedImage);
	pSource->SetRead(0);
	pDest->SetReadWrite(0);

	//kernel Unitario  :..........función impulso en teoria de control
	/*D3DXMATRIX kernel  (0,0,0,0,
						0,1,0,0,
						0,0,0,0,
						0,0,0,0);*/

	g_Manager.UpdateCSConstantBuffer(&g_P,sizeof(PARAMS));
	g_Manager.GetContext()->CSSetShader(g_Manager.m_mapComputeShaders[104].m_pICS,NULL,0);
	g_Manager.GetContext()->CSSetConstantBuffers(0,1,&g_Manager.m_pICSConstantBuffer);
	g_Manager.GetContext()->Dispatch((g_pImage->m_ulSizeX+15)/16,(g_pImage->m_ulSizeY+15)/16,1);
	pDest->Update();
	CGPUView::DestroyView(pDest);
	CGPUView::DestroyView(pSource);		

	//Para CPU, esto sirve para liberar memoria
	g_Manager.GetContext()->Flush();
}
//////////////////////////
//		Histograma		//
//////////////////////////

bool g_bSmoothing	= false;				//Bandera suavizado histograma
int g_nSizeWindowSmooting	= 0;			//Tamaño de la ventana para suavizado del histograma
int g_nRepeatSmoothing		= 1;
bool g_bMinima		= false;				//Bandera minimos histograma
int g_nSizeWindowMinima	= 1;				//Tamaño de la ventana para encontrar minimos del histograma
float g_fThresholdMinima	= 0.0f;			//Umbral para localizar minimos del histograma

bool g_bMultiThreshold = false;

int  g_nTamanoVentana[]	= {	1,3,5,7,9,
							11,13,15,17,19,
							21,23,25,27,29,
							31,33,35,37,39,
							41,43,45,47,49,
							51,53,55,57,59};
int g_nColorHistograma = 3;

void BuildHistogram(void)
{
	if(g_pImage)
	{
		g_pHistograma=g_pImage->BuildHistogram();
		g_pHistograma->Normalizar();

		if(g_pGraficoHistograma)
			CVAImagen::DestroyImage(g_pGraficoHistograma);

		if(g_bSmoothing)
			g_pHistograma->SuavizarHistograma(g_nTamanoVentana[g_nSizeWindowSmooting], g_nRepeatSmoothing);
	
		if(g_bMinima)
			g_pHistograma->ObtenerMinimos(g_fThresholdMinima, g_nColorHistograma);
		
		if(g_bMultiThreshold)
		{
			if(g_pSegmentation)
				CVAImagen::DestroyImage(g_pSegmentation);
			g_pSegmentation=CVAImagen::CreateNewImage(g_pImage->m_ulSizeX,g_pImage->m_ulSizeY);

			if(g_nColorHistograma==0)
				g_pSegmentation=CVAImagen::MultipleThreshold(g_pImage, g_pSegmentation, g_pHistograma->Minimos.MinimoR);

			else if(g_nColorHistograma==1)
				g_pSegmentation=CVAImagen::MultipleThreshold(g_pImage, g_pSegmentation, g_pHistograma->Minimos.MinimoG);

			else if(g_nColorHistograma==2)
				g_pSegmentation=CVAImagen::MultipleThreshold(g_pImage, g_pSegmentation, g_pHistograma->Minimos.MinimoB);
		}

		g_pGraficoHistograma=g_pHistograma->CrearGrafico(512,512,g_nColorHistograma);
	}
}
//////////////////////////////////
//			Video Capture		//
//////////////////////////////////

bool g_bVideoCapture	= false;
bool g_bImageView		= false;

//////////////////////////
//		Contraste		//
//////////////////////////

unsigned char g_ContrastTable[256];
float g_ContrastPower=1.0f;

void AplicarContraste(void)
{
	if(g_pHistograma)
		delete g_pHistograma;
	if(g_pImage)
	{
		if(g_bImageView)
			g_pImage=CVAImagen::CloneImage(g_pCopiaImagen);
		g_pImage->ApplyFunctionTransfer(g_ContrastTable);
	}
}
//////////////////////////////////
//			Video Capture		//
//////////////////////////////////

void PlayVideo(HWND hWnd)
{
	//////////////////////////////////////////////////
	//		Configuramos la captura de video		//
	//////////////////////////////////////////////////
			
	D3DXMatrixIdentity(&g_Mt);

	//Histograma
	CVAImagen::BuildContrastTable(g_ContrastTable,g_ContrastPower);

	//VideoCapture
	g_pIVC = CreateAtWareVideoCapture();				//Creamos una captura
	g_pIVC->Initialize(hWnd);							//Inicializamos la pantalla
	g_pIVC->EnumAndChooseCaptureDevice();				//Seleccionamos un dispositivo

	AM_MEDIA_TYPE MediaType;							//Declaramos la estructura AM_MEDIA_TYPE
	memset(&MediaType,0,sizeof(AM_MEDIA_TYPE));			//Definimos el buffer para MediaType
	MediaType.majortype=MEDIATYPE_Video;				//Indicamos que captaremos video
	MediaType.subtype=MEDIASUBTYPE_RGB24;				//Seleccionamos el formato de video
	g_pIVC->SetMediaType(&MediaType);					//Configuramos el formato
	//EnumVideoFormatsAndConfigure(g_pIVC);				//Esta función va despues de haber elegido un dispositivo
			
	g_pCapture= new CCapture(g_pIVC, &g_pImage,hWnd,g_Manager);
	g_pIVC->SetCallBack(g_pCapture,0);//El cero o uno es el metodo de recepcion que hicimos
		
	g_pIVC->BuildStreamGraph();
	g_pIVC->ShowPreviewWindow(true);
	g_pIVC->Start();
							
	//////////////////////////////////////////////////////////////////////////
	//		Ajusta la imagen de convolución al tamaño de la pantalla		//
	//////////////////////////////////////////////////////////////////////////

	if(g_pIVC)
	{
		RECT rc;
		GetClientRect(hWnd,&rc);	//Calcula el tamaño del area cliente "pantalla blanca"
		rc.left = 0;
		rc.top = 0;
		rc.right = rc.right/3;
		rc.bottom = rc.bottom/2;
		g_pIVC->SetPreviewWindowPosition(&rc);
	}
}
void StopVideo(HWND hWnd)
{
	if(g_pIVC)
	{
		g_pIVC->Stop();
		if(g_pCapture)
			delete g_pCapture;
		DestroyAtWareVideoCapture(g_pIVC);
	}

	if(g_pGraficoHistograma)
	{
		CVAImagen::DestroyImage(g_pGraficoHistograma);
		g_pGraficoHistograma		= NULL;
	}

	if(g_pTransformedImage)
	{
		CVAImagen::DestroyImage(g_pTransformedImage);
		g_pTransformedImage			= NULL;
	}
					
	if(g_pConvolvedImage)
	{
		CVAImagen::DestroyImage(g_pConvolvedImage);
		g_pConvolvedImage			= NULL;
	}
				
	if(g_pImage)
	{
		CVAImagen::DestroyImage(g_pImage);
		g_pImage					= NULL;
	}

	if(g_pCopiaImagen)
	{
		CVAImagen::DestroyImage(g_pCopiaImagen);
		g_pCopiaImagen				= NULL;
	}

	if(g_pGraficoHistograma)
	{
		CVAImagen::DestroyImage(g_pGraficoHistograma);
		g_pGraficoHistograma		= NULL;
	}

	if(g_pSegmentation)
	{
		CVAImagen::DestroyImage(g_pSegmentation);
		g_pSegmentation				= NULL;
	}
	
	if(g_pScan)
	{
		CVAImagen::DestroyImage(g_pScan);
		g_pScan						= NULL;
	}

	if(g_pDepthMap)
	{
		CVAImagen::DestroyImage(g_pDepthMap);
		g_pDepthMap					= NULL;
	}

						
	InvalidateRect(hWnd,NULL,true);
}



//////////////////////////////////////////
//			Comunicación USB			//
//////////////////////////////////////////

bool SendPICCommand(HANDLE hUSB,PICCOMMAND* pInCmd)
{
	unsigned char Zero=0;
	DWORD dwBytesWritten=0;
	WriteFile(hUSB,&Zero,1,&dwBytesWritten,NULL);  //Debes enviar un byte 0x00 antes de enviar un datagrama
	WriteFile(hUSB,&pInCmd->m_bCommandCode,1,&dwBytesWritten,NULL);
	WriteFile(hUSB,&pInCmd->m_bParamsBufferSize,1,&dwBytesWritten,NULL);
	if(pInCmd->m_bParamsBufferSize)
	{
		WriteFile(
			hUSB,
			pInCmd->m_abParamsBuffer,
			pInCmd->m_bParamsBufferSize,
			&dwBytesWritten,NULL);
	}
	return true;

}
bool ReceivePICCommand(HANDLE hUSB,PICCOMMAND* pOutCmd)
{
	DWORD dwBytesWritten=0;
	memset(pOutCmd->m_abParamsBuffer,0,16);
	ReadFile(hUSB,&pOutCmd->m_bCommandCode,1,&dwBytesWritten,NULL);
	ReadFile(hUSB,&pOutCmd->m_bParamsBufferSize,1,&dwBytesWritten,NULL);
	if(pOutCmd->m_bParamsBufferSize)
	{
		ReadFile(hUSB,pOutCmd->m_abParamsBuffer,
			pOutCmd->m_bParamsBufferSize,
			&dwBytesWritten,NULL);
	}
	return true;
}

//////////////////////////////////
//		Mapa de profundidad		//
//////////////////////////////////

HANDLE hUSB;
unsigned long g_ulNumberScans = 0;
bool g_bScanActive = false;
bool g_bPICResponse = true;

double g_dC		= 135;				// Distancia del lente de la camara a cero
double g_dA		= 135;				// Diastancia del laser al lente de la camara 
double g_dEo	= 593;				// Distancia en Pixeles del lente al CCD
double g_dAlpha	= 45;				// Angulo de visión de la camara
double g_dPixelZero = 240;			// Cantidad de pixeles del cero de la imagen a cero de referencia del scaner

CVAImagen::PIXEL g_ScaleColor[1500];

void LoadScaleColor()
{
	unsigned char r = 255;
	unsigned char g = 0;
	unsigned char b = 0;

	for(int x = 0; x < 1500 ; x++)
	{
		if(r == 255 && g < 255)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			g++;
		}
		else if(g == 255 && r > 0)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			r--;
		}
		else if(g == 255 && b < 255)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			b++;
		}
		else if(b == 255 && g > 0)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			g--;
		}
		else if(b == 255 && r < 255)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			r++;
		}
		else if(r == 255 && b > 0)
		{
			g_ScaleColor[x].r = r;
			g_ScaleColor[x].g = g;
			g_ScaleColor[x].b = b;
			b--;
		}
	}
}
double Radianes(double Grados)
{
	return  Grados * 3.1416 / 180;
}

double PixelDepth(double g_dPixelZero /* Cantidad de pixeles del cero de la imagen a cero de referencia del scaner*/
				, double dPixelScan /* Cantidad de pixeles del cero de la imagen al reflejo del objeto scaneado*/)
{
	double theta = atan( ( g_dPixelZero - dPixelScan ) / g_dEo );
	double phi = Radianes (90 - g_dAlpha) - theta;
	double D = g_dA * tan( phi);
	
	if((g_dC - D) >= 0)
		return g_dC - D;
	else
		return 0;
}
CVAImagen::PIXEL DepthMapColor(double Depth)
{
	CVAImagen::PIXEL Color;
	
	int nColor = Depth*20;

	Color = g_ScaleColor[nColor];

	return Color;
}
CVAImagen* BuildDepthMap(CVAImagen *pScan, CVAImagen *pDepthMap, unsigned long i)
{
	for(unsigned long j = pDepthMap->m_ulSizeY-1 ; j > 0 ; j--)
	{
		pDepthMap->m_pBaseDirection[(j*pDepthMap->m_ulSizeX)+i] = DepthMapColor(PixelDepth(g_dPixelZero,pScan->m_ulMostShinyPixel.top()));
		
		pScan->m_ulMostShinyPixel.pop();
	}
	return pDepthMap;
}
void MapaProfundidad()
{
	if(g_pImage)
	{
		//////////////////////////////////////////////////////////
		//			Nueva Imagen para mapa de profundidad		//
		//////////////////////////////////////////////////////////

		//Se crea una vez que se termine de escanear todo el objeto
		if(g_ulNumberScans == 0)	
		{
			if(g_pDepthMap)
				CVAImagen::DestroyImage(g_pDepthMap);
			g_pDepthMap=CVAImagen::CreateNewImage(g_pImage->m_ulSizeX,g_pImage->m_ulSizeY);
			memset(g_pDepthMap->m_pBaseDirection,0x0,g_pDepthMap->m_ulSizeX*g_pDepthMap->m_ulSizeY*sizeof(CVAImagen::PIXEL));
			LoadScaleColor();
		}
		
		//////////////////////////////////////////////////
		//			Obtenemos reflejo del laser			//
		//////////////////////////////////////////////////

		if(g_pScan)
			CVAImagen::DestroyImage(g_pScan);
		g_pScan=CVAImagen::CreateNewImage(g_pImage->m_ulSizeX,g_pImage->m_ulSizeY);
		memset(g_pScan->m_pBaseDirection,0x0,g_pScan->m_ulSizeX*g_pScan->m_ulSizeY*sizeof(CVAImagen::PIXEL));

		g_pScan = CVAImagen::MostShinyPixelColumn(g_pImage, g_pScan);		//Obtenemos los pixeles donde se refleja el laser

		//////////////////////////////////////////
		//			Comunicación USB			//
		//////////////////////////////////////////

		PICCOMMAND _3DScanRequest={COMMAND_START_3DSCAN,0};
		PICCOMMAND _3DScanResponse;

		SendPICCommand(hUSB,&_3DScanRequest);
		ReceivePICCommand(hUSB,&_3DScanResponse);

		switch(_3DScanResponse.m_bCommandCode)
		{
			case ERROR_PIC_SUCCESS:
				{
					//Condición para indicar cuando termino de escanear el objeto
					if(g_ulNumberScans == g_pDepthMap->m_ulSizeX-1)
					{
						PICCOMMAND _3DScanReturnRequest={COMMAND_RETURN_3DSCAN,0};
						PICCOMMAND _3DScanReturnResponse;

						SendPICCommand(hUSB,&_3DScanReturnRequest);
						ReceivePICCommand(hUSB,&_3DScanReturnResponse);

						switch(_3DScanReturnResponse.m_bCommandCode)
						{
							case ERROR_PIC_SUCCESS:
								{
									g_ulNumberScans = 0;
									g_bScanActive = false;
									break;
								}
							case ERROR_PIC_UNIMPLEMENTED_COMMAND:
								printf("%s\n","El pic ha respondido, pero no ha ejecuta el comando ya que no lo reconoce");
								printf("%s\n","Verifique que la aplicación esté en ejecución");
								break;
						}
					}
					else
					{
						g_pDepthMap = BuildDepthMap(g_pScan, g_pDepthMap, g_ulNumberScans);				//Construimos el mapa de profundidad

						//Condición para indicar cuando termino de escanear el objeto
						if(g_ulNumberScans == g_pDepthMap->m_ulSizeX-1)
						{
							g_ulNumberScans = 0;
							g_bScanActive = false;
						}
						else
						{
							g_ulNumberScans++;
						}
					}			
					break;
				}
			case ERROR_PIC_UNIMPLEMENTED_COMMAND:
				printf("%s\n","El pic ha respondido, pero no ha ejecuta el comando ya que no lo reconoce");
				printf("%s\n","Verifique que la aplicación esté en ejecución");
				break;
		}
	}
}

//////////////////////////
//		Tuberia			//
//////////////////////////

void Pipeline()
{
	AplicarContraste();
	BuildHistogram();
	AplicarTransformacionesAfines();				//Aplica transformaciones
	Convolucion();
	if (g_bScanActive)
		MapaProfundidad();
}
void RestorePipeline(void)
{
	//////////////////////////
	//		Histograma		//
	//////////////////////////

	g_bSmoothing=false;
	g_nSizeWindowSmooting = 0;

	g_bMinima	= false;					//Bandera suavizado histograma
	g_nSizeWindowSmooting	= 1;			//Tamaño de la ventana para suavizado del histograma

	g_bMinima		= false;				//Bandera minimos histograma
	g_fThresholdMinima=0.0f;				//Umbral para localizar minimos del histograma

	g_bMultiThreshold = false;

	g_nColorHistograma = 3;

	if(g_pSegmentation)
		memset(g_pSegmentation->m_pBaseDirection,0xff,g_pSegmentation->m_ulSizeX*g_pSegmentation->m_ulSizeY*sizeof(CVAImagen::PIXEL));
			
	//////////////////////////
	//		Contraste		//
	//////////////////////////

	g_ContrastPower=1.0f;	
	CVAImagen::BuildContrastTable(g_ContrastTable,g_ContrastPower);

	//////////////////////////
	//		Rotación		//
	//////////////////////////

	g_Theta=0;									
		
	//////////////////////////
	//		Posición		//
	//////////////////////////

	g_dx=0;
	g_dy=0;

	//////////////////////////
	//		Escalación		//
	//////////////////////////

	g_s=1.0f;									

	//////////////////////////
	//		Filtros			//
	//////////////////////////

	D3DXMATRIX g_kernel  (	0, 0, 0, 0,
							0, 1, 0, 0,
							0, 0, 0, 0,
							0, 0, 0, 0);

	D3DXVECTOR4 g_C(0,0,0,0);
					
	g_P.C=g_C;
	g_P.M=g_kernel;
}
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// Current instance
HWND g_hWnd;
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	InsertFilter(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	SmoothingHistogram(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//										Inicia mi codigo										//
 	//////////////////////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////
	//								Inicializa una consola	                    		//
	//////////////////////////////////////////////////////////////////////////////////////

	CoInitializeEx(0,COINIT_MULTITHREADED);			//Configuramos COM para video

	BOOL bOk = AllocConsole();
	if(bOk)
	{
		int fd;
		HANDLE hOutput;
		FILE* fp;
		hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		fd = _open_osfhandle((intptr_t)hOutput, _O_TEXT);
		fp = _fdopen(fd,"w");
		*stdout = *fp;
		char *pBuffer=(char*)malloc(32);
		setvbuf(stdout,pBuffer,_IOFBF,32);
		SetConsoleTitle(TEXT("Consola de depuración e información"));
	}
	//////////////////////////////////////////////////////////////////////////////////////

	g_Manager.InitializeDirectComputeOnly(false);				//Iniciamos DirectCompute, e indicamos si usamos CPU o GPU

	/////////////////////////////////////////////////////////
	//		Se compilan los shaders escritos el HLSL	   //
	/////////////////////////////////////////////////////////

	g_Manager.InitializeDX11();
											  //Ruta                                  Nombre  Identificador
	g_Manager.CompileAndCreateComputeShader("..\\Shaders\\ConvertBlackAndWhite.hlsl","CSMain",100);				//Convierte a Blanco y Negro
	g_Manager.CompileAndCreateComputeShader("..\\Shaders\\Blur.hlsl","CSMain",101);								//Blur
	g_Manager.CompileAndCreateComputeShader("..\\Shaders\\ConvertYUY2toR8G8B8A8.hlsl","CSMain",102);			//Convierte YUY2 a RGBA
	g_Manager.CompileAndCreateComputeShader("..\\Shaders\\AffineTransform.hlsl","CSMain",103);					//
	g_Manager.CompileAndCreateComputeShader("..\\Shaders\\Convolution3x3.hlsl","CSMain",104);					//

 	//////////////////////////////////////////////////////////////////////////////////////////////////

			//////////////////////////////////////
			//		Conexion Puerto USB			//
			//////////////////////////////////////
			
			/*hUSB=CreateFile(
				L"\\\\.\\com10",                //Nombre device
				GENERIC_READ|GENERIC_WRITE,    //Permisos de acceso
				0,							   //Sin compartir
				NULL,                          //Seguridad del usuario actual
				OPEN_EXISTING,				   //Debe existir                
				0,                             //Nada en especial
				NULL);                         //Acceso Mono hilo
			if(hUSB==INVALID_HANDLE_VALUE)
			{
				hUSB=CreateFile(
					L"\\\\.\\com9",                //Nombre device
					GENERIC_READ|GENERIC_WRITE,    //Permisos de acceso
					0,							   //Sin compartir
					NULL,                          //Seguridad del usuario actual
					OPEN_EXISTING,				   //Debe existir                
					0,                             //Nada en especial
					NULL);                         //Acceso Mono hilo
				if(hUSB==INVALID_HANDLE_VALUE)
				{
					hUSB=CreateFile(
						L"\\\\.\\com8",                //Nombre device
						GENERIC_READ|GENERIC_WRITE,    //Permisos de acceso
						0,							   //Sin compartir
						NULL,                          //Seguridad del usuario actual
						OPEN_EXISTING,				   //Debe existir                
						0,                             //Nada en especial
						NULL);                         //Acceso Mono hilo
					if(hUSB==INVALID_HANDLE_VALUE)
					{
						hUSB=CreateFile(
							L"\\\\.\\com7",                //Nombre device
							GENERIC_READ|GENERIC_WRITE,    //Permisos de acceso
							0,							   //Sin compartir
							NULL,                          //Seguridad del usuario actual
							OPEN_EXISTING,				   //Debe existir                
							0,                             //Nada en especial
							NULL);                         //Acceso Mono hilo
						if(hUSB==INVALID_HANDLE_VALUE)
						{
							printf("Error al intentar conectarse con el PIC");
						}
					}
				}
			}
		¨*/



	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_VA213A, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VA213A));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		fflush(stdout);
	}
	return (int) msg.wParam;
}
//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VA213A));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_VA213A);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}
//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_CHAR:
		{
			switch(wParam)
			{
			case 'q':
				g_Theta-=5*3.151592f/180.0f;
				break;
			case 'e':
				g_Theta+=5*3.151592f/180.0f;
				break;
			case 'w':
				g_dy-=5;
				break;
			case 's':
				g_dy+=5;
				break;
			case 'a':
				g_dx-=5;
				break;
			case 'd':
				g_dx+=5;
				break;
			case 'z':
				g_s-=0.1;
				if(g_s<=0.05f)
					g_s=0.1;
				break;
			case 'x':
				g_s+=0.1;
				break;
			case 'o':
				g_ContrastPower-=0.1;
				CVAImagen::BuildContrastTable(g_ContrastTable,g_ContrastPower);
				break;
			case 'p':
				g_ContrastPower+=0.1;
				CVAImagen::BuildContrastTable(g_ContrastTable,g_ContrastPower);
				break;
			}
			
			if(g_bImageView)
			{
				Pipeline();
				InvalidateRect(hWnd,NULL,false);
			}
			
			return 0;
		}	
	case WM_CREATE:
		{	
			InitializeCriticalSection(&g_cs);

			RestorePipeline();

			return 0;
		}
	case WM_COMMAND:
		{
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			// Parse the menu selections:
			switch (wmId)
			{
			case IDM_ABOUT:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					break;
				}
			case IDM_FILE_EXIT:
				{
					DestroyWindow(hWnd);
					break;
				}
			case IDM_FILE_OPEN:
				{
					OPENFILENAMEA ofn;
					memset(&ofn, 0, sizeof(OPENFILENAME));
					char szFileName[1024];
					szFileName[0]=0;
					ofn.lStructSize=sizeof(OPENFILENAME);
					ofn.hwndOwner=hWnd;
					ofn.hInstance=hInst;
					ofn.lpstrFilter="Archivos RAW (*.raw)\0*.raw\0Archivos BMP (*.bmp)\0*.bmp\0Todos los archivos (*.*)\0*.*\0\0";
					ofn.lpstrFile=szFileName;
					ofn.nMaxFile=1023;

					if(GetOpenFileNameA(&ofn))
					{
						switch(ofn.nFilterIndex)
						{
						case 1:
							{
								if(g_bVideoCapture)
								{
									StopVideo(hWnd);
									g_bVideoCapture	= false;	
								}
								g_bImageView	= true;
								g_pImage = CVAImagen::LoadFromFile(szFileName);
								g_pCopiaImagen = CVAImagen::LoadBMP(szFileName);
								break;
							}				
						case 2:
							{
								if(g_bVideoCapture)
								{
									StopVideo(hWnd);
									g_bVideoCapture	= false;	
								}
								g_bImageView	= true;
								g_pImage = CVAImagen::LoadBMP(szFileName);
								g_pCopiaImagen = CVAImagen::LoadBMP(szFileName);
								if(!g_pImage)
									MessageBox(hWnd,L"Tipo DIB no soportado\r\nConsulte a su programador",L"Error",MB_ICONERROR);
								break;
							}
						default:
							{
								MessageBox(hWnd,L"Tipo de archivo no soportado",L"Error",MB_ICONERROR);
								break;
							}
						}
					}		
					//return 0;
					break;
				}
			case IDM_FILE_SAVE:
				{
					OPENFILENAMEA ofn;
					memset(&ofn, 0, sizeof(OPENFILENAMEA));
					char szFileName[1024];
					szFileName[0]=0;
					ofn.hInstance=hInst;
					ofn.hwndOwner=hWnd;
					ofn.lStructSize=sizeof(OPENFILENAME);
					ofn.lpstrFile=szFileName;
					ofn.lpstrFilter="Archivos RAW (*.raw)\0*.raw\0Archivos BMP (*.bmp)\0*.bmp\0Todos los archivos (*.*)\0*.*\0\0";
					ofn.nMaxFile=1023;
					if(GetSaveFileNameA(&ofn))
					{
						CVAImagen::SaveToFile(g_pImage,szFileName);
					}
					//return 0;
					break;
				}
			case IDM_FILE_PRINT:
				{
					PRINTDLG dlg;
					memset(&dlg,0,sizeof(PRINTDLG));
					dlg.hwndOwner=hWnd;
					dlg.hInstance=hInst;
					dlg.Flags=PD_RETURNDC;
					dlg.lStructSize=sizeof(PRINTDLG);

					if(PrintDlg(&dlg))
					{
						static DOCINFO di= {sizeof (DOCINFO), TEXT ("FormFeed")};
						StartDoc(dlg.hDC,&di);
						StartPage(dlg.hDC);
						g_pImage->DrawImage(dlg.hDC,0,0,640,480);
						EndPage(dlg.hDC);
						EndDoc(dlg.hDC);
						DeleteDC(dlg.hDC);
					}	
					break;
					//return 0;
				}
			case ID_VIDEOCAPTURE_RUN:
				{
					if(!g_bVideoCapture)
					{
						PlayVideo(hWnd);
						RestorePipeline();
						g_bVideoCapture	= true;
						g_bImageView	= false;
					}
					break;
				}
			case ID_VIDEOCAPTURE_STOP:
				{
					if(g_bVideoCapture)
					{
						StopVideo(hWnd);
						g_bVideoCapture	= false;	
					}
					break;
				}
			case ID_OPERATIONS_CONVERT_CMY:
				{
					CVAImagen* pCMY=g_pImage->ConvertCMY();
					CVAImagen::DestroyImage(g_pImage);
					g_pImage=pCMY;
					break;
				}
			case ID_OPERATIONS_CONVERT_BW:
				{
					//////////////////////////////
					//		Usamos el CPU		//
					//////////////////////////////

					//CVAImagen* pBW=g_pImage->ConvertBW();
					//CVAImagen::DestroyImage(g_pImage);
					//g_pImage=pBW;

					//////////////////////////////////////////////////////////////////////////////////
					//		Usamos el GPU. Indicamos a la tarjeta de video que use un shader		//
					//////////////////////////////////////////////////////////////////////////////////

					CGPUView* pViewInput=CGPUView::CreateView(&g_Manager,g_pImage);						//Creamos vista de lectura
					pViewInput->SetRead(0);
					CGPUView* pViewOutput=CGPUView::CreateView(&g_Manager,g_pImage);						//Creamos vista de escritura
					pViewOutput->SetReadWrite(0);
					g_Manager.GetContext()->CSSetShader(g_Manager.m_mapComputeShaders[100].m_pICS,NULL,NULL);
					g_Manager.GetContext()->Dispatch((g_pImage->m_ulSizeX+15)/16,(g_pImage->m_ulSizeY+15)/16,1);
					pViewOutput->Update();				//Este metodo llevo los datos de la vista de salida hasta la imagen
					CGPUView::DestroyView(pViewInput);
					CGPUView::DestroyView(pViewOutput);

					break;
				}
			case ID_OPERATIONS_FILTER_NONE:
				{	
					D3DXMATRIX g_kernel  (	0, 0, 0 ,0,
											0, 1, 0, 0,
											0, 0, 0, 0,
											0, 0, 0, 0);

					D3DXVECTOR4 g_C(0,0,0,0);

					g_P.C=g_C;
					g_P.M=g_kernel;
					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_SOBEL_HORIZONTAL:
				{
					D3DXMATRIX g_kernel  ( -1,-2,-1, 0,
										    0, 0, 0, 0,
											1, 2, 1, 0,
											0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_SOBEL_VERTICAL:
				{
					D3DXMATRIX g_kernel  (	-1, 0, 1, 0,
										    -2, 0, 2, 0,
											-1, 0, 1, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_PREWIT_HORIZONTAL:
				{
					D3DXMATRIX g_kernel  (	 1, 1, 1, 0,
										     0, 0, 0, 0,
											-1,-1,-1, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_PREWIT_VERTICAL:
				{
					D3DXMATRIX g_kernel  (	 1, 0,-1, 0,
										     1, 0,-1, 0,
											 1, 0,-1, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_NORTH:
				{
					D3DXMATRIX g_kernel  (	 0,-1, 0, 0,
										     0, 0, 0, 0,
											 0, 1, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_SOUTH:
				{
					D3DXMATRIX g_kernel  (	 0, 1, 0, 0,
										     0, 0, 0, 0,
											 0,-1, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_EAST:
				{
					D3DXMATRIX g_kernel  (	 0, 0, 0, 0,
										     1, 0,-1, 0,
											 0, 0, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_WEST:
				{
					D3DXMATRIX g_kernel  (	 0, 0, 0, 0,
										    -1, 0, 1, 0,
											 0, 0, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_NORTHEAST:
				{
					D3DXMATRIX g_kernel  (	 0, 0,-1, 0,
										     0, 0, 0, 0,
											 1, 0, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_SOUTHEAST:
				{
					D3DXMATRIX g_kernel  (	 1, 0, 0, 0,
										     0, 0, 0, 0,
											 0, 0,-1, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_NORTHWEST:
				{
					D3DXMATRIX g_kernel  (	-1, 0, 0, 0,
										     0, 0, 0, 0,
											 0, 0, 1, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_EMBOSS_SOUTHWEST:
				{
					D3DXMATRIX g_kernel  (	 0, 0, 1, 0,
										     0, 0, 0, 0,
											-1, 0, 0, 0,
											 0, 0, 0, 0);

					D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
					g_P.C=g_C;
					g_P.M=g_kernel;

					break;
				}
			case ID_OPERATIONS_FILTER_EDGEDETECTION_LAPLACIAN:
				{
					D3DXMATRIX g_kernel  (	0, 1, 0, 0,
											1,-4, 1, 0,
											0, 1, 0, 0,
											0, 0, 0, 0);

						D3DXVECTOR4 g_C(0.5,0.5,0.5,0.5);
					
						g_P.C=g_C;
						g_P.M=g_kernel;

						break;	
				}
			case ID_OPERATIONS_FILTER_SMOOTHING_MEDIA:
				{
					D3DXMATRIX g_kernel  (	1, 1, 1, 0,
											1, 1, 1, 0,
											1, 1, 1, 0,
											0, 0, 0, 0);
					g_kernel=g_kernel/9;

						D3DXVECTOR4 g_C(0,0,0,0);
					
						g_P.C=g_C;
						g_P.M=g_kernel;

						break;	
				}
				case ID_OPERATIONS_FILTER_SMOOTHING_GAUSSIAN:
				{
					D3DXMATRIX g_kernel  (	1, 2, 1, 0,
											2, 4, 2, 0,
											1, 2, 1, 0,
											0, 0, 0, 0);
					g_kernel=g_kernel/16;

						D3DXVECTOR4 g_C(0,0,0,0);
					
						g_P.C=g_C;
						g_P.M=g_kernel;

						break;	
				}
			case ID_OPERATIONS_FILTER_INSERT:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_INSERT_FILTER), g_hWnd, InsertFilter);
					break;
				}
			case ID_OPERATIONS_RESTORE:
				{
					RestorePipeline();
					break;
				}
			case ID_OPERATION_SEGMENTATION_THRESHOLD_MULTIPLE:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_SMOOTHING_HISTOGRAM), g_hWnd, SmoothingHistogram);
					break;
				}
			case ID_3DSCAN_START:
				{
					g_bScanActive = true;

					if(!g_bVideoCapture)
					{
						PlayVideo(hWnd);
						RestorePipeline();
						g_bVideoCapture	= true;
						g_bImageView	= false;
					}
					break;
				}
			case ID_3DSCAN_STOP:
				{
					g_ulNumberScans = 0;
					g_bScanActive = false;

					if(g_bVideoCapture)
					{
						StopVideo(hWnd);
						g_bVideoCapture	= false;	
					}
					break;
				}
			default:
				{
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
			}

			if(g_bImageView)
			{	
				Pipeline();
				InvalidateRect(hWnd,NULL,false);
			}

			return 0;
		}
	case WM_SIZE:
		{
			if(g_pIVC)
			{
				RECT rc;
				GetClientRect(hWnd,&rc);	//Calcula el tamaño del area cliente "pantalla blanca"
				rc.left = 0;
				rc.top = 0;
				rc.right = rc.right/3;
				rc.bottom = rc.bottom/2;
				g_pIVC->SetPreviewWindowPosition(&rc);
			}
			break;
		}
	case WM_PAINT:
		{
			hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code here...
			{
				

				SetStretchBltMode(hdc,HALFTONE);
				RECT rcClient;
				GetClientRect(hWnd,&rcClient);	//Calcula el tamaño del area cliente "pantalla blanca"
				int cx,cy;
				cx=rcClient.right/3;
				cy=rcClient.bottom/2;

				EnterCriticalSection(&g_cs);

				//	Primer Cuadrante
				if(g_pImage)
					g_pImage->DrawImage(hdc,cx*0,cy*0,cx,cy);

				//	Segundo Cuadrante
				if(g_pTransformedImage)
					g_pTransformedImage->DrawImage(hdc,cx*1,cy*0,cx,cy);

				//	Tercer Cuadrante
				if(g_pConvolvedImage)
					g_pConvolvedImage->DrawImage(hdc,cx*2,cy*0,cx,cy);

				//	Cuarto Cuadrante
				if(g_pGraficoHistograma)
					g_pGraficoHistograma->DrawImage(hdc,cx*0,cy*1,cx,cy);

				//	Quinto Cuadrante
				if(g_pSegmentation)
					g_pSegmentation->DrawImage(hdc,cx*1,cy*1,cx,cy);

				//	Sexto Cuadrante
				if(g_pScan)
					g_pScan->DrawImage(hdc,cx*2,cy*0,cx,cy);

				//	Sexto Cuadrante
				if(g_pDepthMap)
					g_pDepthMap->DrawImage(hdc,cx*2,cy*0,cx,cy);

				LeaveCriticalSection(&g_cs);
			}

			EndPaint(hWnd, &ps);
			break;
		}
	case WM_DESTROY:
		{
			if(g_pIVC)
			{
				g_pIVC->Stop();
				if(g_pCapture)
					delete g_pCapture;
				DestroyAtWareVideoCapture(g_pIVC);
			}

			if(g_pGraficoHistograma)
				CVAImagen::DestroyImage(g_pGraficoHistograma);

			if(g_pTransformedImage)
				CVAImagen::DestroyImage(g_pTransformedImage);
					
			if(g_pConvolvedImage)
				CVAImagen::DestroyImage(g_pConvolvedImage);
					
			if(g_pImage)
				CVAImagen::DestroyImage(g_pImage);

			if(g_pCopiaImagen)
				CVAImagen::DestroyImage(g_pCopiaImagen);

			if(g_pGraficoHistograma)
				CVAImagen::DestroyImage(g_pGraficoHistograma);

			if(g_pSegmentation)
				CVAImagen::DestroyImage(g_pSegmentation);

			if(g_pScan)
				CVAImagen::DestroyImage(g_pScan);

			if(g_pDepthMap)
				CVAImagen::DestroyImage(g_pDepthMap);
					
			g_Manager.Uninitialize();
			PostQuitMessage(0);
			break;
		}
	default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}
// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:

		return (INT_PTR)TRUE;

	case WM_COMMAND:

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
INT_PTR CALLBACK InsertFilter(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			return (INT_PTR)TRUE;
		}
	case WM_COMMAND:
		{
			if (LOWORD(wParam) == ID_CANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			if (LOWORD(wParam) == ID_OK)
			{
				char buffer[256];
				float fMatrix[3][3];
				float fConstant[4];

				GetDlgItemTextA(hDlg,IDC_MATRIX_00,buffer,255);
				fMatrix[0][0] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_10,buffer,255);
				fMatrix[1][0] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_20,buffer,255);
				fMatrix[2][0] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_01,buffer,255);
				fMatrix[0][1] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_11,buffer,255);
				fMatrix[1][1] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_21,buffer,255);
				fMatrix[2][1] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_02,buffer,255);
				fMatrix[0][2] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_12,buffer,255);
				fMatrix[1][2] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_MATRIX_22,buffer,255);
				fMatrix[2][2] = atof(buffer);
				
				D3DXMATRIX g_kernel (	fMatrix[0][0],fMatrix[1][0],fMatrix[2][0],0,
										fMatrix[0][1],fMatrix[1][1],fMatrix[2][1],0,
										fMatrix[0][2],fMatrix[1][2],fMatrix[2][2],0,
										0,0,0,0);

				GetDlgItemTextA(hDlg,IDC_CONSTANT_0,buffer,255);
				fConstant[0] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_CONSTANT_1,buffer,255);
				fConstant[1] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_CONSTANT_2,buffer,255);
				fConstant[2] = atof(buffer);
				GetDlgItemTextA(hDlg,IDC_CONSTANT_3,buffer,255);
				fConstant[3] = atof(buffer);

				D3DXVECTOR4 g_C (fConstant[0],fConstant[1],fConstant[2],fConstant[3]);

				GetDlgItemTextA(hDlg,IDC_DIVISOR,buffer,255);

				g_kernel=g_kernel/atof(buffer);

				g_P.C=g_C;
				g_P.M=g_kernel;
				
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
		}
	}
	return (INT_PTR)FALSE;
}
INT_PTR CALLBACK SmoothingHistogram(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			HWND hCombo;
			char *SizeWindow[] = {"1","3","5","7","9",
								  "11","13","15","17","19",
								  "21","23","25","27","29",
								  "31","33","35","37","39",
								  "41","43","45","47","49"};

			char *Pixel[] = {"Red","Green","Blue"};

			char *RepeatSmoothing[] = {	"1","2","3","4","5",
										"6","7","8","9","10"};

			for(int i=0; i<25; i++)
			{
				hCombo = GetDlgItem(hDlg, IDC_SIZE_WINDOW_SMOOTHING);
				SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)SizeWindow[i]);

				//hCombo = GetDlgItem(hDlg, IDC_SIZE_WINDOW_MINIMA);
				//SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)SizeWindow[i]);

				if(i<3)
				{
					hCombo = GetDlgItem(hDlg, IDC_SIZE_WINDOW_COLOR);
					SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)Pixel[i]);
				}
				if(i<10)
				{
					hCombo = GetDlgItem(hDlg, IDC_REPEAT_SMOOTHING);
					SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)RepeatSmoothing[i]);
				}
			}
			return (INT_PTR)TRUE;
		}
	case WM_COMMAND:
		{
			int nSelectColor	= 4;
			if(LOWORD(wParam) == IDC_HISTOGRAM_SMOOTHING)
			{
				g_bSmoothing = SendMessageA(GetDlgItem(hDlg, IDC_HISTOGRAM_SMOOTHING), BM_GETCHECK, 0, 0);		
				EnableWindow(GetDlgItem(hDlg, IDC_SIZE_WINDOW_SMOOTHING), g_bSmoothing);
				EnableWindow(GetDlgItem(hDlg, IDC_SIZE_WINDOW_COLOR), g_bSmoothing);
				EnableWindow(GetDlgItem(hDlg, IDC_REPEAT_SMOOTHING), g_bSmoothing);
			}
			if(LOWORD(wParam) == IDC_HISTOGRAM_MINIMA)
			{
				g_bMinima = SendMessageA(GetDlgItem(hDlg, IDC_HISTOGRAM_MINIMA), BM_GETCHECK, 0, 0);		
				EnableWindow(GetDlgItem(hDlg, IDC_MINIMA_UMBRAL), g_bMinima);
			}
			if (LOWORD(wParam) == ID_CANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			if (LOWORD(wParam) == ID_OK_MULTIPLE_THRESHOLD)
			{
				char buffer[256];

				//	Guarda estatus en variables boleanas
				g_bSmoothing = SendMessageA(GetDlgItem(hDlg, IDC_HISTOGRAM_SMOOTHING), BM_GETCHECK, 0, 0);

				//	Guarda seleccionado en variables enteras
				g_nSizeWindowSmooting = SendMessageA(GetDlgItem(hDlg, IDC_SIZE_WINDOW_SMOOTHING), CB_GETCURSEL, 0, 0);

				//	Guarda seleccionado en variables enteras
				g_nRepeatSmoothing = SendMessageA(GetDlgItem(hDlg, IDC_REPEAT_SMOOTHING), CB_GETCURSEL, 0, 0);

				//	Guarda seleccionado en variables enteras
				g_nColorHistograma = SendMessageA(GetDlgItem(hDlg, IDC_SIZE_WINDOW_COLOR), CB_GETCURSEL, 0, 0);

				//	Guarda estatus en variables boleanas
				g_bMinima = SendMessageA(GetDlgItem(hDlg, IDC_HISTOGRAM_MINIMA), BM_GETCHECK, 0, 0);

				//	Guarda seleccionado en variables enteras
				//	g_nSizeWindowMinima = SendMessageA(GetDlgItem(hDlg, IDC_SIZE_WINDOW_MINIMA), CB_GETCURSEL, 0, 0);

				GetDlgItemTextA(hDlg,IDC_MINIMA_UMBRAL,buffer,255);
				g_fThresholdMinima = atof(buffer);
				
				//	Guarda estatus en variables boleanas
				g_bMultiThreshold = SendMessageA(GetDlgItem(hDlg, IDC_MULTIPLE_THRESHOLD), BM_GETCHECK, 0, 0);

				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
		}
	}
	return (INT_PTR)FALSE;
}
