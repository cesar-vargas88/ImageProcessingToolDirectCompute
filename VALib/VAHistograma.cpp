#include "StdAfx.h"
#include "VAHistograma.h"
#include <memory.h>
#include <stdlib.h>

CVAHistograma::CVAHistograma(void)
{
	memset(&m_Hist,0,sizeof(m_Hist));		//Inicializa a cero m_Hist
}
CVAHistograma::~CVAHistograma(void)
{
}
#define max(a,b) ((a)<(b))?(b):(a)
void CVAHistograma::Normalizar(void)
{
	unsigned long MaxR=0;
	unsigned long MaxG=0;
	unsigned long MaxB=0;
	unsigned long MaxA=0;

	for(int i=0;i<256;i++)
	{
		MaxR=max(MaxR,m_Hist[i].FreqR);
		MaxG=max(MaxG,m_Hist[i].FreqG);
		MaxB=max(MaxB,m_Hist[i].FreqB);
		MaxA=max(MaxA,m_Hist[i].FreqA);
	}
	for(int i=0;i<256;i++)
	{
		m_HistNorm[i].FreqR=m_Hist[i].FreqR/(float)MaxR;
		m_HistNorm[i].FreqG=m_Hist[i].FreqG/(float)MaxG;
		m_HistNorm[i].FreqB=m_Hist[i].FreqB/(float)MaxB;
		m_HistNorm[i].FreqA=m_Hist[i].FreqA/(float)MaxA;
	}
}
CVAImagen* CVAHistograma::CrearGrafico(int sx, int sy, int nColor)
{
	CVAImagen* pGrafico= CVAImagen::CreateNewImage(sx, sy);
	memset(pGrafico->m_pBaseDirection,0,pGrafico->m_ulSizeX*pGrafico->m_ulSizeY*sizeof(CVAImagen::PIXEL));

	float x=0;

	float dx, dy;

	dx=(sx-1)/256.0f;

	for(int i=0;i<=255;i++)
	{
		if(nColor==0)
		{
			if(!Minimos.MinimoR.empty() && Minimos.MinimoR.top() == i )
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1))).r=0xff;

				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).r=0xff;
					(*pGrafico)(x,j).g=0xff;
					(*pGrafico)(x,j).b=0xff;
				}
				Minimos.MinimoR.pop();
			}
			else
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1))).r=0xff;
				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).r=0xff;
				}
			}
		}
		else if(nColor==1)
		{
			if(!Minimos.MinimoG.empty() && Minimos.MinimoG.top() == i )
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1))).g=0xff;

				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).r=0xff;
					(*pGrafico)(x,j).g=0xff;
					(*pGrafico)(x,j).b=0xff;
				}
				Minimos.MinimoG.pop();
			}
			else
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1))).g=0xff;
				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).g=0xff;
				}
			}
		}
		else if(nColor==2)
		{
			if(!Minimos.MinimoB.empty() && Minimos.MinimoB.top() == i )
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1))).b=0xff;

				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).r=0xff;
					(*pGrafico)(x,j).g=0xff;
					(*pGrafico)(x,j).b=0xff;
				}
				Minimos.MinimoB.pop();
			}
			else
			{
				(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1))).b=0xff;
				for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1);j--)
				{
					(*pGrafico)(x,j).b=0xff;
				}
			}
		}
		else 
		{			
			(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1))).r=0xff;
			(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1))).g=0xff;
			(*pGrafico)(x,max(0,min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1))).b=0xff;
			
			for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqR*(sy-1),sy-1);j--)
			{
				(*pGrafico)(x,j).r=0xff;
			}
			for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqG*(sy-1),sy-1);j--)
			{
				(*pGrafico)(x,j).g=0xff;
			}
			for(int j=(sy-1);j>min((sy-1)-m_HistNorm[i].FreqB*(sy-1),sy-1);j--)
			{
				(*pGrafico)(x,j).b=0xff;
			}
		}
		x+=dx;
	}

	return pGrafico;
}
void CVAHistograma::SuavizarHistograma(int nSizeWindow, int nRepeatSmooting)
{
	FRECUENCIA_NORMALIZADA Promedio;

	int V=nSizeWindow;

	while(nRepeatSmooting+1)
	{
		for(int i=0;i<256;i++)
		{
			Promedio.FreqR=0.0f;
			Promedio.FreqG=0.0f;
			Promedio.FreqB=0.0f;

			for(int v=-(V/2);v<=(V/2);v++)
			{
				if((i+v)<0 || (i+v)>255)
				{
					Promedio.FreqR+=m_HistNorm[i].FreqR;
					Promedio.FreqG+=m_HistNorm[i].FreqG;
					Promedio.FreqB+=m_HistNorm[i].FreqB;
				}
				else
				{
					Promedio.FreqR+=m_HistNorm[i+v].FreqR;
					Promedio.FreqG+=m_HistNorm[i+v].FreqG;
					Promedio.FreqB+=m_HistNorm[i+v].FreqB;
				}
			}

			m_HistNormSuavizado[i].FreqR=Promedio.FreqR/V;
			m_HistNormSuavizado[i].FreqG=Promedio.FreqG/V;
			m_HistNormSuavizado[i].FreqB=Promedio.FreqB/V;
		}
		for(int i=0;i<256;i++)
		{
			m_HistNorm[i].FreqR=m_HistNormSuavizado[i].FreqR;
			m_HistNorm[i].FreqG=m_HistNormSuavizado[i].FreqG;
			m_HistNorm[i].FreqB=m_HistNormSuavizado[i].FreqB;
		}
		nRepeatSmooting--;
	}
}
void CVAHistograma::ObtenerMinimos(float fUmbral, int nColor)
{
	for(int i=254;i>=1;i--)
	{
		switch(nColor)
		{
			case 0:
			{
				if((m_HistNorm[i].FreqR < m_HistNorm[i-1].FreqR) && (m_HistNorm[i].FreqR < m_HistNorm[i+1].FreqR) && ((m_HistNorm[i-1].FreqR - m_HistNorm[i].FreqR) > fUmbral) && ((m_HistNorm[i+1].FreqR - m_HistNorm[i].FreqR) > fUmbral))
					Minimos.MinimoR.push(i);
				break;
			}
			case 1:
			{
				if((m_HistNorm[i].FreqG < m_HistNorm[i-1].FreqG) && (m_HistNorm[i].FreqG < m_HistNorm[i+1].FreqG) && ((m_HistNorm[i-1].FreqG - m_HistNorm[i].FreqG) > fUmbral) && ((m_HistNorm[i+1].FreqG - m_HistNorm[i].FreqG) > fUmbral))
					Minimos.MinimoG.push(i);
				break;
			}
			case 2:
			{
				if((m_HistNorm[i].FreqB < m_HistNorm[i-1].FreqB) && (m_HistNorm[i].FreqB < m_HistNorm[i+1].FreqB) && ((m_HistNorm[i-1].FreqB - m_HistNorm[i].FreqB) > fUmbral) && ((m_HistNorm[i+1].FreqB - m_HistNorm[i].FreqB) > fUmbral))
					Minimos.MinimoB.push(i);
				break;
			}
		}
	}
}