cbuffer Params
{
	matrix Mt;
};

Texture2D<float4> Input:register(t0);
RWTexture2D<float4> Output:register(u0);

[numthreads(16,16,1)]

void CSMain(uint3 id:SV_DispatchThreadID)
{
	int2 Position;
	float4 fPosition;
	fPosition=mul(float4(id.x,id.y,0,1),Mt);
	Position.x=fPosition.x;
	Position.y=fPosition.y;
	//Output[int2(id.x,id.y)]=float4(0,0,0,1);;
	Output[int2(id.x,id.y)]=Input[Position];
}