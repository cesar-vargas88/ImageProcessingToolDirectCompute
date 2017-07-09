#include "StdAfx.h"
#include "GPUView.h"

CGPUView::CGPUView(CDX11Manager* pManager)
{
	m_pManager=pManager;
}
CGPUView::~CGPUView(void)
{
}
CGPUView* CGPUView::CreateView(CDX11Manager* pManager, CVAImagen* pImage)
{
	CGPUView* pGPUView = new CGPUView(pManager);
	pGPUView->m_pImageTarget=pImage;

	//Crear un buffer en el GPU

	ID3D11Device* pDev=pGPUView->m_pManager->GetDevice();
	D3D11_TEXTURE2D_DESC dtd;
	memset(&dtd,0,sizeof(D3D11_TEXTURE2D_DESC));
	dtd.ArraySize=1;
	dtd.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
	dtd.CPUAccessFlags=0;
	dtd.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
	dtd.Height=pImage->m_ulSizeY;
	dtd.Width=pImage->m_ulSizeX;
	dtd.Usage=D3D11_USAGE_DEFAULT;
	dtd.MipLevels=1;
	dtd.SampleDesc.Count=1;
	dtd.SampleDesc.Quality=0;

	D3D11_SUBRESOURCE_DATA sd;
	sd.pSysMem=pImage->m_pBaseDirection;
	sd.SysMemPitch=pImage->m_ulSizeX*sizeof(CVAImagen::PIXEL);
	sd.SysMemSlicePitch=0;
	
	//Crear Textura en GPU/CPU
	ID3D11Texture2D* pTexture2D=NULL;
	pDev->CreateTexture2D(&dtd,&sd,&pTexture2D);
	long lIDResource=pGPUView->m_pManager->CreateResourceID();
	pGPUView->m_lIDResource=lIDResource;
	pGPUView->m_pManager->m_mapTextures.insert(make_pair(lIDResource,pTexture2D));

	//Vista de lectura
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
	ID3D11ShaderResourceView* pISRV=NULL;
	memset(&srvd,0,sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	srvd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
	srvd.ViewDimension=::D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels=1;
	srvd.Texture2D.MostDetailedMip=0;
	pGPUView->m_pManager->GetDevice()->CreateShaderResourceView(pTexture2D,&srvd,&pISRV);
	pGPUView->m_pManager->m_mapShaderResourceViews.insert(make_pair(lIDResource,pISRV));

	//Vista de escritura lectura
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd;
	ID3D11UnorderedAccessView* pIUAV=NULL;
	memset(&uavd,0,sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uavd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
	uavd.ViewDimension=D3D11_UAV_DIMENSION_TEXTURE2D;
	uavd.Texture2D.MipSlice=0;
	pGPUView->m_pManager->GetDevice()->CreateUnorderedAccessView(pTexture2D,&uavd,&pIUAV);
	pGPUView->m_pManager->m_mapUnorderedAccessViews.insert(make_pair(lIDResource,pIUAV));
		
	return pGPUView;
}
void CGPUView::DestroyView(CGPUView* pViewToDestroy)
{
	pViewToDestroy->m_pManager->m_mapShaderResourceViews[pViewToDestroy->m_lIDResource]->Release();					//Liberamos recurso
	pViewToDestroy->m_pManager->m_mapShaderResourceViews.erase(pViewToDestroy->m_lIDResource);

	pViewToDestroy->m_pManager->m_mapUnorderedAccessViews[pViewToDestroy->m_lIDResource]->Release();				//Liberamos recurso
	pViewToDestroy->m_pManager->m_mapUnorderedAccessViews.erase(pViewToDestroy->m_lIDResource);

	pViewToDestroy->m_pManager->m_mapTextures[pViewToDestroy->m_lIDResource]->Release();							//Liberamos recurso
	pViewToDestroy->m_pManager->m_mapTextures.erase(pViewToDestroy->m_lIDResource);	

	delete pViewToDestroy;
}
void CGPUView::SetReadWrite(unsigned long ulRegisterU)
{
	m_pManager->GetContext()->CSSetUnorderedAccessViews(ulRegisterU,1,&m_pManager->m_mapUnorderedAccessViews[m_lIDResource],NULL);
}
void CGPUView::SetRead(unsigned long ulRegisterT)
{
	m_pManager->GetContext()->CSSetShaderResources(ulRegisterT,1,&m_pManager->m_mapShaderResourceViews[m_lIDResource]);	
}
void CGPUView::Update(void)
{
	//Crear un bufrer de memoria de sistema en donde el GPU pueda escribir información y el CPU pueda leer
	ID3D11Texture2D *pSystemTexture=NULL;
	D3D11_TEXTURE2D_DESC dtd;
	memset(&dtd,0,sizeof(D3D11_TEXTURE2D_DESC));
	dtd.ArraySize=1;
	dtd.BindFlags=0;
	dtd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
	dtd.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
	dtd.Width=m_pImageTarget->m_ulSizeX;
	dtd.Height=m_pImageTarget->m_ulSizeY;
	dtd.MipLevels=1;
	dtd.SampleDesc.Count=1;
	dtd.SampleDesc.Quality=0;
	dtd.Usage=D3D11_USAGE_STAGING;					//Avisa al controlador para dejar al alcance de varios escenarios (De sistema y video)
	m_pManager->GetDevice()->CreateTexture2D(&dtd,NULL,&pSystemTexture);
	m_pManager->GetContext()->CopyResource(pSystemTexture,m_pManager->m_mapTextures[m_lIDResource]);

	D3D11_MAPPED_SUBRESOURCE map;
	m_pManager->GetContext()->Map(pSystemTexture,0,D3D11_MAP_READ,0,&map);
	//Hemos logrado acceso a los bytes desde el CPU, ahora copiar los datos hacia la imagen
	char *pSource=(char*)map.pData;
	for(int j=0;j<m_pImageTarget->m_ulSizeY;j++)
	{
		memcpy(&(*m_pImageTarget)(0,j),pSource,m_pImageTarget->m_ulSizeX*sizeof(CVAImagen::PIXEL));
		pSource+=map.RowPitch;
	}
	m_pManager->GetContext()->Unmap(pSystemTexture,0);
	pSystemTexture->Release();
}