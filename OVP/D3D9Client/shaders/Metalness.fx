// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// ==============================================================

#define eps 0.001f
//#define _VISCASCADES

// ============================================================================
// Vertex shader for physics based rendering
//
PBRData MetalnessVS(MESH_VERTEX vrt)
{
	// Zero output.
	PBRData outVS = (PBRData)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;

#if SHDMAP > 0
	outVS.shdH = mul(float4(posW, 1.0f), gLVP);
#endif

	outVS.nrmW = nrmW;
	outVS.tanW = float4(mul(float4(vrt.tanL, 0.0f), gW).xyz, vrt.tex0.z);
	outVS.posH = mul(float4(posW, 1.0f), gVP);
	outVS.camW = -posW;
	outVS.tex0 = vrt.tex0.xy;

	return outVS;
}


// ============================================================================
//
float BeckmanNDF(float dHN, float rgh)
{
	float r2 = rgh*rgh;
	float dHN2 = dHN*dHN;
	float2 w = rcp(float2(3.14f * r2 * dHN2*dHN2, r2*dHN2));
	return w.x * exp((dHN2 - 1.0f) * w.y);
}

// ============================================================================
//
float GGX_NDF(float dHN, float rgh)
{
	float r2 = rgh*rgh;
	float dHN2 = dHN*dHN;
	float d = (r2 * dHN2) + (1.0f - dHN2);
	return r2 / (3.14f * d * d);
}


// ============================================================================
//
float SchlickBeckmanGSF(float dLN, float dCN, float rgh) // pre-devided by dLN * dCN
{
	float2 dots = clamp(float2(dLN, dCN), eps, 1.0f); // Avoid div-by-zero
	float  r2 = rgh * rgh;
	float2 e; e.xy = (1.0f - r2);
	float2 w = rcp((dots * e) + r2);
	return w.x*w.y;
}


// ============================================================================
//
float DiffuseRetroReflectance(float dLN, float dCN, float dLH, float rgh, float mtl)
{
	float2 q = (1.0f-float2(dLN, dCN));	q *= q*q;
	float z = 0.5f + 1.6f * dLH*dLH * rgh;
	return ((1.0f - q.x) + z*q.x) * ((1.0f - q.y) + z*q.y);
}


// ============================================================================
//
float3 LightFX(float3 c)
{
	float q = cmax(c);
	return c * rsqrt(2 + q*q) * 1.8f;
}

// ============================================================================
//
float3 LightFXSq(float3 c)
{
	c = sqrt(c);
	float q = cmax(c);
	return c * rsqrt(2 + q*q) * 1.8f;
}


// ============================================================================
//
void SampleEnvMap(out float3 cE, float dCN, float fRgh, float fMetal, float3 rflW, float3 nrmW)
{
	// Sharpen reflection at low angles 
	fRgh = saturate(fRgh - 0.1f);
	float fLOD = fRgh * lerp(dCN * 2.0f, (0.2f + dCN*0.8f) * 2.5f, fMetal);	// Compute LOD level for blur effect

	fLOD *= 5.0f * rsqrt(1.0f + fLOD*fLOD);

	cE = texCUBElod(EnvMapAS, float4(rflW, fLOD)).rgb;
}


// ============================================================================
//
void Transmittance(in out float4 cDiff, float uLN, float uLC, float2 uv, float3 cSun)
{
	float4 cTransm = float4(cDiff.rgb, 1.0f);
	float3 cTransl = cDiff.rgb;
	
	if (gCfg.Transm) {
		cTransm = tex2D(TransmS, uv);
		cTransm.a *= 1024.0f;
	}

	if (gCfg.Transl) cTransl = tex2D(TranslS, uv).rgb;

	float sunLightFromBehind = saturate(-uLN);
	float sunSpotFromBehind = saturate(pow(saturate(-uLC), cTransm.a) * 3.0); // "3.0" Causes the transmittance (sun spot) effect to fall off at very shallow angles

	cDiff.rgb += (1.0f - cDiff.rgb) * cTransl.rgb * saturate(cSun * sunLightFromBehind);
	cDiff.rgb += cTransm.rgb * (sunSpotFromBehind * cSun);
}

#include "BakedVC.fx"

// ============================================================================
// A Shader for a typical "Metalness" PBR workflow.
// ============================================================================

float4 MetalnessPS(float4 sc : VPOS, PBRData frg) : COLOR
{
	float3 nrmT;
	float3 nrmW;
	float3 cEmis;
	float4 cSpecularMap;
	float4 cDiff;
	float  fHeat;
	float  fSmth, fMetal;
	float3 cDiffLocal;
	float3 cSpecLocal;

	// ======================================================================
	// Start fetching texture data
	// ======================================================================

	if (gTextured) cDiff = tex2D(WrapS, frg.tex0.xy);
	else		   cDiff = 1;

#if defined(_VISCASCADES)
	cDiff.rgb *= VisualizeCascades(frg.shdH);
	return cDiff;
#endif

	if (gOITEnable) if (cDiff.a < 0.5f) clip(-1);

	// Fetch a normal map
	//
	if (gCfg.Norm) nrmT = tex2D(Nrm0S, frg.tex0.xy).rgb;

	// Fetch Smoothness map (i.e. *_rghn.dds)
	//
	if (gCfg.Rghn) fSmth = tex2D(RghnS, frg.tex0.xy).g;
	else		   fSmth = 1.0f;

	// Fetch Metalness map
	//
	if (gCfg.Metl) fMetal = tex2D(MetlS, frg.tex0.xy).g;
	else		   fMetal = gMtrl.metalness;

	// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
	//
	if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb;
	else		   cEmis = 0;

	// Sample specular map																										// Added
	//																															// Added
	if (gCfg.Spec) cSpecularMap = tex2D(SpecS, frg.tex0.xy).rgba;																// Added

	// Fetch Heat map
	//
	if (gCfg.Heat) fHeat = saturate((tex2D(HeatS, frg.tex0.xy).g) - 1.0f + (gMtrl.specialfx.x * gMtrl.specialfx.x * gMtrl.specialfx.x));
	else fHeat = (gMtrl.specialfx.x * gMtrl.specialfx.x * gMtrl.specialfx.x);

	// ----------------------------------------------------------------------
	// Now do other calculations while textures are being fetched
	// ----------------------------------------------------------------------

	float3 camW = normalize(frg.camW);
	float3 cSun = gSun.Color * lerp(float3(1.1, 1.1, 0.9), float3(1,1,1), saturate(gRadius[3]*2e-5));


	// ======================================================================
	// Construct a proper world space normal
	// ======================================================================

	float3 tanW = frg.tanW.xyz;
	float3 bitW = cross(tanW, frg.nrmW) * frg.tanW.w;
	
	if (gCfg.Norm) {
		nrmT.rg = nrmT.rg * 2.0f - 1.0f;
		nrmW = frg.nrmW*nrmT.z + tanW*nrmT.x + bitW*nrmT.y;
	}
	else nrmW = frg.nrmW;

	nrmW = normalize(nrmW);
	


	// ======================================================================
	// Typical compatibility requirements
	// ======================================================================

	cDiff.a = saturate(cDiff.a * gMtrlAlpha);
	cDiff.rgb = saturate(cDiff.rgb + gNoColor.rgb);
	

	// ======================================================================
	// Some Precomputations
	// ======================================================================

	float3 sunW = -gSun.Dir;	
	float3 cEnv = 0;
	
	float3 rflW = reflect(-camW, nrmW);
	float3 hlvW = normalize(camW + sunW);

	// Dot Products
	float uLN = dot(sunW, nrmW);
	float uLC = dot(sunW, camW);
	float dLN = saturate(uLN);
	float dLH = saturate(dot(sunW, hlvW));
	float dCN = saturate(dot(camW, nrmW));
	float dHN = saturate(dot(hlvW, nrmW));
	
	// Apply a proper curve to a texture data, modulate with material value and clamp
	fSmth = pow(abs(fSmth), gMtrl.roughness.y) * gMtrl.roughness.x;

	// Apply fresnel and Fresnell cut-off to fSmth
	fSmth = fSmth + ((1.0f - fSmth) * pow(abs(1.0f - dCN), 4.0f)) * pow(abs(fSmth), 0.5f);

	float fRgh = saturate(1.0f - fSmth);
	float fRgh3 = fRgh*fRgh*fRgh;


	// ======================================================================
	// Compute Local Light Sources
	// ======================================================================

	LocalLightsEx(cDiffLocal, cSpecLocal, nrmW, -frg.camW, fRgh3, true);


#if defined(_ENVMAP)

	if (gEnvMapEnable) {

		// ======================================================================
		// Sample Env Map
		SampleEnvMap(cEnv, dCN, fRgh, fMetal, rflW, nrmW);
	}

#if defined(_IRRADIANCE)
	// ======================================================================
	// Sample Irradiance Map
	float3 cAmbient = Paraboloidal_LVLH(IrradS, nrmW).rgb;
	cAmbient *= cAmbient;

	//cAmbient = saturate(cAmbient * (1.0f + 15.0f * gNightTime));	
	// Apply base ambient light
	cAmbient = max(cAmbient, gSun.Ambient);
#endif
#endif

#if !defined(_ENVMAP) || !defined(_IRRADIANCE)
	// ======================================================================
	// Compute Earth glow
	float angl = saturate((-dot(gCameraPos, nrmW) - gProxySize) * gInvProxySize);
	float3 cAmbient = gAtmColor.rgb * max(0, angl * gGlowConst) + gSun.Ambient;
#endif


	cAmbient *= (1.0f - fMetal); // No ambient for metals


	// ======================================================================
	// Add vessel self-shadows
	// ======================================================================
#if SHDMAP > 0
	if (gCockpit) {
		cSun *= smoothstep(0, 0.72, ComputeShadowVC(frg.shdH, dLN, sc));
	}
	else {
		cSun *= smoothstep(0, 0.72, ComputeShadow(frg.shdH, dLN, sc));
	}
#endif
	

	// ======================================================================
	// Main shader core MetalnessPS
	// ======================================================================
	float  fD = GGX_NDF(dHN, lerp(0.01f, 1.0f, fRgh3));
	float  fG = SchlickBeckmanGSF(dLN, dCN, fRgh);
	float  fR = DiffuseRetroReflectance(dLN, dCN, dLH, fRgh, fMetal); 
	
	float3 cS2 = 0;																											// Added
	float3 cSpec2 = 0;																										// Added
	
	// Base material color for reflections. Use cDiff for metals and very rough plastics, white for the rest. 
	// cDiff for rough plastics is to avoid washed-out(white) look of black and rough parts.
	float3 cSpec = lerp(cDiff.rgb, float3(1, 1, 1), (1.0f - fMetal) * (1.0f - fRgh3));

	// Fresnel power 2.5 for glossy, 5.0 for rough
	float fFrs = pow(1.0f - dCN, fRgh*2.5 + 2.5f);

	// Fresnel cut-off below X of fSmth
	fFrs *= saturate(0.3f - fRgh*fRgh) * 3.3f;

	// Assume that plastics absorve 50-90% of specular light
	float  fP = lerp(0.1f + (1.0f - fRgh)*0.4f, 1.0f, fMetal);


	// ======================================================================												// Start of Added section
	// Add multilayer texture effect 
	// ======================================================================
	if (gCfg.Spec) {
		cSpec2 = cSpecularMap.rgb;
		cSpecularMap.a = 1.0f - cSpecularMap.a; // use this
		// cSpecularMap.a = 0.0f;
		float fD2 = GGX_NDF(dHN, lerp(0.01f, 1.0f, cSpecularMap.a)); // makes the sun glint larger at full smoothness so that it doesn't disappear

		// Specular Color
		cS2 = (fD2 * cSpec2 * fP) * cSpecularMap.a ; // (4.0f*dLN*dCN) removed to avoid division by zero, compensation in GSF
		// cS2 = (fD2 * cSpec2 * fG * fP) * 0.25f; // (4.0f*dLN*dCN) removed to avoid division by zero, compensation in GSF
	}																														// end of Added section

	// Fresnel color shift
	float3 cF = cSpec + (1.0f - cSpec) * fFrs;

	// Specular Color
	float3 cS = (fD * cF * fG * fP) * 0.25f; // (4.0f*dLN*dCN) removed to avoid division by zero, compensation in GSF

		
	// How plastics reflect the environment
	float  R = 0.1f * fSmth;
	float  frP = R + (1.0f - R) * fFrs;

	float3 cE = (cEnv * cF * lerp(frP, 1.0f, fMetal));

	// Attennuate diffuse color for Metals & Fresnel
	float  fA = (1.0f - fFrs) * (1.0f - fMetal);

	// Add a faint diffuse hue for rough metals. Rough metal doesn't look good if it's totally black
	fA += fRgh * fMetal * 0.05f;

	// Light terms
	float3 zL = Sq(cSun * fR * dLN) + cDiffLocal + Sq(cAmbient);

	float3 zD = cDiff.rgb * fA * LightFXSq(gMtrl.diffuse.rgb * zL + Sq(gMtrl.emissive.rgb));

	// Combine specular terms
	// float3 zS = cS * (cSun * dLN) + cSpec * LightFX(cSpecLocal) * 0.5f;
	float3 zS = cS * (cSun * dLN) + cSpec * LightFX(cSpecLocal) * 0.5f  + cS2 * (cSun * dLN) + cSpec2 * LightFX(cSpecLocal) * 0.5f;		// Modified
	
	cDiff.rgb = zD + zS + cE;

	// Override material alpha to make reflections visible
	cDiff.a = saturate(cDiff.a + cmax(zS + cE));

	
	// ======================================================================
	// Add texture transmittance 
	// ======================================================================

	if (gCfg.Transm || gCfg.Transl) Transmittance(cDiff, uLN, uLC, frg.tex0.xy, cSun);
	


	// Add emission texture to output, modulate with material
	cDiff.rgb = max(cDiff.rgb, cEmis * gMtrl.emission2.rgb);

	// Add heat glow
	//float3 cHeat = float3(pow(abs(fHeat), 0.5f), pow(abs(fHeat), 1.5f), pow(abs(fHeat), 8.0f));
	float3 cHeat = pow(abs(fHeat), float3(0.5f, 1.5f, 8.0f));
	cDiff.rgb = cDiff.rgb + cHeat;

#if defined(_DEBUG)
	cDiff = cDiff * (1.0f - gColor*0.5f) + gColor;
#endif

#if defined(_LIGHTGLOW)
	cDiff.rgb *= gSun.Transmission;
	cDiff.rgb += gSun.Inscatter;
	return cDiff;
#else
	float3 h2 = cDiff.rgb * cDiff.rgb;
	cDiff.rgb *= pow(max(0, 1.0f + h2 * h2), -0.25);

	cDiff.rgb *= gSun.Transmission;
	cDiff.rgb += gSun.Inscatter;

	return cDiff;
#endif
}
