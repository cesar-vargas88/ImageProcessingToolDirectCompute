Texture2D<float4> Input:register(t0);				//Tipo de dato en HLSL
RWTexture2D<float4> Output:register(u0);

[numthreads(16,16,1)]								//Matriz de 16x16, cada thread es para un pixel

void CSMain(uint3 id:SV_DispatchThreadID)			//Se usa uint3 porque necesito indicar la coordenada de mi thread "numthreads(x,y,z)"
													//uint3 vector de 3 enteros de 32 bits cada uno													//SV_DispatchThreadID, indica como se van a leer las coordenadas
{
	float4 PixelRGB24;
	float4 PixelRGB32;

	PixelRGB24 = Input[uint2(id.x,id.y)];

	PixelRGB32.x	= PixelRGB24.x;
	PixelRGB32.y	= PixelRGB24.y;
	PixelRGB32.z	= PixelRGB24.z;
	PixelRGB32.w	= 0.1f;
	
	Output[uint2(id.x, 480-id.y)] = PixelRGB32;
}
