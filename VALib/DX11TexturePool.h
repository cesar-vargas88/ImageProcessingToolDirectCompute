#pragma once
#include "texturepool.h"
#include "DX11Manager.h"
class CDX11TexturePool :
	public CTexturePool
{

	friend class CDX11Manager;
protected:
	CDX11Manager* m_pDX11Manager;
	CDX11TexturePool(CDX11Manager* pOwner);
public:
	virtual ~CDX11TexturePool(void);
	virtual long LoadTexture(const char* pszFileName);
	virtual void ReleaseTexture(long lSRVID);
	virtual void ReleaseAll(void);
};

