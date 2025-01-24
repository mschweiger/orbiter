
uniform extern float3  vDir;
uniform extern float3  vUp;
uniform extern float3  vCp;
uniform extern float   fD;
uniform extern bool    bDir;

sampler tCube;
sampler tSrc;

static float coeff[8] = { 0.13298076, 0.125794409, 0.106482669, 0.080656908, 0.054670025, 0.033159046, 0.017996989, 0.00874063 };

float4 PSBlur(float x : TEXCOORD0, float y : TEXCOORD1) : COLOR
{
	x = x * 2.0f - 1.0f;
	y = y * 2.0f - 1.0f;

	float3 vD;

	float3 dir = vDir - vUp*y + vCp*x;

	if (bDir) vD = cross(dir, vCp);
	else	  vD = cross(dir, vUp);

	vD = normalize(vD) * fD;

	float3 color = texCUBE(tCube, dir).rgb;
	float3 vX = 0;
	float f = 0.75f;
	float a = 0.5f;

	for (int i = 1; i < 16; i++) {
		vX += vD;
		color += f * texCUBE(tCube, dir + vX).rgb;
		color += f * texCUBE(tCube, dir - vX).rgb;
		a += f;
		f *= 0.75f;
	}
	color /= (a*2.0f);
	return float4(color, 1);
}


float4 PS2DBlur(float x : TEXCOORD0, float y : TEXCOORD1) : COLOR
{
	return float4(0.5, 0.5, 0.5, 1.0);
}
