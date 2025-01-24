// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// ==============================================================



float3 cLuminosity = { 0.4, 0.7, 0.3 };


inline float cmax(float3 color)
{
	return max(max(color.r, color.g), color.b);
}

// Sun light brightness for diffuse and specular lighting
#include "LightBlur.hlsl"

// Incluse Light and Shadow
#include "Common.hlsl"

// Must be included here
#include "PBR.fx"

// Must be included here
#include "Metalness.fx"


// ============================================================================
// Vertex shader for physics based rendering
//
PBRData AdvancedVS(MESH_VERTEX vrt)
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
float4 AdvancedPS(float4 sc : VPOS, PBRData frg) : COLOR
{
	float3 bitW;
	float3 nrmT;
	float3 cRefl;
	float3 cEmis;
	float4 cSpec;
	float4 cTex;

	float3 cDiffLocal;
	float3 cSpecLocal;

	if (gTextured) cTex = tex2D(WrapS, frg.tex0.xy);
	else		   cTex = 1;

	if (gOITEnable) if (cTex.a < 0.5f) clip(-1);

	if (gCfg.Norm) nrmT  = tex2D(Nrm0S, frg.tex0.xy).rgb;

	if (gCfg.Spec) cSpec = tex2D(SpecS, frg.tex0.xy);
	else		   cSpec = gMtrl.specular;

	if (gCfg.Refl) cRefl = tex2D(ReflS, frg.tex0.xy).rgb;
	else		   cRefl = gMtrl.reflect.rgb;

	// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
	if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb;
	else		   cEmis = 0;


	float3 nrmW = frg.nrmW;
	float3 tanW = frg.tanW.xyz;
	float3 cSun = saturate(gSun.Color);
	float3 CamD = normalize(frg.camW);
	float3 Base = (gMtrl.ambient.rgb*gSun.Ambient) + (gMtrl.emissive.rgb);


	// Compute World space normal -------------------------------------------
	//
	if (gCfg.Norm) {
		nrmT = nrmT * 2.0 - 1.0;
		bitW = cross(tanW, nrmW) * frg.tanW.w;
		nrmW = nrmW*nrmT.z + tanW*nrmT.x + bitW*nrmT.y;
	}

	nrmW  = normalize(nrmW);

	float3 TnrmW = -nrmW;
	float3 RflW  = reflect(-CamD, nrmW);
	float  dLN   = saturate(-dot(gSun.Dir, nrmW));

	if (gCfg.Spec) cSpec.a *= 255.0f;

	// Approximate roughness
	float fRghn = log2(cSpec.a) * 0.1f;

	// Sunlight calculation
	float fSun = pow(saturate(-dot(RflW, gSun.Dir)), cSpec.a) * saturate(cSpec.a);

	if (dLN == 0) fSun = 0;

	// Special alpha only texture in use
	cTex.rgb = saturate(cTex.rgb + gNoColor.rgb);


	// ----------------------------------------------------------------------
	// Add vessel self-shadows
	// ----------------------------------------------------------------------

#if SHDMAP > 0
	if (!gCockpit) cSun.rgb *= ComputeShadow(frg.shdH, dLN, sc);
#endif



	// ----------------------------------------------------------------------
	// Compute Local Light Sources
	// ----------------------------------------------------------------------

	LocalLightsEx(cDiffLocal, cSpecLocal, nrmW, -frg.camW, cSpec.a, false);


	// Lit the diffuse texture
	cTex.rgb *= saturate(Base + gMtrl.diffuse.rgb * Light_fx(cDiffLocal + cSun * dLN));

	// Lit the specular surface
	cSpec.rgb *= saturate(cSpecLocal + fSun * cSun);


	// Compute Transluciency effect --------------------------------------------------------------
	//

	if (gCfg.Transm || gCfg.Transl) {

		float4 cTransm = float4(cTex.rgb, 1.0f);

		if (gCfg.Transm) {
			cTransm = tex2D(TransmS, frg.tex0.xy);
			cTransm.a *= 1024.0f;
		}

		float3 cTransl = cTex.rgb;

		if (gCfg.Transl) {
			cTransl = tex2D(TranslS, frg.tex0.xy).rgb;
		}	

		float sunLightFromBehind = saturate(dot(gSun.Dir, nrmW));
		float sunSpotFromBehind = pow(saturate(dot(gSun.Dir, CamD)), cTransm.a);
		sunSpotFromBehind *= saturate(sunLightFromBehind * 3.0f);// Causes the transmittance (sun spot) effect to fall off at very shallow angles

		cTransl.rgb *= saturate(cSun * sunLightFromBehind);

		cTex.rgb += (1 - cTex.rgb) * cTransl.rgb;
		cTex.rgb += cTransm.rgb * (sunSpotFromBehind * cSun);
	}


	float fFrsl = 1.0f;
	float fInt = 0.0f;

	// Compute reflectivity
	float fRefl = cmax(cRefl);


#if defined(_ENVMAP)


	// Compute environment map/fresnel effects --------------------------------
	//
	if (gEnvMapEnable) {

		// Do we need fresnel code for this render pass ?

		if (gFresnel) {
		
			fFrsl = gMtrl.fresnel.y;

			// Get mirror reflection for fresnel
			float3 cEnvFres = texCUBElod(EnvMapAS, float4(RflW, 0)).rgb;

			float  dCN = saturate(dot(CamD, nrmW));

			// Compute a fresnel term with compensations included
			fFrsl *= pow(1.0f - dCN, gMtrl.fresnel.x) * (1.0 - fRefl) * any(cRefl);

			// Sunlight reflection for fresnel material
			cSpec.rgb = saturate(cSpec.rgb + fSun * fFrsl * cSun);

			// Compute total reflected light with fresnel reflection
			// and accummulate in cSpec
			cSpec.rgb = saturate(cSpec.rgb + fFrsl * cEnvFres);

			// Compute intensity
			fInt = saturate(dot(cSpec.rgb, cLuminosity));

			// Attennuate diffuse surface
			cTex.rgb *= (1.0f - fInt);
		}

		// Compute LOD level for blur effect
		float fLOD = (1.0f - fRghn) * 10.0f;

		float3 cEnv = texCUBElod(EnvMapAS, float4(RflW, fLOD)).rgb;

		// Compute total reflected light, accummulate in cSpec
		cSpec.rgb += cRefl.rgb * cEnv;
	}

#endif

	// Attennuate diffuse surface
	cTex.rgb *= (1.0f - fRefl);

	// Re-compute output alpha for alpha blending stage
	// NOTE: Without fresnel fInt remains zero
	cTex.a = saturate(cTex.a + fInt);

	// Add reflections to output
	cTex.rgb += cSpec.rgb;

	// Add emissive textures to output
	cTex.rgb += cEmis;

#if defined(_DEBUG)
	//if (gDebugHL) cTex = cTex*0.5f + gColor;
	cTex = cTex * (1 - gColor*0.5f) + gColor;
#endif

	cTex.rgb *= gSun.Transmission;
	cTex.rgb += gSun.Inscatter;

	return cTex;
}





// ============================================================================
// This is the default mesh rendering technique
//
technique VesselTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 PBR_VS();
		pixelShader = compile ps_3_0 PBR_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 AdvancedVS();
		pixelShader = compile ps_3_0 AdvancedPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P2
	{
		vertexShader = compile vs_3_0 FAST_VS();
		pixelShader = compile ps_3_0 FAST_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P3	// XR2 HUD PASS
	{
		vertexShader = compile vs_3_0 FAST_VS();
		pixelShader = compile ps_3_0 XRHUD_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P4
	{
		vertexShader = compile vs_3_0 MetalnessVS();
		pixelShader = compile ps_3_0 MetalnessPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P5
	{
		vertexShader = compile vs_3_0 MetalnessVS();
		pixelShader = compile ps_3_0 BakedVC_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}
}
