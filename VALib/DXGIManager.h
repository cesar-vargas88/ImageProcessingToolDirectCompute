#pragma once
#include <Windows.h>
#include <DXGI.h>
#include <D3D11.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) {if((x)){ (x)->Release();(x)=NULL;}}
#endif

class CDXGIManager
{
public:
	//Descriptor de la cadena de superficies
	DXGI_SWAP_CHAIN_DESC m_dscd; 
protected:
	//Permitirá organizar la cadena de superficies
	IDXGISwapChain* m_pISwapChain;
	//Se analizará en 5°
	ID3D11Device*   m_pID3D11Dev;
	//Feature Level Actual
	D3D_FEATURE_LEVEL m_FeatureLevel;
	//Configuración de calidad y número de muestras por pixel
	DXGI_SAMPLE_DESC m_MultiSampleConfig;
	//Se analizará en 5°
	ID3D11DeviceContext* m_pID3D11DevContext;
	//Referencia al Backbuffer
	ID3D11Texture2D* m_pID3D11Texture2D; 
	ID3D11Texture2D* m_pID3D11SystemMem;
	virtual void PerFeatureLevel();
	DXGI_ADAPTER_DESC m_AdapterDesc;
public:
	bool InitializeSwapChain(HWND hWnd,bool bFullScreen=false,bool bUseGPU=true,bool bEnableAntialiasing=false,unsigned int uiNumberOfSamplesPerPixel=4,unsigned int uiSampleQuality=0);
	virtual void Uninitialize(void);
	virtual void Resize(int cx,int cy);
	CDXGIManager(void);
	~CDXGIManager(void);
	ID3D11Device* GetDevice(){return m_pID3D11Dev;};
	ID3D11DeviceContext* GetContext(){return m_pID3D11DevContext;};
	IDXGISwapChain*	GetSwapChain(){return m_pISwapChain;};
};

