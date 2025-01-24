
#define KERNEL_RADIUS 2.0f

sampler tShadowMap[3] : register(s13);

// ============================================================================
//
float4 Paraboloidal_LVLH(sampler s, float3 i)
{
	float z = dot(gCameraPos, i);
	float2 p = float2(dot(gEast, i), dot(gNorth, i)) / (1.0f + abs(z));
	p *= float2(0.2273f, 0.4545f);
	float4 A = tex2D(s, p + float2(0.25f, 0.5f));
	float4 B = tex2D(s, p + float2(0.75f, 0.5f));
	return lerp(A, B, smoothstep(-0.03, 0.03, z));
}

float3 Sq(float3 x)
{
	return x*x;
}

float4 Sq(float4 x)
{
	return x*x;
}

bool PointInRect(float2 pt)
{
	if (pt.x < 0) return false;
	if (pt.x > 1) return false;
	if (pt.y < 0) return false;
	if (pt.y > 1) return false;
	return true;
}

// ==========================================================================================================
// Local light sources
// ==========================================================================================================


float3 Light_fx(float3 x)
{
	return saturate(x);  //1.5 - exp2(-x.rgb)*1.5f;
}

void LocalLights(
	out float3 diff_out,
	out float3 spec_out,
	in float3 nrmW,
	in float3 posW,
	in float sp,
	uniform int x,
	uniform bool bSpec)
{

	float3 posWN = normalize(-posW);
	float3 p[4];
	float4 spe;
	int i;

	// Relative positions
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position;

	// Square distances
	float4 sd;
	[unroll] for (i = 0; i < 4; i++) sd[i] = dot(p[i], p[i]);

	// Normalize
	sd = rsqrt(sd);
	[unroll] for (i = 0; i < 4; i++) p[i] *= sd[i];

	// Distances
	float4 dst = rcp(sd);

	// Attennuation factors
	float4 att;
	[unroll] for (i = 0; i < 4; i++) att[i] = dot(gLights[i + x].attenuation.xyz, float3(1.0, dst[i], dst[i] * dst[i]));

	att = rcp(att);

	// Spotlight factors
	float4 spt;
	[unroll] for (i = 0; i < 4; i++) {
		spt[i] = (dot(p[i], gLights[i + x].direction) - gLights[i + x].param[Phi]) * gLights[i + x].param[Theta];
		if (gLights[i + x].type == 0) spt[i] = 1.0f;
	}

	spt = saturate(spt);

	// Diffuse light factors
	float4 dif;
	[unroll] for (i = 0; i < 4; i++) dif[i] = dot(-p[i], nrmW);

	dif = saturate(dif);
	dif *= (att*spt);

	// Specular lights factors
	if (bSpec) {

		[unroll] for (i = 0; i < 4; i++) spe[i] = dot(reflect(p[i], nrmW), posWN) * (dif[i] > 0);

		spe = pow(saturate(spe), sp);
		spe *= (att*spt);
	}

	diff_out = 0;
	spec_out = 0;

	[unroll] for (i = 0; i < 4; i++) diff_out += gLights[i + x].diffuse.rgb * dif[i];

	if (bSpec) {
		[unroll] for (i = 0; i < 4; i++) spec_out += gLights[i + x].diffuse.rgb * spe[i];
	}
}


void LocalLightsBeckman(
	out float3 diff_out,
	out float3 spec_out,
	in float3 nrmW,
	in float3 posW,
	in float fRgh,
	uniform int x,
	uniform bool bSpec)
{

	float3 camW = normalize(-posW);
	float3 p[4];
	float3 H[4];
	float4 spe;
	float4 dHN;
	int i;

	// Relative positions
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position;

	// Square distances
	float4 sd;
	[unroll] for (i = 0; i < 4; i++) sd[i] = dot(p[i], p[i]);

	// Normalize
	sd = rsqrt(sd);
	[unroll] for (i = 0; i < 4; i++) p[i] *= sd[i];

	// Distances
	float4 dst = rcp(sd);

	if (bSpec) {

		// Halfway Vectors
		float4 hd;
		[unroll] for (i = 0; i < 4; i++) H[i] = (camW - p[i]);
		[unroll] for (i = 0; i < 4; i++) hd[i] = dot(H[i], H[i]);

		hd = rsqrt(hd);

		[unroll] for (i = 0; i < 4; i++) H[i] *= hd[i];
		[unroll] for (i = 0; i < 4; i++) dHN[i] = dot(H[i], nrmW);
	}

	

	// Attennuation factors
	float4 att;
	[unroll] for (i = 0; i < 4; i++) att[i] = dot(gLights[i + x].attenuation.xyz, float3(1.0, dst[i], dst[i] * dst[i]));

	att = rcp(att);

	// Spotlight factors
	float4 spt;
	[unroll] for (i = 0; i < 4; i++) {
		spt[i] = (dot(p[i], gLights[i + x].direction) - gLights[i + x].param[Phi]) * gLights[i + x].param[Theta];
		if (gLights[i + x].type == 0) spt[i] = 1.0f;
	}

	spt = saturate(spt);

	// Diffuse light factors
	float4 dif;
	[unroll] for (i = 0; i < 4; i++) dif[i] = dot(-p[i], nrmW);

	dif = saturate(dif);

	// Specular lights factors
	
	if (bSpec) {

		float r2 = fRgh*fRgh;
		float4 d2 = dHN*dHN;
		float4 w = rcp(3.14*r2*d2*d2);
		float4 q = rcp(r2*d2);

		spe = (att*spt*dif) * w * exp((d2 - 1.0f) * q);
	}

	dif *= (att*spt);

	diff_out = 0;
	spec_out = 0;

	[unroll] for (i = 0; i < 4; i++) diff_out += Sq(gLights[i + x].diffuse.rgb * dif[i]);

	if (bSpec) {
		[unroll] for (i = 0; i < 4; i++) spec_out += gLights[i + x].diffuse.rgb * spe[i];
	}
}



void LocalLightsEx(out float3 cDiffLocal, out float3 cSpecLocal, in float3 nrmW, in float3 posW, in float sp, uniform bool ubBeckman)
{

#if LMODE !=0
	if (!gLightsEnabled) {
		cDiffLocal = 0;
		cSpecLocal = 0;
	}
#endif

#if LMODE == 1
	if (ubBeckman) LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
	else LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);

#elif LMODE == 2
	if (ubBeckman) LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
	else LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);

#elif LMODE == 3
	float3 dd, ss;
	if (ubBeckman) {
		LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
		LocalLightsBeckman(dd, ss, nrmW, posW, sp, 4, false);
	}
	else {
		LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
		LocalLights(dd, ss, nrmW, posW, sp, 4, false);
	}
	cDiffLocal += dd;
	cSpecLocal += ss;

#elif LMODE == 4
	float3 dd, ss;
	if (ubBeckman) {
		LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
		LocalLightsBeckman(dd, ss, nrmW, posW, sp, 4, true);
	}
	else {
		LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
		LocalLights(dd, ss, nrmW, posW, sp, 4, true);
	}
	cDiffLocal += dd;
	cSpecLocal += ss;
#else
	cDiffLocal = 0;
	cSpecLocal = 0;
#endif
}





// ==========================================================================================================
// Object Self Shadows
// ==========================================================================================================

float SampleShadows(float2 sp, float pd, int sid)
{

	float2 dxa = float2(gSHD[1], 0) * 0.75f;
	float2 dya = float2(0, gSHD[1]) * 0.75f;
	float2 dxb = dxa * 0.707f;
	float2 dyb = dya * 0.707f;	
	float  va = 0;

	if ((tex2D(tShadowMap[sid], sp - dxb - dyb).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp - dya).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp + dxb - dyb).r) > pd) va++;
	
	if ((tex2D(tShadowMap[sid], sp - dxa).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp + dxa).r) > pd) va++;
	
	if ((tex2D(tShadowMap[sid], sp - dxb + dyb).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp + dya).r) > pd) va++;
	if ((tex2D(tShadowMap[sid], sp + dxb + dyb).r) > pd) va++;

	return va * 0.1111111f;
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadows2(float2 sp, float pd, int sid)
{
	
	float val = 0;
	float m = KERNEL_RADIUS * gSHD[1];

	[unroll] for (int i = 0; i < KERNEL_SIZE; i++) {
		if ((tex2D(tShadowMap[sid], sp + kernel[i].xy * m).r) > pd) val += kernel[i].z;
	}

	return saturate(val * KERNEL_WEIGHT);
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadows3(float2 sp, float pd, float4 frame, int sid)
{

	float val = 0;
	frame *= KERNEL_RADIUS * gSHD[1];

	[unroll] for (int i = 0; i < KERNEL_SIZE; i++) {
		float2 ofs = frame.xy*kernel[i].x + frame.zw*kernel[i].y;
		if (tex2D(tShadowMap[sid], sp + ofs).r > pd) val += kernel[i].z;
	}

	return saturate(val * KERNEL_WEIGHT);
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadowsEx(float2 sp, float pd, float4 sc, int sid)
{
	
#if SHDMAP == 1
	return SampleShadows(sp, pd, sid);
#elif SHDMAP == 2 || SHDMAP == 4
	return SampleShadows2(sp, pd, sid);
#else
	float si, co;
	sc += (gSHD[2] * 2.0f);
	sincos(sc.y + sc.x*149.0f, si, co);
	return SampleShadows3(sp, pd, float4(si, co, co, -si), sid);
#endif
}



// ---------------------------------------------------------------------------------------------------
//
float ComputeShadow(float4 shdH, float dLN, float4 sc)
{
	if (!gShadowsEnabled) return 1.0f;

	shdH.xyz /= shdH.w;
	shdH.z = 1 - shdH.z;
	float2 sp = shdH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	sp += gSHD[1] * 0.5f;

	if (sp.x < 0 || sp.y < 0) return 1.0f;	// If a sample is outside border -> fully lit
	if (sp.x > 1 || sp.y > 1) return 1.0f;
	
	float fShadow;

	float kr = gSHD[0] * KERNEL_RADIUS;
	float dx = rsqrt(1.0 - dLN*dLN);
	float ofs = 0.33f * kr / (dLN * dx);
	float omx = min(gSHD[0] * 2.0f + max(0, ofs), 0.25);
	
	float  pd = shdH.z + omx * gSHD[3];

	if (pd < 0) pd = 0;
	if (pd > 1) pd = 1;

	fShadow = SampleShadowsEx(sp, pd, sc, 0);
	
	return 1 - fShadow;
}


// ---------------------------------------------------------------------------------------------------
//
float ComputeShadowVC(float4 shdH, float dLN, float4 sc)
{
	if (!gShadowsEnabled) return 1.0f;

	shdH.xyz /= shdH.w;
	shdH.z = 1 - shdH.z;
	float2 sp = shdH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	float2 c[3];
	int sid = 0;

#if CASCOUNT >= 1
	c[0] = (sp + gSHDSubRect[0].xy) * gSHDSubRect[0].zw;
	if (!PointInRect(c[0])) return 1.0f; // Sample outside of cascade '0'
#endif
#if CASCOUNT >= 2
	c[1] = (sp + gSHDSubRect[1].xy) * gSHDSubRect[1].zw;
	if (PointInRect(c[1])) sid = 1;
#endif
#if CASCOUNT >= 3
	c[2] = (sp + gSHDSubRect[2].xy) * gSHDSubRect[2].zw;
	if (PointInRect(c[2])) sid = 2;
#endif

	float kr = gSHDPx[sid];
	float omx = max(0.0015f, kr * sqrt(rcp(dLN * dLN) - 1.0f));
	float  pd = shdH.z + min(omx, 0.02f) * gSHD[3];

	if (pd < 0) pd = 0;
	if (pd > 1) pd = 1;

	float fShadow[3];

#if CASCOUNT >= 1
	fShadow[0] = SampleShadows(c[0], pd, 0);
#endif
#if CASCOUNT >= 2
	fShadow[1] = SampleShadows(c[1], pd, 1);
#endif
#if CASCOUNT >= 3
	fShadow[2] = SampleShadows(c[2], pd, 2);
#endif

	return 1 - fShadow[sid];
}


// ---------------------------------------------------------------------------------------------------
//
float3 VisualizeCascades(float4 shdH)
{
	if (!gShadowsEnabled) return float3(1, 1, 1);

	shdH.xyz /= shdH.w;
	shdH.z = 1 - shdH.z;
	float2 sp = shdH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	sp += gSHD[1] * 0.5f; // Shift 0.5 pixels aside

	float2 c0 = (sp + gSHDSubRect[0].xy) * gSHDSubRect[0].zw;
	float2 c1 = (sp + gSHDSubRect[1].xy) * gSHDSubRect[1].zw;
	float2 c2 = (sp + gSHDSubRect[2].xy) * gSHDSubRect[2].zw;

#if CASCOUNT >= 3
	if (PointInRect(c2)) return float3(0, 0, 1);
#endif
#if CASCOUNT >= 2
	if (PointInRect(c1)) return float3(0, 1, 0);
#endif
#if CASCOUNT >= 1
	if (PointInRect(c0)) return float3(1, 0, 0);
#endif

	return float3(1, 1, 1);
}
