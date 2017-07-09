#pragma once
#include "DX11Manager.h"
#include "VAImagen.h"
class CGPUView
{
protected:
	CDX11Manager*	m_pManager;
	CVAImagen*		m_pImageTarget;
	long			m_lIDResource;
	CGPUView(CDX11Manager* m_pManager);
	~CGPUView(void);
public:
	
	static CGPUView* CreateView(CDX11Manager* pManager, CVAImagen* pImage);
	static void DestroyView(CGPUView* pViewToDestroy);
	void SetReadWrite(unsigned long ulRegisterU);
	void SetRead(unsigned long ulRegisterT);
	void Update(void);							//Transfiere la memoria de video (Texture2D) hacia la memoria  de sistema
};


