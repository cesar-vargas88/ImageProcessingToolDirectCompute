//Para hacer blur necesitamos conocer las energias de los pixeles aledaños, y los sumamos

Texture2D<float4> Input:register(t0);				
RWTexture2D<float4> Output:register(u0);

float4 SumaPixeles(uint2 coord)
{
	float4 Promedio;
	Promedio=float4(0,0,0,0);
	Promedio=Input[coord+uint2(0,-10)];
	Promedio+=Input[coord+uint2(10,0)];
	Promedio+=Input[coord+uint2(0,10)];
	Promedio+=Input[coord+uint2(-10,0)];
	Promedio+=Input[coord];
	Promedio*=(1/5.0f);
	Promedio.w=1;
	return Promedio;
}

[numthreads(16,16,1)]								
void CSMain(uint3 id:SV_DispatchThreadID)			
{
	Output[uint2(id.x,id.y)]=SumaPixeles(uint2(id.x,id.y));
}
