#include "StdAfx.h"
#include "DX11TexturePool.h"


CDX11TexturePool::CDX11TexturePool(CDX11Manager* pOwner)
{
	m_pDX11Manager=pOwner;
}


CDX11TexturePool::~CDX11TexturePool(void)
{
}

long CDX11TexturePool::LoadTexture(const char* pszFileName)
{
	auto it=m_mapTextureFileNameToSRV.find(pszFileName);
	if(it!=m_mapTextureFileNameToSRV.end())
	{
		m_mapSRVToTextureInfo[it->second].m_lRefCount++;
		return it->second; //Una textura con ese nombre de archivo ya existe, retornar el ID ya existente
	}
	//Intentar cargar la textura
	long lSRVID=m_pDX11Manager->CreateResourceID();
	if(m_pDX11Manager->LoadTexture2DAndCreateShaderResourceView(pszFileName,lSRVID))
	{
		TEXTURE_INFO TI;
		TI.m_lRefCount=1;
		TI.m_strFileName=pszFileName;
		m_mapSRVToTextureInfo.insert(make_pair(lSRVID,TI));
		m_mapTextureFileNameToSRV.insert(make_pair(pszFileName,lSRVID));
		return lSRVID;
	}
	return 0;
}
void CDX11TexturePool::ReleaseTexture(long lSRVID)
{
	auto it=m_mapSRVToTextureInfo.find(lSRVID);
	if(it!=m_mapSRVToTextureInfo.end())
	{
		--it->second.m_lRefCount;
		if(0==it->second.m_lRefCount)
		{
			REMOVE_RESOURCE(m_pDX11Manager->m_mapShaderResourceViews,lSRVID);
			REMOVE_RESOURCE(m_pDX11Manager->m_mapTextures,lSRVID);
			m_mapTextureFileNameToSRV.erase(it->second.m_strFileName);
			m_mapSRVToTextureInfo.erase(it);
		}
	}
}

void CDX11TexturePool::ReleaseAll(void)
{
	for(auto itT=m_mapSRVToTextureInfo.begin();itT!=m_mapSRVToTextureInfo.end();itT++)
	{
		REMOVE_RESOURCE(m_pDX11Manager->m_mapShaderResourceViews,itT->first);
		REMOVE_RESOURCE(m_pDX11Manager->m_mapTextures,itT->first);
	}
	m_mapTextureFileNameToSRV.clear();
	m_mapSRVToTextureInfo.clear();
}