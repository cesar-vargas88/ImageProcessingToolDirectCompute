#pragma once
#include "dxgimanager.h"
#include <map>
#include <stack>
#include <string>
using namespace std;

struct VERTEX_SHADER
{
	ID3D11VertexShader* m_pIVS;       //The Vertex Shader
	ID3D11InputLayout*  m_pIA;        //Its Input Layout
	ID3D10Blob*			m_pIVSCode;   //Binary code from a compiled vertex shader in HLSL
};
struct PIXEL_SHADER
{
	ID3D11PixelShader* m_pIPS;			//The Pixel Shader 
	ID3D10Blob*		   m_pIPSCode;      //Binary code from a compiled pixel shader in HLSL
};
struct COMPUTE_SHADER
{
	ID3D11ComputeShader* m_pICS;
	ID3D10Blob*			 m_pICSCode;
};
class CTexturePool;
class CMeshBase;
#define REMOVE_RESOURCE(Collection,ID) { auto it=Collection.find((ID)); if(it!=Collection.end()) { SAFE_RELEASE(it->second); Collection.erase(it); } }

class CDX11Manager :
	public CDXGIManager
{
protected:
	long m_lResourceIDGenerator;
	virtual void PerFeatureLevel(void);
public:
	//Superficies de textura (Memoria) 
	map<long,ID3D11Texture2D*> m_mapTextures;
	//Render Targets Views (Se asocian a una textura)
	map<long,ID3D11RenderTargetView*> m_mapRenderTargetViews; //Any render target can be used as texture, used when postprocessing
	//////////////////////////////////////////////////////////////////////////////
	//Shader Resource Views (Se asocian a una textura)							//
	map<long,ID3D11ShaderResourceView*> m_mapShaderResourceViews;				//
	//Unodered Acces View (Se asocian a una textura) //Lectura y escritura		//
	map<long,ID3D11UnorderedAccessView*> m_mapUnorderedAccessViews;				//
	//////////////////////////////////////////////////////////////////////////////
	//Depth Stencil Views
	map<long,ID3D11DepthStencilView*> m_mapDepthStencilViews;
	//La biblioteca de samplers states
	map<long,ID3D11SamplerState*> m_mapSamplers;
	//Blend States
	map<long,ID3D11BlendState*> m_mapBlendStates;
	//La biblioteca de rasterizadores
	map<long,ID3D11RasterizerState*> m_mapRasterizers;
	//La biblioteca de vertex shaders compilados
	map<long,ID3D11DepthStencilState*> m_mapDepthStencilStates;
	map<long,VERTEX_SHADER>    m_mapVertexShaders;
	//La biblioteca de pixel shaders compilados
	map<long,PIXEL_SHADER>     m_mapPixelShaders;
	//Biblioteca de shaders de computación
	map<long,COMPUTE_SHADER>   m_mapComputeShaders;
	//Buffers de constantes para Vertex Shaders y Pixel Shaders
	ID3D11Buffer* m_pIVSConstantBuffer;
	ID3D11Buffer* m_pIPSConstantBuffer;
	ID3D11Buffer* m_pICSConstantBuffer;

	map<long,ID3D11Buffer*>     m_mapVertexBuffers;
	map<long,ID3D11Buffer*>		m_mapIndexBuffers;

	stack<long>        m_stkResourcesFrames;
	char* m_pszVertexShaderProfile;
	char* m_pszPixelShaderProfile;
	char* m_pszComputeShaderProfile;
public:
	long CreateResourceID(void);
	void BeginResourcesFrame(void);
	CTexturePool* CreateTexturePool();
	void EndResourcesFrame(void);
	CDX11Manager(void);
	bool InitializeDX11(void);
	bool CompileShader(const char* pszFileName,const char* pszShaderProfile,const char* pszEntryPoint,ID3D10Blob** ppOutShaderCode);
	bool CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* pLayoutInfo,unsigned long ulInputElements,long lVSID);
	bool CompileAndCreateVertexShader(const char* pszFileName,const char* pszEntryPoint,long lVSID);
	bool CompileAndCreatePixelShader(const char* pszFileName,const char* pszEntryPoint,long lPSID);
	bool CompileAndCreateComputeShader(const char* pszFileName,const char* pszEntryPoint,long lCSID);
	bool CreateVertexBuffer(void* pInitialData,unsigned int uiStride,unsigned int uiVertexCount,long lVBID);
	bool CreateIndexBuffer (unsigned long *pulInitialData,unsigned int uiIndexCount,long lVBID);
	bool LoadTexture2DAndCreateShaderResourceView(const char* pszFileName,long lSRID);
	bool UpdateConstantBuffer(ID3D11Buffer* pID3D11Buffer,void* pInData,unsigned long ulSize);
	bool UpdateCSConstantBuffer(void* pInData,unsigned long ulSize){return UpdateConstantBuffer(m_pICSConstantBuffer,pInData,ulSize);}
	bool UpdateVSConstantBuffer(void* pInData,unsigned long ulSize){return UpdateConstantBuffer(m_pIVSConstantBuffer,pInData,ulSize);}
	bool UpdatePSConstantBuffer(void* pInData,unsigned long ulSize){return UpdateConstantBuffer(m_pIPSConstantBuffer,pInData,ulSize);}	
	bool DrawIndexed(void* pVertexData,unsigned int uiVertexStride,unsigned int uiVertexCount,unsigned long* pIndexData,unsigned int uiIndexCount);
	bool Draw(void* pVertexData,unsigned int uiVertexStride,unsigned int uiVertexCount);
	virtual void Resize(int cx,int cy);
	virtual void Uninitialize(void);
	void DestroyShader(long lShaderID);
	virtual ~CDX11Manager(void);
	//////////////////////////////////////////////////////////////
	bool InitializeDirectComputeOnly(bool bUsedGPU=true);		//
	//////////////////////////////////////////////////////////////
};

