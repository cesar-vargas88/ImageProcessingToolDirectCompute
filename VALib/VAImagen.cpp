#include "StdAfx.h"
#include "VAImagen.h"
#include <memory.h>
#include <fstream>
#include "VAHistograma.h"
#include <math.h>
#include <stack>

using namespace std;

CVAImagen::CVAImagen(void)
{
	m_pBaseDirection=0;
	m_ulSizeX=0;
	m_ulSizeY=0;
	m_ulMostShinyPixel;
}
CVAImagen::~CVAImagen(void)
{
}
CVAImagen::PIXEL& CVAImagen::operator()(unsigned long i, unsigned long j)
{
	return *(m_pBaseDirection+m_ulSizeX*j+i);
}
CVAImagen* CVAImagen::CreateNewImage(unsigned long X, unsigned long Y)
{
	CVAImagen *pNew = new CVAImagen();
	pNew->m_ulSizeX = X;
	pNew->m_ulSizeY = Y;
	pNew->m_pBaseDirection = new CVAImagen::PIXEL[X*Y];
	return pNew;
}
CVAImagen* CVAImagen::CloneImage(CVAImagen* pOriginalImage)
{
	CVAImagen* pNew = CreateNewImage(pOriginalImage->m_ulSizeX,pOriginalImage->m_ulSizeY);
	memcpy(pNew->m_pBaseDirection,pOriginalImage->m_pBaseDirection, pOriginalImage->m_ulSizeX * pOriginalImage->m_ulSizeY * sizeof(CVAImagen::PIXEL));
	return pNew;
}
void CVAImagen::DestroyImage(CVAImagen* pImage)
{
	delete [] pImage->m_pBaseDirection;
	delete pImage;
}
// Dibuja la imagen al macenada en el contexto de dispositivo especificado
void CVAImagen::DrawImage(HDC hdc, int x0, int y0, int cx, int cy)
{
		HBITMAP hbmpMem=CreateCompatibleBitmap(hdc,m_ulSizeX, m_ulSizeY);					//Crea memoria administrada por el sistema operativo
		HDC hdcMem=CreateCompatibleDC(hdc);													//Pido representacion virtual de ese dispositivo, con las mismas caracteristicas que la imagen original
		SetBitmapBits(hbmpMem,m_ulSizeX*m_ulSizeY*sizeof(PIXEL),this->m_pBaseDirection);	//Transfiero desde la a la memoria del sistema.
		HGDIOBJ hOld=SelectObject(hdcMem,hbmpMem);											//Pido a la impresora virtual que usa la imagen
		StretchBlt(hdc,x0,y0,cx,cy,hdcMem,0,0,m_ulSizeX,m_ulSizeY,SRCCOPY);					//Pido a la tarjeta de video que copia la imagen del bloque de memoria al monitor
		SelectObject(hdcMem, hOld);
		DeleteObject(hbmpMem);
		DeleteDC(hdcMem);
}
bool CVAImagen::SaveToFile(CVAImagen *pImage,const char* pszFileName)
{
	fstream File;
	File.open(pszFileName,ios::binary|ios::out);
	if(!File.is_open())
		return false;
	pImage->Serialize(File,true);
	File.close();
	return true;
}
CVAImagen* CVAImagen::LoadFromFile(const char* pszFileName)
{
	fstream Archivo;
	Archivo.open(pszFileName,ios::in|ios::binary);
	CVAImagen *pNuevaImagen=new CVAImagen;
	pNuevaImagen->Serialize(Archivo,false);
	Archivo.close();
	return pNuevaImagen;
}
void CVAImagen::Serialize(iostream& io,bool bSave)
{
	//Algoritmo para guardar
	if(bSave)
	{
		io.write((char*)&m_ulSizeX,sizeof(unsigned long));
		io.write((char*)&m_ulSizeY,sizeof(unsigned long));
		io.write((char*)m_pBaseDirection,m_ulSizeX*m_ulSizeY*sizeof(PIXEL));
	}
	//Algoritmo de carga
	else
	{
		io.read((char*)&m_ulSizeX,sizeof(unsigned long));
		io.read((char*)&m_ulSizeY,sizeof(unsigned long));
		if(m_pBaseDirection)
			delete [] m_pBaseDirection;
		m_pBaseDirection=new PIXEL[m_ulSizeX*m_ulSizeY];
		io.read((char*)m_pBaseDirection,m_ulSizeX*m_ulSizeY*sizeof(PIXEL));
	}
}
CVAImagen* CVAImagen::ConvertCMY()
{
	CVAImagen* pCMY=this->CreateNewImage(m_ulSizeX,m_ulSizeY);
	for(int i=0;i<m_ulSizeX*m_ulSizeY;i++)
	{
		PIXEL ColorCMY,ColorRGB;
		ColorRGB=m_pBaseDirection[i];
		ColorCMY.r=255-ColorRGB.r;
		ColorCMY.g=255-ColorRGB.g;
		ColorCMY.b=255-ColorRGB.b;
		pCMY->m_pBaseDirection[i]=ColorCMY;
	}
	return pCMY;
}
CVAImagen* CVAImagen::ConvertBW()
{
	CVAImagen* pBW=this->CreateNewImage(m_ulSizeX,m_ulSizeY);
	for(int i=0;i<m_ulSizeX*m_ulSizeY;i++)
	{
		PIXEL ColorBW,ColorRGB;
		ColorRGB=m_pBaseDirection[i];
		
		ColorBW.r=ColorBW.g=ColorBW.b=((unsigned long)ColorRGB.r+(unsigned long)ColorRGB.g+(unsigned long)ColorRGB.b)/3;

		pBW->m_pBaseDirection[i]=ColorBW;
	}
	return pBW;
}
CVAImagen* CVAImagen::LoadBMP(const char* pszFileName)
{
	fstream in;
	in.open(pszFileName,ios::in|ios::binary);
	if(!in.is_open())
		return NULL;
	BITMAPFILEHEADER bfh;			//Siempre van
	BITMAPINFOHEADER bih;			//Siempre van
	RGBQUAD	Palette[256];			//Declara una paleta (no puede ser más grande de 256)
	memset(&bfh,0,sizeof(BITMAPFILEHEADER));
	memset(&bih,0,sizeof(BITMAPINFOHEADER));
	memset(Palette,0,sizeof(RGBQUAD)*256);
	
	//Leer la signature
	in.read((char*)&bfh.bfType,sizeof(bfh.bfType));
	if(bfh.bfType!='MB')
		return NULL;

	in.read((char*)&bfh.bfSize,sizeof(BITMAPFILEHEADER)-sizeof(bfh.bfType));
	//Leer el BITMAPINFOHEADER
	in.read((char*)&bih.biSize,sizeof(bih.biSize));
	if(bih.biSize!=sizeof(BITMAPINFOHEADER))
		return NULL;
	in.read((char*)&bih.biWidth,sizeof(BITMAPINFOHEADER)-sizeof(bih.biSize));

	CVAImagen* pImage=NULL;

	switch(bih.biBitCount)
	{
	case 1:	//Monocromatico
		{
			pImage=CreateNewImage(bih.biWidth,bih.biHeight);
			in.read((char*)Palette,(pow(2.0,1.0*bih.biBitCount)*sizeof(RGBQUAD))); 
			unsigned long ulScanLineSize;
			ulScanLineSize = ((bih.biWidth*bih.biBitCount +31) >>5)<<2;
			unsigned char* pScanLine=new unsigned char[ulScanLineSize];
			int nBit	= 0;
			int nByte	= 0;
			for(int j=0;j<bih.biHeight;j++)
			{
				in.read((char*)pScanLine,ulScanLineSize);
				nBit	= 0;
				nByte	= 0;
				for(int i=0;i<bih.biWidth;i++)
				{
					PIXEL P;		//traducir un indice en un CVAImagen::PIXEL
			
					if(nBit == 8)
					{
						nByte++;
						nBit = 0;
					}
					
					P.r=Palette[(pScanLine[nByte]>>nBit)&1].rgbRed;
					P.g=Palette[(pScanLine[nByte]>>nBit)&1].rgbGreen;
					P.b=Palette[(pScanLine[nByte]>>nBit)&1].rgbBlue;
					P.a=0xff;
					(*pImage)((nByte*8)+7-nBit,bih.biHeight-j-1)=P;	
					nBit ++;
				}	
			}
			delete pScanLine;
			return pImage;
		}
	case 4: //Hexadecacromatico
		{
			pImage=CreateNewImage(bih.biWidth,bih.biHeight);
			in.read((char*)Palette,(pow(2.0,1.0*bih.biBitCount)*sizeof(RGBQUAD))); 
			unsigned long ulScanLineSize;
			ulScanLineSize = ((bih.biWidth*bih.biBitCount +31) >>5)<<2;
			unsigned char* pScanLine=new unsigned char[ulScanLineSize];
			int nNibble	= 0;
			int nByte	= 0;
			for(int j=0;j<bih.biHeight;j++)
			{
				in.read((char*)pScanLine,ulScanLineSize);
				nNibble	= 0;
				nByte	= 0;
				for(int i=0;i<bih.biWidth;i++)
				{
					//traducir un indice en un CVAImagen::PIXEL
					PIXEL P;
					
					if(nNibble == 2)
					{
						nNibble = 0;
						nByte++;
					}
					else if(nNibble = 0)
					{
						P.r=Palette[(pScanLine[nByte]>>4)&0x0F].rgbRed;
						P.g=Palette[(pScanLine[nByte]>>4)&0x0F].rgbGreen;
						P.b=Palette[(pScanLine[nByte]>>4)&0x0F].rgbBlue;
						P.a=0xff;
					}
					else if(nNibble = 1)
					{
						P.r=Palette[(pScanLine[nByte])&0x0F].rgbRed;
						P.g=Palette[(pScanLine[nByte])&0x0F].rgbGreen;
						P.b=Palette[(pScanLine[nByte])&0x0F].rgbBlue;
						P.a=0xff;
					}
					
					(*pImage)(i,bih.biHeight-j-1)=P;
					nNibble++;
				}
			}
			delete pScanLine;
			return pImage;
		}
	case 8:	//256 Colores
		{
			pImage=CreateNewImage(bih.biWidth,bih.biHeight);
			in.read((char*)Palette,(pow(2.0,1.0*bih.biBitCount)*sizeof(RGBQUAD)));
			unsigned long ulScanLineSize;
			ulScanLineSize = ((bih.biWidth*bih.biBitCount +31) >>5)<<2;
			unsigned char* pScanLine=new unsigned char[ulScanLineSize];
			for(int j=0;j<bih.biHeight;j++)
			{
				in.read((char*)pScanLine,ulScanLineSize);
				for(int i=0;i<bih.biWidth;i++)
				{
					//traducir un indice en un CVAImagen::PIXEL
					PIXEL P;
					P.r=Palette[pScanLine[i]].rgbRed;
					P.g=Palette[pScanLine[i]].rgbGreen;
					P.b=Palette[pScanLine[i]].rgbBlue;
					P.a=0xff;
					(*pImage)(i,bih.biHeight-j-1)=P;
				}
			}
			delete pScanLine;
			return pImage;
		}
	case 24://True color 24bpp
		{
			pImage=CreateNewImage(bih.biWidth,bih.biHeight);
			unsigned long ulScanLineSize;
			ulScanLineSize = ((bih.biWidth*bih.biBitCount +31) >>5)<<2;
			unsigned char* pScanLine=new unsigned char[ulScanLineSize];
			int nByte = 0;
			for(int j=0;j<bih.biHeight;j++)
			{
				nByte = 0;
				in.read((char*)pScanLine,ulScanLineSize);
				for(int i=0;i<bih.biWidth;i++)
				{
					//traducir un indice en un CVAImagen::PIXEL
					PIXEL P;
					P.r=pScanLine[nByte+2];
					P.g=pScanLine[nByte+1];
					P.b=pScanLine[nByte];
					P.a=0xff;
					(*pImage)(i,bih.biHeight-j-1)=P;
					nByte=nByte+3;
				}
			}
			delete pScanLine;
			return pImage;
		}
	case 32://True color 32bpp
		{
			pImage=CreateNewImage(bih.biWidth,bih.biHeight);
			unsigned long ulScanLineSize;
			ulScanLineSize = ((bih.biWidth*bih.biBitCount +31) >>5)<<2;
			unsigned char* pScanLine=new unsigned char[ulScanLineSize];
			int nByte = 0;
			for(int j=0;j<bih.biHeight;j++)
			{
				nByte = 0;
				in.read((char*)pScanLine,ulScanLineSize);
				for(int i=0;i<bih.biWidth;i++)
				{
					//traducir un indice en un CVAImagen::PIXEL
					PIXEL P;
					P.r=pScanLine[nByte+2];
					P.g=pScanLine[nByte+1];
					P.b=pScanLine[nByte];
					P.a=pScanLine[nByte+3];
					(*pImage)(i,bih.biHeight-j-1)=P;
					nByte=nByte+4;
				}
			}
			delete pScanLine;
			return pImage;
		}
	}
}
CVAHistograma* CVAImagen::BuildHistogram()
{
	CVAHistograma* pHist=new CVAHistograma;
	unsigned long ulPixelCount=m_ulSizeX*m_ulSizeY;
	PIXEL* pPixel=m_pBaseDirection;
	
	while(ulPixelCount--)
	{
		pHist->m_Hist[pPixel->r].FreqR++;
		pHist->m_Hist[pPixel->g].FreqG++;
		pHist->m_Hist[pPixel->b].FreqB++;
		pHist->m_Hist[pPixel->a].FreqA++;
		pPixel++;
	}
	return pHist;
}
void CVAImagen::ApplyFunctionTransfer(unsigned char* pTablaTransferencia)
{
	unsigned long PixelCount=m_ulSizeX*m_ulSizeY;
	PIXEL* pPixel=m_pBaseDirection;
	while(PixelCount--)
	{
		pPixel->r=pTablaTransferencia[pPixel->r];
		pPixel->g=pTablaTransferencia[pPixel->g];
		pPixel->b=pTablaTransferencia[pPixel->b];
		pPixel->a=pTablaTransferencia[pPixel->a];
		pPixel++;
	}
}
void CVAImagen::BuildContrastTable(unsigned char* pTabla,float Potencia)
{
	for(int i=0;i<256;i++)
	{
		float x=i*(1.0f/255.0f);
		float y=pow(x,Potencia);
		pTabla[i]=max(0,min((unsigned int)(255*y),255));
	}
}//x^Potencia
CVAImagen* CVAImagen::MostShinyPixelColumn(CVAImagen* pImage, CVAImagen* pEscanear)
{
	stack <unsigned long> snMasIntenso;

	unsigned long WideScan = (pImage->m_ulSizeX - pImage->m_ulSizeY) / 2;

	for(unsigned long i=WideScan;i<pImage->m_ulSizeX-WideScan;i++)
	{
		unsigned char cMasIntenso = 0;
		PIXEL Imagen;
		PIXEL UNO;

		UNO.r = 255;
		UNO.g = 255;
		UNO.b = 255;
		UNO.a = 255;

		for(unsigned long j=0;j<pImage->m_ulSizeY;j++)
		{
			Imagen=pImage->m_pBaseDirection[(j*pImage->m_ulSizeX)+i];

			if(Imagen.r>cMasIntenso)
			{
				while(!snMasIntenso.empty())
				{
					snMasIntenso.pop();
				}
				snMasIntenso.push((j*pImage->m_ulSizeX)+i);
				cMasIntenso=Imagen.r;
			}
			else if(Imagen.r==cMasIntenso)
				snMasIntenso.push((j*pImage->m_ulSizeX)+i);
		}

		if (cMasIntenso > 200)
		{
			unsigned long Cantidad = snMasIntenso.size();
			unsigned long Suma = 0;

			while(!snMasIntenso.empty())
			{
				Suma +=snMasIntenso.top()/pImage->m_ulSizeX;
				snMasIntenso.pop();
			}

			pEscanear->m_pBaseDirection[((Suma/Cantidad)*pImage->m_ulSizeX)+i] = UNO;
			pEscanear->m_ulMostShinyPixel.push(Suma/Cantidad);
		}
		else
		{
			while(!snMasIntenso.empty())
			{
				snMasIntenso.pop();
			}
			pEscanear->m_pBaseDirection[((240)*pImage->m_ulSizeX)+i] = UNO;
			pEscanear->m_ulMostShinyPixel.push(240);
		}
	}
	return pEscanear;
}
CVAImagen* CVAImagen::MultipleThreshold(CVAImagen* pImage, CVAImagen* pSegmentation, stack<unsigned char> sMinima)
{
	unsigned char ucMinima[128];
	unsigned char ucNumberMinima=sMinima.size();
	unsigned char ucUmbralWide;

	PIXEL TableUmbral[128];
	
	char y = 1;
	for(unsigned char x=0 ; x <= sMinima.size() ; x++)
	{
		if(y & 0x01)
			TableUmbral[x].r = 255-((x/7)*28);
		else
			TableUmbral[x].r = 0;

		if(y & 0x02)
			TableUmbral[x].g = 255-((x/7)*28);
		else
			TableUmbral[x].g = 0;

		if(y & 0x04)
			TableUmbral[x].b = 255-((x/7)*28);
		else
			TableUmbral[x].b = 0;

		if(y==7)
			y=1;
		else
			y++;
	}

	ucMinima[0] = 255;

	while(!sMinima.empty())
	{
		ucMinima[sMinima.size()]=sMinima.top();
		sMinima.pop();
	}
	
	ucMinima[ucNumberMinima+1] = 0;

	for(int i=0;i<(pImage->m_ulSizeX*pImage->m_ulSizeY);i++)
	{
		for(int x = ucNumberMinima; x >=0 ; x--)
		{
			if(x > 0)
			{
				if((pImage->m_pBaseDirection[i].r >= ucMinima[x+1]) && (pImage->m_pBaseDirection[i].r < ucMinima[x]))	
				{
					pSegmentation->m_pBaseDirection[i].r=TableUmbral[ucNumberMinima-x].r;
					pSegmentation->m_pBaseDirection[i].g=TableUmbral[ucNumberMinima-x].g;
					pSegmentation->m_pBaseDirection[i].b=TableUmbral[ucNumberMinima-x].b;
					break;
				}
			}
			else
			{
				if((pImage->m_pBaseDirection[i].r >= ucMinima[x+1]) && (pImage->m_pBaseDirection[i].r <= ucMinima[x]))	
				{
					pSegmentation->m_pBaseDirection[i].r=TableUmbral[ucNumberMinima-x].r;
					pSegmentation->m_pBaseDirection[i].g=TableUmbral[ucNumberMinima-x].g;
					pSegmentation->m_pBaseDirection[i].b=TableUmbral[ucNumberMinima-x].b;
					break;
				}
			}
		}
	}
	return pSegmentation;
}