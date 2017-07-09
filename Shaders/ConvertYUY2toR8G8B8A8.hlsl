Texture2D<float4> Input:register(t0);				//Tipo de dato en HLSL
RWTexture2D<float4> Output:register(u0);

[numthreads(16,16,1)]								//Matriz de 16x16, cada thread es para un pixel

void CSMain(uint3 id:SV_DispatchThreadID)			//Se usa uint3 porque necesito indicar la coordenada de mi thread "numthreads(x,y,z)"
													//uint3 vector de 3 enteros de 32 bits cada uno													//SV_DispatchThreadID, indica como se van a leer las coordenadas
{
	float4 PixelRGBpar;
	float4 PixelRGBnon;
	float4 PixelYUY2;

	PixelYUY2 = Input[uint2(id.x,id.y)];

	PixelRGBpar.z = (1.168627450980392 * (PixelYUY2.x - 0.0627450980392157))																	+	(1.603921568627451  * (PixelYUY2.w - 0.5019607843137255));
	PixelRGBpar.y = (1.168627450980392 * (PixelYUY2.x - 0.0627450980392157))	-	(0.392156862745098 * (PixelYUY2.y - 0.5019607843137255))	-	(0.8156862745098039 * (PixelYUY2.w - 0.5019607843137255));
	PixelRGBpar.x = (1.168627450980392 * (PixelYUY2.x - 0.0627450980392157))	+	(2.023529411764706 * (PixelYUY2.y - 0.5019607843137255))																 ;
	PixelRGBpar.w = 0.0f;

	PixelRGBnon.z = (1.168627450980392 * (PixelYUY2.z - 0.0627450980392157))																	+	(1.603921568627451  * (PixelYUY2.w - 0.5019607843137255));
	PixelRGBnon.y = (1.168627450980392 * (PixelYUY2.z - 0.0627450980392157))	-	(0.392156862745098 * (PixelYUY2.y - 0.5019607843137255))	-	(0.8156862745098039 * (PixelYUY2.w - 0.5019607843137255));
	PixelRGBnon.x = (1.168627450980392 * (PixelYUY2.z - 0.0627450980392157))	+	(2.023529411764706 * (PixelYUY2.y - 0.5019607843137255))																 ;
	PixelRGBnon.w = 0.0f;

	Output[uint2(id.x*2, id.y)]		= PixelRGBpar;
	Output[uint2((id.x*2)+1, id.y)] = PixelRGBnon;
}
