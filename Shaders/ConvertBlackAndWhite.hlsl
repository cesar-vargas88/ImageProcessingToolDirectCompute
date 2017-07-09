Texture2D<float4> Input:register(t0);				//Tipo de dato en HLSL
RWTexture2D<float4> Output:register(u0);
[numthreads(16,16,1)]								//Matriz de 16x16, cada thread es para un pixel

void CSMain(uint3 id:SV_DispatchThreadID)			//Se usa uint3 porque necesito indicar la coordenada de mi thread "numthreads(x,y,z)"
													//uint3 vector de 3 enteros de 32 bits cada uno
													//SV_DispatchThreadID, indica como se van a leer las coordenadas
{
	float4 PixelBW;
	PixelBW.xyzw=Input[uint2(id.x,id.y)].xyzw;		//Copia el valor de "y" en las cuatro coordenadas. Usamos el color verde para aproximar a blanco y negro
	PixelBW.w=0;
	float Gray=sqrt(dot(PixelBW,PixelBW))/sqrt(3);				//dot es el producto punto
	PixelBW=Gray;
	PixelBW.w=1;
	Output[uint2(id.x,id.y)]=PixelBW;
}
