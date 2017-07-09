#include "StdAfx.h"
#include "DXGIManager.h"


CDXGIManager::CDXGIManager(void)
{
	m_pISwapChain=NULL;
	m_pID3D11Dev=NULL;
	m_pID3D11DevContext=NULL;
	m_pID3D11SystemMem=NULL;
	m_pID3D11Texture2D=NULL;
	m_FeatureLevel=(D3D_FEATURE_LEVEL)0;
	memset(&m_AdapterDesc,0,sizeof(DXGI_ADAPTER_DESC));
	m_MultiSampleConfig.Count=1;
	m_MultiSampleConfig.Quality=0;
	memset(&m_dscd,0,sizeof(DXGI_SWAP_CHAIN_DESC));

	m_dscd.BufferDesc.RefreshRate.Numerator=60;
	m_dscd.BufferDesc.RefreshRate.Denominator=1;
	m_dscd.BufferDesc.Scaling=DXGI_MODE_SCALING_UNSPECIFIED;
	m_dscd.BufferDesc.ScanlineOrdering=DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	m_dscd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
	m_dscd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
	m_dscd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
	m_dscd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	m_dscd.Windowed=true;

}
CDXGIManager::~CDXGIManager(void)
{

}
bool CDXGIManager::InitializeSwapChain(HWND hWnd, bool bFullScreen,bool bUseGPU,bool bEnableAntialising,unsigned int uiNumberOfSamplesPerPixel,unsigned int uiSampleQuality )
{
	RECT rc;
	GetClientRect(hWnd,&rc);
	if(bEnableAntialising)
	{
		m_MultiSampleConfig.Count=uiNumberOfSamplesPerPixel;
		m_MultiSampleConfig.Quality=uiSampleQuality;
	}
	else
	{
		m_MultiSampleConfig.Count=1;
		m_MultiSampleConfig.Quality=0;	
	}
	m_dscd.BufferCount=1; //Un backbuffer
	if(!bFullScreen)
	{
		m_dscd.BufferDesc.Height=rc.bottom;
		m_dscd.BufferDesc.Width=rc.right;
	}
	//60Hz
	m_dscd.OutputWindow=hWnd;
	m_dscd.SampleDesc=m_MultiSampleConfig;
	m_dscd.Windowed=!bFullScreen;
	HRESULT hr;
	if(bUseGPU)
		hr=D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&m_dscd,&m_pISwapChain,&m_pID3D11Dev,&m_FeatureLevel,&m_pID3D11DevContext);
	else
		hr=D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&m_dscd,&m_pISwapChain,&m_pID3D11Dev,&m_FeatureLevel,&m_pID3D11DevContext);
	if(FAILED(hr))
	{
		MessageBox(hWnd,L"Unable to initialize DirectX 11 DXGI",
			L"Error",MB_ICONERROR);
		return false;
	}
	IDXGIDevice * pDXGIDevice;
	hr = m_pID3D11Dev->QueryInterface(IID_IDXGIDevice, (void **)&pDXGIDevice);
      
	IDXGIAdapter * pDXGIAdapter;
	hr = pDXGIDevice->GetParent(IID_IDXGIAdapter, (void **)&pDXGIAdapter);
	pDXGIAdapter->GetDesc(&m_AdapterDesc);
	printf("Información sobre el dispositivo de gráfico:\n");
	wprintf(TEXT("Adatador Gráfico:%s\n"),m_AdapterDesc.Description);
	wprintf(TEXT("Memoria Dedicada de Video:\t%8uMB\n"),m_AdapterDesc.DedicatedVideoMemory/(1024*1024));
	wprintf(TEXT("Memoria Dedicada de Sistema:\t%8uMB\n"),m_AdapterDesc.DedicatedSystemMemory/(1024*1024));
	wprintf(TEXT("Memoria Compartida de Sistema:\t%8uMB\n"),m_AdapterDesc.SharedSystemMemory/(1024*1024));
	SAFE_RELEASE(pDXGIAdapter);
	SAFE_RELEASE(pDXGIDevice);
	PerFeatureLevel();
	return true;
}
void CDXGIManager::Resize(int cx,int cy)
{
	HRESULT hr;
	if(m_pISwapChain)
	{
		if(cx && cy)
		{
			SAFE_RELEASE(m_pID3D11Texture2D);
			SAFE_RELEASE(m_pID3D11SystemMem);
			m_pID3D11DevContext->ClearState();
			hr=m_pISwapChain->ResizeBuffers(1,cx,cy,m_dscd.BufferDesc.Format,0);	
			//Extraer Referencia al Backbuffer
			hr=m_pISwapChain->GetBuffer(0,IID_ID3D11Texture2D,(void**)&m_pID3D11Texture2D);
			//Crear el bloque de memoria de sistema sobre el cual vamos a escribir (DrawImage)
			D3D11_TEXTURE2D_DESC dtd;
			memset(&dtd,0,sizeof(D3D11_TEXTURE2D_DESC));
			dtd.Width=cx;
			dtd.Height=cy;
			dtd.Format=m_dscd.BufferDesc.Format;
			dtd.ArraySize=1;
			dtd.MipLevels=1;
			dtd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE|D3D11_CPU_ACCESS_READ;
			dtd.Usage=D3D11_USAGE_STAGING;
			dtd.SampleDesc.Count=1;
			dtd.SampleDesc.Quality=0;
			hr=m_pID3D11Dev->CreateTexture2D(&dtd,NULL,&m_pID3D11SystemMem);
			
		}
	}
}
void CDXGIManager::PerFeatureLevel()
{
	switch(m_FeatureLevel)
	{
	case D3D_FEATURE_LEVEL_11_0:
		printf("%s","DirectX 11: Feature Level 11.0 Shader Model 5.0 Activado\n");
		break;
	case D3D_FEATURE_LEVEL_10_1:
		printf("%s","DirectX 11: Feature Level 10.1 Shader Model 4.1 Activado\n");
		break;
	case D3D_FEATURE_LEVEL_10_0:
		printf("%s","DirectX 11: Feature Level 10.0 Shader Model 4.0 Activado\n");
		break;
	case D3D_FEATURE_LEVEL_9_3:
		printf("%s","DirectX 11: Feature Level 9.3 Shader Model 4.0 10Level9.3 Activado\n");
		break;
	case D3D_FEATURE_LEVEL_9_2:
		printf("%s","DirectX 11: Feature Level 9.2 Shader Model 4.0 10Level9.2 Activado\n");
		break;
	case D3D_FEATURE_LEVEL_9_1:
		printf("%s","DirectX 11: Feature Level 9.1 Shader Model 4.0 10Level9.1 Activado\n");
		break;
	}
}
void CDXGIManager::Uninitialize()
{
	if(m_pISwapChain)
		m_pISwapChain->SetFullscreenState(false,0);
	SAFE_RELEASE(m_pID3D11SystemMem);
	SAFE_RELEASE(m_pID3D11Texture2D);
	SAFE_RELEASE(m_pID3D11DevContext);
	SAFE_RELEASE(m_pID3D11Dev);
	SAFE_RELEASE(m_pISwapChain);
}
