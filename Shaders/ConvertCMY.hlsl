Texture2D<float4> Input:register(t0);				//Tipo de dato en HLSL
RWTexture2D<float4> Output:register(u0);
[numthreads(16,16,1)]								//Matriz de 16x16, cada thread es para un pixel

void CSMain(uint3 id:SV_DispatchThreadID)			//Se usa uint3 porque necesito indicar la coordenada de mi thread "numthreads(x,y,z)"
													//uint3 vector de 3 enteros de 32 bits cada uno
													//SV_DispatchThreadID, indica como se van a leer las coordenadas
{
	float4 PixelCMY;
	PixelCMY.xyzw=Input[uint2(id.x,id.y)].xyzw;


	Output[uint2(id.x,id.y)]=PixelCMY;
}
