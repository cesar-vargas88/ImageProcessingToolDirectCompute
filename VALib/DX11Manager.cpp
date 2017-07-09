#include "StdAfx.h"
#include "DX11Manager.h"
#include <D3D11.h>
#include <D3DX11.h>
#include "DX11TexturePool.h"

CDX11Manager::CDX11Manager(void)
{
	m_lResourceIDGenerator=100;
	m_pIVSConstantBuffer=NULL;
	m_pIPSConstantBuffer=NULL;
	m_pICSConstantBuffer=NULL;
	m_pszVertexShaderProfile="Desconocido";
	m_pszPixelShaderProfile="Desconocido";
	m_pszComputeShaderProfile="Desconocido";
	//Reservar los slots del RTV y Textura del backbuffer, y DepthStencil.
	m_mapTextures.insert(make_pair(0,(ID3D11Texture2D*)NULL));
	m_mapRenderTargetViews.insert(make_pair(0,(ID3D11RenderTargetView*)NULL));
	//Reservar los slots del SRV, DSV y la textura del buffer
	m_mapTextures.insert(make_pair(1,(ID3D11Texture2D*)NULL));
	m_mapDepthStencilViews.insert(make_pair(1,(ID3D11DepthStencilView*)NULL));
	m_mapShaderResourceViews.insert(make_pair(1,(ID3D11ShaderResourceView*)NULL));
}
CDX11Manager::~CDX11Manager(void)
{
}
void CDX11Manager::PerFeatureLevel()
{
	__super::PerFeatureLevel();
	switch(m_FeatureLevel)
	{
	case D3D_FEATURE_LEVEL_11_0:
		m_pszVertexShaderProfile="vs_5_0";
		m_pszPixelShaderProfile="ps_5_0";
		m_pszComputeShaderProfile="cs_5_0";
		break;
	case D3D_FEATURE_LEVEL_10_1:
		m_pszVertexShaderProfile="vs_4_1";
		m_pszPixelShaderProfile="ps_4_1";
		m_pszComputeShaderProfile="cs_4_1";
		break;
		
	case D3D_FEATURE_LEVEL_10_0:
		m_pszVertexShaderProfile="vs_4_0";
		m_pszPixelShaderProfile="ps_4_0";
		m_pszComputeShaderProfile="cs_4_0";
		break;
	case D3D_FEATURE_LEVEL_9_3:
		m_pszVertexShaderProfile="vs_4_0_level_9_3";
		m_pszPixelShaderProfile="ps_4_0_level_9_3";
		break;
	
	case D3D_FEATURE_LEVEL_9_2:
	case D3D_FEATURE_LEVEL_9_1:
		m_pszVertexShaderProfile="vs_4_0_level_9_1";
		m_pszPixelShaderProfile="ps_4_0_level_9_1";
		break;
	}
	D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts = { 0 } ;
    m_pID3D11Dev->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
    if ( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
	{
		printf("%s\n","El dispositivo actual no es compatible con DirectCompute Shader Model 4.X");
		m_pszComputeShaderProfile="No soportado";
	}
	printf("Perfiles de sombreador activados:\nPixel Shader=%s\nVertex Shader=%s\nCompute Shader:%s\n",
		m_pszPixelShaderProfile,m_pszVertexShaderProfile,m_pszComputeShaderProfile);
}
void CDX11Manager::BeginResourcesFrame(void)
{
	m_stkResourcesFrames.push(m_lResourceIDGenerator);
}
void CDX11Manager::EndResourcesFrame(void)
{
	if(m_stkResourcesFrames.size())
	{
		long lID=m_lResourceIDGenerator-1;
		m_lResourceIDGenerator=m_stkResourcesFrames.top();
		m_stkResourcesFrames.pop();
		while(m_lResourceIDGenerator<lID)
		{
			REMOVE_RESOURCE(m_mapTextures,lID);
			REMOVE_RESOURCE(m_mapRenderTargetViews,lID);
			REMOVE_RESOURCE(m_mapShaderResourceViews,lID);
			REMOVE_RESOURCE(m_mapUnorderedAccessViews,lID);
			REMOVE_RESOURCE(m_mapSamplers,lID);
			REMOVE_RESOURCE(m_mapRasterizers,lID);
			REMOVE_RESOURCE(m_mapBlendStates,lID);
			REMOVE_RESOURCE(m_mapVertexBuffers,lID);
			REMOVE_RESOURCE(m_mapIndexBuffers,lID);
			{
				auto it=m_mapVertexShaders.find(lID);
				if(it!=m_mapVertexShaders.end())
				{
					SAFE_RELEASE(it->second.m_pIA);
					SAFE_RELEASE(it->second.m_pIVS);
					SAFE_RELEASE(it->second.m_pIVSCode);
					m_mapVertexShaders.erase(it);
				}
			}
			{
				auto it=m_mapPixelShaders.find(lID);
				if(it!=m_mapPixelShaders.end())
				{
					SAFE_RELEASE(it->second.m_pIPS);
					SAFE_RELEASE(it->second.m_pIPSCode);
					m_mapPixelShaders.erase(it);
				}
			}
			lID--;
		}
	}
}
long CDX11Manager::CreateResourceID(void)
{
	return m_lResourceIDGenerator++;
}
CTexturePool* CDX11Manager::CreateTexturePool()
{
	return new CDX11TexturePool(this);
}
bool CDX11Manager::InitializeDX11()
{
	HRESULT hr;
	if(!m_pID3D11Dev)
		return false;
	//Configurar el estado de los Rasterizadores de hardware
	D3D11_RASTERIZER_DESC drd;
	memset(&drd,0,sizeof(D3D11_RASTERIZER_DESC));
	drd.CullMode=D3D11_CULL_BACK;   //Recortar polígonos traceros
	drd.FillMode=D3D11_FILL_SOLID;  //Relleno sólido
	drd.FrontCounterClockwise=false; //Los polígonos delanteros son aquellos cuyos vértices se disponen en el sentido de las manecillas del reloj
	drd.MultisampleEnable=true;     //Multi muestreo desactivado
	drd.AntialiasedLineEnable=true;  //Las líneas se muestran con antialiasing (Mediante Alpha Blending)
	ID3D11RasterizerState *pRS=NULL;
	hr=m_pID3D11Dev->CreateRasterizerState(&drd,&pRS);
	if(FAILED(hr)) return false;
	m_pID3D11DevContext->RSSetState(pRS);
	m_mapRasterizers.insert(make_pair(0,pRS));
	//Configurar los samplers que serán utilizados
	ID3D11SamplerState* pSS=NULL;
	D3D11_SAMPLER_DESC dss;
	memset(&dss,0,sizeof(D3D11_SAMPLER_DESC));
	
	//Primer Sampler POINT SAMPLING
	dss.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT; //Point Sampler
	dss.AddressU=dss.AddressV=dss.AddressW=D3D11_TEXTURE_ADDRESS_WRAP; //Textura repetida en U,V y W
	dss.MaxAnisotropy=0; //0 porque se trata de un Point Sampler, por lo que no se realiza ninguna interpolación.
	dss.MaxLOD=D3D11_FLOAT32_MAX;  //Less detailed Mip-map
	dss.MinLOD=0.0f;    //Most detailed Mip-map
	dss.MipLODBias=0.0f;
	hr=m_pID3D11Dev->CreateSamplerState(&dss,&pSS);
	if(FAILED(hr)) return false;
	m_mapSamplers.insert(make_pair(0,pSS));

	//Segundo Sampler LINEAR SAMPLING
	dss.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR; //Trilinear Sampler
	hr=m_pID3D11Dev->CreateSamplerState(&dss,&pSS);
	if(FAILED(hr)) return false;
	m_mapSamplers.insert(make_pair(1,pSS));
	
	//Tercer Sampler ANISOTROPIC   //Con alta calidad de texturización en presencia de pendientes altas con respecto a la profundidad
	dss.Filter=D3D11_FILTER_ANISOTROPIC;
	dss.MaxAnisotropy=4;
	hr=m_pID3D11Dev->CreateSamplerState(&dss,&pSS);
	if(FAILED(hr)) return false;
	m_mapSamplers.insert(make_pair(2,pSS));
	
	//Cuarto Sampler LINEAR SAMPLING CLAMPED  (Para efectos convolutivos, donde la textura no deberá repetirse durante el muestreo)
	dss.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	dss.AddressU=dss.AddressV=dss.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;  //El texel del borde se repite.
	dss.MaxAnisotropy=0;
	hr=m_pID3D11Dev->CreateSamplerState(&dss,&pSS);
	if(FAILED(hr)) return false;
	m_mapSamplers.insert(make_pair(3,pSS));

	//Crear los buffers de constantes
	D3D11_BUFFER_DESC bd;
	memset(&bd,0,sizeof(D3D11_BUFFER_DESC));
	bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
	bd.Usage=D3D11_USAGE_DYNAMIC;
	//Los buffer de constantes deben tener una longitud en bytes múltiplo de 16 bytes (128 bits) 
	bd.ByteWidth=1024;
	//Los CS,VS y PS Constant buffers.
	hr=m_pID3D11Dev->CreateBuffer(&bd,NULL,&m_pICSConstantBuffer);
	if(FAILED(hr)) return false;
	hr=m_pID3D11Dev->CreateBuffer(&bd,NULL,&m_pIVSConstantBuffer);
	if(FAILED(hr)) return false;
	hr=m_pID3D11Dev->CreateBuffer(&bd,NULL,&m_pIPSConstantBuffer);
	if(FAILED(hr)) return false;


	//MEZCLADO DE COMBINACIÓN ALPHA Channel


	D3D11_BLEND_DESC bsd;
	ID3D11BlendState* pIBlendState=NULL;
	memset(&bsd,0,sizeof(D3D11_BLEND_DESC));
	bsd.AlphaToCoverageEnable=FALSE;
	bsd.IndependentBlendEnable=FALSE;
	bsd.RenderTarget[0].BlendEnable=TRUE;
	bsd.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
	// Color=ColorSrc*(as) + ColorDest*(1-as)
	bsd.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
	bsd.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
	bsd.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
	// Alpha=AlphaSrc*(as) + AlphaDest*(1-as)
	bsd.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_SRC_ALPHA;
	bsd.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
	//Modificar todos los canales (rgb)
	bsd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
	hr=m_pID3D11Dev->CreateBlendState(&bsd,&pIBlendState);
	if(FAILED(hr)) return false;
	m_mapBlendStates.insert(make_pair(0,pIBlendState));



	// Configuración del modo de operación del stencil y del buffer de profundidad.
	D3D11_DEPTH_STENCIL_DESC dsd;
	ID3D11DepthStencilState* pID3D11DepthStencilState=0;
	memset(&dsd,0,sizeof(D3D11_DEPTH_STENCIL_DESC));
	
	dsd.DepthEnable = true;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dsd.StencilEnable = true;
	dsd.StencilReadMask = 0xFF;
	dsd.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	// Stencil operations if pixel is back-facing.
	dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	hr=m_pID3D11Dev->CreateDepthStencilState(&dsd,&pID3D11DepthStencilState);
	if(FAILED(hr)) return false;
	m_mapDepthStencilStates.insert(make_pair(0,pID3D11DepthStencilState));

	return true;
}
void CDX11Manager::Resize(int cx,int cy)
{
	HRESULT hr=E_FAIL;
	if(!m_pID3D11Dev)
		return;
	if(!(cx && cy)) return;

	//Render target Backbuffer
	SAFE_RELEASE(m_mapRenderTargetViews[0]);
	SAFE_RELEASE(m_mapTextures[0]);

	//Shader Resource and DepthStencil
	SAFE_RELEASE(m_mapShaderResourceViews[1]);
	SAFE_RELEASE(m_mapDepthStencilViews[1]);
	SAFE_RELEASE(m_mapTextures[1]);

	//Hacer trabajo de la clase base
	__super::Resize(cx,cy);
	
	//Manterner una referencia hacia la abstracción de memoria del backbuffer
	m_mapTextures[0]=m_pID3D11Texture2D;
	m_pID3D11Texture2D->AddRef();
	hr=m_pID3D11Dev->CreateRenderTargetView(m_mapTextures[0],NULL,&(m_mapRenderTargetViews[0]));

	//Crear el buffer de profundidad
	D3D11_TEXTURE2D_DESC dbd;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
	
	memset(&dbd,0,sizeof(D3D11_TEXTURE2D_DESC));
	memset(&dsvd,0,sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
	memset(&srvd,0,sizeof(srvd));
	
	dbd.Width=cx;
	dbd.Height=cy;
	dbd.Format=DXGI_FORMAT_R24G8_TYPELESS;
	dbd.MipLevels=1;
	dbd.ArraySize=1;
	dbd.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
	dbd.SampleDesc=m_MultiSampleConfig;
	
	hr=m_pID3D11Dev->CreateTexture2D(&dbd,NULL,&m_mapTextures[1]);
	if(FAILED(hr))
	{
		wprintf(TEXT("Unable to Create Depth Buffer Surface"));
	}
	//Inicializar el descriptores de las vistas del buffer de profundidad
	dsvd.Format =DXGI_FORMAT_D24_UNORM_S8_UINT;
	srvd.Format= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	if(m_MultiSampleConfig.Count>1)
	{
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvd.Texture2D.MipSlice = 0;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels=1;
	}
	//Crear la vista del buffer de profundidad
	hr=m_pID3D11Dev->CreateDepthStencilView(m_mapTextures[1],&dsvd,&m_mapDepthStencilViews[1]);
	if(FAILED(hr))
	{
		wprintf(TEXT("Unable to Create Stencil View"));
	}
	//Crear la vista de recurso de sombreador del Buffer de Profunfidad
	hr=m_pID3D11Dev->CreateShaderResourceView(m_mapTextures[1],&srvd,&m_mapShaderResourceViews[1]);
	if(FAILED(hr))
	{
		wprintf(TEXT("Unable to Create Stencil Shader Resource View"));
	}
	//Establecer el puerto de visión
	D3D11_VIEWPORT vpd;
	vpd.MaxDepth = 1.0f;
	vpd.MinDepth = 0.0f;
	vpd.Width = (float)(cx);
	vpd.Height = (float)(cy);
	vpd.TopLeftX = 0;
	vpd.TopLeftY = 0;
	m_pID3D11DevContext->RSSetViewports(1,&vpd);
}
void CDX11Manager::Uninitialize()
{
	SAFE_RELEASE(m_pIVSConstantBuffer);
	SAFE_RELEASE(m_pIPSConstantBuffer);
	SAFE_RELEASE(m_pICSConstantBuffer);
	for(auto it=m_mapComputeShaders.begin();it!=m_mapComputeShaders.end();it++)
	{
		SAFE_RELEASE(it->second.m_pICSCode);
		SAFE_RELEASE(it->second.m_pICS);
	}
	m_mapComputeShaders.clear();
	for(auto it=m_mapVertexShaders.begin();it!=m_mapVertexShaders.end();it++)
	{
		SAFE_RELEASE(it->second.m_pIVSCode);
		SAFE_RELEASE(it->second.m_pIA);
		SAFE_RELEASE(it->second.m_pIVS);
	}
	m_mapVertexShaders.clear();
	for(auto it=m_mapPixelShaders.begin();it!=m_mapPixelShaders.end();it++)
	{
		SAFE_RELEASE(it->second.m_pIPS);
		SAFE_RELEASE(it->second.m_pIPSCode);
	}
	m_mapPixelShaders.clear();

	for(auto it=m_mapUnorderedAccessViews.begin();it!=m_mapUnorderedAccessViews.end();it++)
	m_mapUnorderedAccessViews.clear();

	for(auto it=m_mapDepthStencilViews.begin();it!=m_mapDepthStencilViews.end();it++)
		SAFE_RELEASE(it->second);
	m_mapDepthStencilViews.clear();

	for(auto it=m_mapRenderTargetViews.begin();it!=m_mapRenderTargetViews.end();it++)
		SAFE_RELEASE(it->second);
	m_mapRenderTargetViews.clear();

	for(auto it=m_mapTextures.begin();it!=m_mapTextures.end();it++)
		SAFE_RELEASE(it->second);
	m_mapTextures.clear();

	for(auto it=m_mapShaderResourceViews.begin();it!=m_mapShaderResourceViews.end();it++)
		SAFE_RELEASE(it->second);
	m_mapShaderResourceViews.clear();

	for(auto it=m_mapRenderTargetViews.begin();it!=m_mapRenderTargetViews.end();it++)
		SAFE_RELEASE(it->second);
	m_mapRenderTargetViews.clear();

	//States
	for(auto it=m_mapRasterizers.begin();it!=m_mapRasterizers.end();it++)
		SAFE_RELEASE(it->second);
	m_mapRasterizers.clear();
	for(auto it=m_mapSamplers.begin();it!=m_mapSamplers.end();it++)
		SAFE_RELEASE(it->second);
	m_mapSamplers.clear();
	for(auto it=m_mapBlendStates.begin();it!=m_mapBlendStates.end();it++)
		SAFE_RELEASE(it->second);
	m_mapBlendStates.clear();
	for(auto it=m_mapDepthStencilStates.begin();it!=m_mapDepthStencilStates.end();it++)
		SAFE_RELEASE(it->second);
	m_mapDepthStencilStates.clear();

	//Buffers
	for(auto it=m_mapVertexBuffers.begin();it!=m_mapVertexBuffers.end();it++)
		SAFE_RELEASE(it->second);
	m_mapVertexBuffers.clear();
	for(auto it=m_mapIndexBuffers.begin();it!=m_mapIndexBuffers.end();it++)
		SAFE_RELEASE(it->second);
	m_mapIndexBuffers.clear();

	__super::Uninitialize();
}
bool CDX11Manager::CompileShader(const char* pszFileName,const char* pszShaderProfile,const char* pszEntryPoint,ID3D10Blob** ppOutShaderCode)
{
	HRESULT hr=E_FAIL;
	*ppOutShaderCode=NULL;
	DWORD dwCompileOptions=D3D10_SHADER_ENABLE_STRICTNESS;
	ID3D10Blob* pErrorMsgs=NULL;
	hr=D3DX11CompileFromFileA(pszFileName,NULL,NULL,pszEntryPoint,pszShaderProfile,dwCompileOptions,0,NULL,ppOutShaderCode,&pErrorMsgs,NULL);
	if(pErrorMsgs)
	{
		char * pszBuffer=new char[pErrorMsgs->GetBufferSize()+1];
		memset(pszBuffer,0,pErrorMsgs->GetBufferSize()+1);
		memcpy(pszBuffer,pErrorMsgs->GetBufferPointer(),pErrorMsgs->GetBufferSize());
		printf("%s",pszBuffer);
		fflush(stdout);
		SAFE_RELEASE(pErrorMsgs);
		delete [] pszBuffer;
	}
	if(FAILED(hr))
	{
		SAFE_RELEASE(pErrorMsgs);
		SAFE_RELEASE(*ppOutShaderCode);
		return false;
	}
	return true;
}
bool CDX11Manager::CompileAndCreateVertexShader(const char* pszFileName,const char* pszEntryPoint,long lVSID)
{
	HRESULT hr;
	printf("Compilando Vertex Shader:%s...\r\n",pszFileName);
	ID3D10Blob* pIShaderCode=NULL;
	if(CompileShader(pszFileName,m_pszVertexShaderProfile,pszEntryPoint,&pIShaderCode))
	{
		ID3D11VertexShader* pIVS=NULL;
		if(pIShaderCode) //Estar seguro de que hay código aceptable para crear un Shader
		{
			hr=m_pID3D11Dev->CreateVertexShader(pIShaderCode->GetBufferPointer(),pIShaderCode->GetBufferSize(),NULL,&pIVS);
			if(FAILED(hr))
			{
				printf("No se pudo crear el shader a partir del código.\r\n");
				SAFE_RELEASE(pIShaderCode);
				return false;
			}
			//Si existe un shader con ese id, actualizarlo
			auto it=m_mapVertexShaders.find(lVSID);
			if(it!=m_mapVertexShaders.end())
			{
				printf("Un vertex shader con el ID:%d ya existe. El nuevo shader reemplazará al previo.\r\n",lVSID);
				SAFE_RELEASE(it->second.m_pIVS);
				SAFE_RELEASE(it->second.m_pIVSCode);
				it->second.m_pIVSCode=pIShaderCode;
				it->second.m_pIVS=pIVS;
			}
			else //Se trata de un nuevo shader
			{
				VERTEX_SHADER VS;
				VS.m_pIVS=pIVS;
				VS.m_pIVSCode=pIShaderCode;
				VS.m_pIA=NULL;
				m_mapVertexShaders.insert(make_pair(lVSID,VS));
			}
			printf("Suceso.\r\n");
			return true;
		}
		else
		{
			printf("No se pudo crear el shader\r\n");
		}
	}
	else
		printf("No se pudo iniciar la compilación");
	SAFE_RELEASE(pIShaderCode);
	return false;
}
bool CDX11Manager::CompileAndCreatePixelShader(const char* pszFileName,const char* pszEntryPoint,long lPSID)
{
	HRESULT hr;
	printf("Compilando Pixel Shader:%s...\r\n",pszFileName);
	ID3D10Blob* pIShaderCode=NULL;
	if(CompileShader(pszFileName,m_pszPixelShaderProfile,pszEntryPoint,&pIShaderCode))
	{
		ID3D11PixelShader* pIPS=NULL;
		if(pIShaderCode) //Estar seguro de que hay código aceptable para crear un Shader
		{
			hr=m_pID3D11Dev->CreatePixelShader(pIShaderCode->GetBufferPointer(),pIShaderCode->GetBufferSize(),NULL,&pIPS);
			if(FAILED(hr))
			{
				printf("No se pudo crear el shader a partir del código.\r\n");
				SAFE_RELEASE(pIShaderCode);
				return false;
			}
			//Si existe un shader con ese id, actualizarlo
			auto it=m_mapPixelShaders.find(lPSID);
			if(it!=m_mapPixelShaders.end())
			{
				printf("Un pixel shader con el ID:%d ya existe. El nuevo shader reemplazará al previo.\r\n",lPSID);
				SAFE_RELEASE(it->second.m_pIPS);
				SAFE_RELEASE(it->second.m_pIPSCode);
				it->second.m_pIPSCode=pIShaderCode;
				it->second.m_pIPS=pIPS;
			}
			else //Se trata de un nuevo shader
			{
				PIXEL_SHADER PS;
				PS.m_pIPS=pIPS;
				PS.m_pIPSCode=pIShaderCode;
				m_mapPixelShaders.insert(make_pair(lPSID,PS));
			}
			printf("Suceso.\r\n");
			return true;
		}
		else
		{
			printf("No se pudo crear el shader\r\n");
		}
	}
	else
		printf("No se pudo iniciar la compilación");
	SAFE_RELEASE(pIShaderCode);
	return false;
}
bool CDX11Manager::CompileAndCreateComputeShader(const char* pszFileName,const char* pszEntryPoint,long lCSID)
{
	HRESULT hr;
	printf("Compilando Compute Shader:%s...\r\n",pszFileName);
	ID3D10Blob* pIShaderCode=NULL;
	if(CompileShader(pszFileName,m_pszComputeShaderProfile,pszEntryPoint,&pIShaderCode))
	{
		ID3D11ComputeShader* pICS=NULL;
		if(pIShaderCode) //Estar seguro de que hay código aceptable para crear un Shader
		{
			hr=m_pID3D11Dev->CreateComputeShader(pIShaderCode->GetBufferPointer(),pIShaderCode->GetBufferSize(),NULL,&pICS);
			if(FAILED(hr))
			{
				printf("No se pudo crear el shader a partir del código.\r\n");
				SAFE_RELEASE(pIShaderCode);
				return false;
			}
			//Si existe un shader con ese id, actualizarlo
			auto it=m_mapComputeShaders.find(lCSID);
			if(it!=m_mapComputeShaders.end())
			{
				printf("Un compute shader con el ID:%d ya existe. El nuevo shader reemplazará al previo.\r\n",lCSID);
				SAFE_RELEASE(it->second.m_pICS);
				SAFE_RELEASE(it->second.m_pICSCode);
				it->second.m_pICSCode=pIShaderCode;
				it->second.m_pICS=pICS;
			}
			else //Se trata de un nuevo shader
			{
				COMPUTE_SHADER CS;
				CS.m_pICS=pICS;
				CS.m_pICSCode=pIShaderCode;
				m_mapComputeShaders.insert(make_pair(lCSID,CS));
			}
			printf("Suceso.\r\n");
			return true;
		}
		else
		{
			printf("No se pudo crear el shader\r\n");
		}
	}
	else
		printf("No se pudo iniciar la compilación\r\n");
	SAFE_RELEASE(pIShaderCode);
	return false;
}
bool CDX11Manager::CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* pLayoutInfo,unsigned long ulInputElements,long lVSID)
{
	HRESULT hr=E_FAIL;
	auto it=m_mapVertexShaders.find(lVSID);
	if(it!=m_mapVertexShaders.end())
	{
		ID3D11InputLayout* pIA=NULL;
		hr=m_pID3D11Dev->CreateInputLayout(pLayoutInfo,ulInputElements,it->second.m_pIVSCode->GetBufferPointer(),it->second.m_pIVSCode->GetBufferSize(),&pIA);
		if(FAILED(hr))
			return false;
		it->second.m_pIA=pIA;
		return true;
	}
	return false;
}
bool CDX11Manager::LoadTexture2DAndCreateShaderResourceView(const char* pszFileName,long lSRID)
{
	HRESULT hr=E_FAIL;
	ID3D11Resource*		pIResource=NULL;
	ID3D11Texture2D*	pITexture2D=NULL;
	//------------------------- CARGAR LA TEXTURA DESDE EL ARCHIVO FUENTE ----------------------------------------
	D3DX11_IMAGE_LOAD_INFO ili;
	//Los miembros no utilizados de D3DX11_IMAGE_LOAD_INFO, deben inicializarse a D3DX11_DEFAULT (-1), excepto pSrcInfo que es NULL
	//pSrcInfo, deberá apuntar D3DX11_IMAGE_INFO* si se requiere consultar los atributos del archivo de origen. 
	memset(&ili,D3DX11_DEFAULT,sizeof(D3DX11_IMAGE_LOAD_INFO));
	ili.pSrcInfo=NULL;
	ili.Usage=D3D11_USAGE_DEFAULT;
	ili.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
	ili.BindFlags=D3D11_BIND_SHADER_RESOURCE;
	ili.CpuAccessFlags=0;
	hr=D3DX11CreateTextureFromFileA(m_pID3D11Dev,pszFileName,&ili,NULL,&pIResource,NULL);
	if(FAILED(hr)) return false;
	hr=pIResource->QueryInterface(IID_ID3D11Texture2D,(void**)&pITexture2D);
	if(FAILED(hr))
	{
		SAFE_RELEASE(pIResource);
		return false;
	}
	//Crear un Shader Resource View a partir de la textura recién cargada, con el mismo formato y número de mipmaps.
	D3D11_TEXTURE2D_DESC td;
	pITexture2D->GetDesc(&td);
	ID3D11ShaderResourceView* pISRV=NULL;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
	memset(&srvd,0,sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	srvd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Format=td.Format;
	srvd.Texture2D.MostDetailedMip=0;
	srvd.Texture2D.MipLevels=td.MipLevels;
	hr=m_pID3D11Dev->CreateShaderResourceView(pIResource,&srvd,&pISRV);
	if(FAILED(hr))
	{
		SAFE_RELEASE(pITexture2D);
		SAFE_RELEASE(pIResource);
		return false;
	}
	{
		auto it=m_mapTextures.find(lSRID);
		if(it!=m_mapTextures.end()) //Si existe un recurso con ese ID
		{
			//Liberar el recurso anterior.
			SAFE_RELEASE(m_mapTextures[lSRID]);
			m_mapTextures.erase(it);
		}
	}
	{
		auto it=m_mapShaderResourceViews.find(lSRID);
		if(it!=m_mapShaderResourceViews.end())
		{
			//Liberar la vista anterior
			SAFE_RELEASE(m_mapShaderResourceViews[lSRID]);
			m_mapShaderResourceViews.erase(it);
		}
	}
	SAFE_RELEASE(pIResource);
	m_mapTextures.insert(make_pair(lSRID,pITexture2D));
	m_mapShaderResourceViews.insert(make_pair(lSRID,pISRV));
	return true;
}
bool CDX11Manager::UpdateConstantBuffer(ID3D11Buffer* pID3D11Buffer,void* pData,unsigned long ulSize)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	D3D11_BUFFER_DESC bd;
	pID3D11Buffer->GetDesc(&bd);
	if(SUCCEEDED(m_pID3D11DevContext->Map(pID3D11Buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))
	{
		memcpy(mapped.pData,pData,min(bd.ByteWidth,ulSize));
		m_pID3D11DevContext->Unmap(pID3D11Buffer,0);
		return true;
	}
	return false;
}
bool CDX11Manager::CreateVertexBuffer(void* pInitialData,unsigned int uiStride,unsigned int uiVertexCount,long lVBID)
{
	HRESULT hr=E_FAIL;
	ID3D11Buffer* pIVB=NULL;
	D3D11_BUFFER_DESC bd;
	memset(&bd,0,sizeof(D3D11_BUFFER_DESC));
	bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
	bd.Usage=D3D11_USAGE_DEFAULT;
	bd.ByteWidth=uiStride*uiVertexCount;
	D3D11_SUBRESOURCE_DATA sd;
	sd.pSysMem=pInitialData;
	sd.SysMemPitch=NULL;
	sd.SysMemSlicePitch=NULL;
	hr=m_pID3D11Dev->CreateBuffer(&bd,&sd,&pIVB);
	if(FAILED(hr)) return false;
	auto it=m_mapVertexBuffers.find(lVBID);
	if(it!=m_mapVertexBuffers.end())
	{
		SAFE_RELEASE(it->second);
		m_mapVertexBuffers.erase(it);
	}
	m_mapVertexBuffers.insert(make_pair(lVBID,pIVB));
	return true;
}
bool CDX11Manager::CreateIndexBuffer(unsigned long* pulInitialData,unsigned int uiIndexCount, long lIBID)
{
	HRESULT hr=E_FAIL;
	ID3D11Buffer* pIIB=NULL;
	D3D11_BUFFER_DESC bd;
	memset(&bd,0,sizeof(D3D11_BUFFER_DESC));
	bd.BindFlags=D3D11_BIND_INDEX_BUFFER;
	bd.Usage=D3D11_USAGE_DEFAULT;
	bd.ByteWidth=sizeof(unsigned long)*uiIndexCount;
	D3D11_SUBRESOURCE_DATA sd;
	sd.pSysMem=(void*)pulInitialData;
	sd.SysMemPitch=NULL;
	sd.SysMemSlicePitch=NULL;
	hr=m_pID3D11Dev->CreateBuffer(&bd,&sd,&pIIB);
	if(FAILED(hr)) return false;
	auto it=m_mapIndexBuffers.find(lIBID);
	if(it!=m_mapIndexBuffers.end())
	{
		SAFE_RELEASE(it->second);
		m_mapIndexBuffers.erase(it);
	}
	m_mapIndexBuffers.insert(make_pair(lIBID,pIIB));
	return true;
}
bool CDX11Manager::DrawIndexed(void* pVertexData,unsigned int uiVertexStride,unsigned int uiVertexCount,unsigned long* pIndexData,unsigned int uiIndexCount)
{
	if(CreateVertexBuffer(pVertexData,uiVertexStride,uiVertexCount,-2))
	{
		if(CreateIndexBuffer(pIndexData,uiIndexCount,-2))
		{
			unsigned int uiOffset=0;
			m_pID3D11DevContext->IASetVertexBuffers(0,1,&m_mapVertexBuffers[-2],&uiVertexStride,&uiOffset);
			m_pID3D11DevContext->IASetIndexBuffer(m_mapIndexBuffers[-2],DXGI_FORMAT_R32_UINT,0);
			m_pID3D11DevContext->DrawIndexed(uiIndexCount,0,0);
			return true;
		}
	}
	return false;
}
bool CDX11Manager::Draw(void* pVertexData,unsigned int uiVertexStride,unsigned int uiVertexCount)
{
	if(CreateVertexBuffer(pVertexData,uiVertexStride,uiVertexCount,-2))
	{
		unsigned int uiOffset=0;
		m_pID3D11DevContext->IASetVertexBuffers(0,1,&m_mapVertexBuffers[-2],&uiVertexStride,&uiOffset);
		m_pID3D11DevContext->Draw(uiVertexCount,0);
		return true;
	}
	return false;
}
void CDX11Manager::DestroyShader(long lShaderID)
{
	{
		auto it=m_mapComputeShaders.find(lShaderID);
		if(it!=m_mapComputeShaders.end())
		{
			SAFE_RELEASE(it->second.m_pICSCode);
			SAFE_RELEASE(it->second.m_pICS);
			m_mapComputeShaders.erase(it);
		}
	}
	{
		auto it=m_mapVertexShaders.find(lShaderID);
		if(it!=m_mapVertexShaders.end())
		{
			SAFE_RELEASE(it->second.m_pIA);
			SAFE_RELEASE(it->second.m_pIVS);
			SAFE_RELEASE(it->second.m_pIVSCode);
			m_mapVertexShaders.erase(it);
		}
	}
	{
		auto it=m_mapPixelShaders.find(lShaderID);
		if(it!=m_mapPixelShaders.end())
		{
			SAFE_RELEASE(it->second.m_pIPS);
			SAFE_RELEASE(it->second.m_pIPSCode);
			m_mapPixelShaders.erase(it);
		}
	}
}
///////////////////////////////////////////////////////////////////
bool CDX11Manager::InitializeDirectComputeOnly(bool bUsedGPU)
{
	HRESULT hr=E_FAIL;
	if(bUsedGPU)
	{
		//Usa GPU
		hr=D3D11CreateDevice(
			NULL,						//Default gfx adapter
			D3D_DRIVER_TYPE_HARDWARE,	//Use Hardware
			NULL,						//Not sw rasterizer
			0,							//Debug, Threaded, etc.
			0,							//Feature Levels
			0,							//Size of above
			D3D11_SDK_VERSION,			//SDK Version
			&m_pID3D11Dev,				//D3D Version
			&m_FeatureLevel,			//Of actual device
			&m_pID3D11DevContext);		//Buvunit of device
	}
	else
	{
		//Usa CPU
		hr=D3D11CreateDevice(
			NULL,						//Default gfx adapter
			D3D_DRIVER_TYPE_WARP,		//Use Software
			NULL,						//Not sw rasterizer
			0,							//Debug, Threaded, etc.
			0,							//Feature Levels
			0,							//Size of above
			D3D11_SDK_VERSION,			//SDK Version
			&m_pID3D11Dev,				//D3D Version
			&m_FeatureLevel,			//Of actual device
			&m_pID3D11DevContext);		//Buvunit of device
	}
	if(SUCCEEDED(hr))
	{
		printf("Dispositivo creado exitosamente...\n");
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
		PerFeatureLevel();		//Hace los preparativos para que se compilen bien los shaders

		return true;
	}
	return false;	
}
