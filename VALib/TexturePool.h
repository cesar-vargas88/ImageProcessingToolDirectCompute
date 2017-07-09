#pragma once
#include <map>
using namespace std;
class CTexturePool
{
protected:
	//Mapas de refencia cruzada de ID's de SRV <-> Nombre de archivo de imagen
	struct TEXTURE_INFO
	{
		string m_strFileName;
		long   m_lRefCount;
	};
	map<long,TEXTURE_INFO>	m_mapSRVToTextureInfo;
	map<string,long>		m_mapTextureFileNameToSRV;
protected:
	CTexturePool();
public:
	/*
	Carga una textura y retorna el identificador de SRV de la textura cargada, 
	Si se intenta cargar una textura con ese mismo nombre de archivo por segunda ocación, se retornará
	el identificador ya existente, evitando texturas duplicadas. Se incrementa el cotador de referencias a
	dicho recurso.
	*/
	virtual long LoadTexture(const char* pszFileName)=0;
	/*
		Decrementa el contador de referencias a una textura. Cuando el contador llega a cero, se libera el recurso
		de textura
	*/
	virtual void ReleaseTexture(long lSRVID)=0;
	virtual void ReleaseAll(void)=0;
	virtual ~CTexturePool(void);
};

