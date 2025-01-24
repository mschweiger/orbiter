// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// licensed under MIT
// ==============================================================

uniform extern float3  vParTexPos[6]; // Pre-computed paraboloidal map coords for prime directions
uniform extern float3  fControl[16];
uniform extern int	   iCount;
uniform extern bool    bEnabled[6];
uniform extern float   fShine;
uniform extern bool	   bShine;

sampler tMap[16] : register(s0);	// Lightmaps
sampler tAO[6] : register(s0);		// AO Textures for prime directions
sampler tIrrad : register(s6);		// Paraboloidal irradiance map

float cmax(float3 a)
{
	return max(a.x, max(a.y, a.z));
}

// ============================================================================
//
float3 LightFX(float3 c)
{
	float q = cmax(c);
	return c * 1.2f * rsqrt(2 + q * q);
}


// ============================================================================
//
float4 ParaboloidalSampler(sampler s, float3 p)
{
	float4 A = tex2D(s, p.xy + float2(0.25f, 0.5f));
	float4 B = tex2D(s, p.xy + float2(0.75f, 0.5f));
	return lerp(A, B, smoothstep(-0.03, 0.03, p.z));
}



// ============================================================================
// Combine multiple (regular) baked lightmaps into a single map
//
float4 PSMain(float x : TEXCOORD0, float y : TEXCOORD1) : COLOR
{
	float3 color = 0;
	[unroll] for (int i = 0; i < iCount; i++) color += tex2D(tMap[i], float2(x, y)).rgb * fControl[i];
	return float4(LightFX(color), 1.0f);
}



// ============================================================================
// Combine multiple baked (ambient) lightmaps into a single map
// Include sun and planet shine coming through windows
//
float4 PSSunAO(float x : TEXCOORD0, float y : TEXCOORD1) : COLOR
{
	float3 color = 0;

	// For a given pixel in texture (x,y)
	[unroll] for (int i = 0; i < 6; i++) { // Browse through direction
		if (bEnabled[i]) {		
			// Get ambient distribution inside VC for a given light direction 'i'
			float3 ad = tex2D(tAO[i], float2(x, y)).rgb;

			float3 cShine = 0.0f;
			// Get planet shine color for a given direction 'i'
			if (bShine) cShine = ParaboloidalSampler(tIrrad, vParTexPos[i]).rgb;
			// Compute sunlight and planet shine factors
			float3 shineAO = ad * cShine * fShine;
			float3 sunAO = ad * fControl[i];
			color += (sunAO + shineAO);
		}
	}
	return float4(LightFX(color), 1.0f);
}
