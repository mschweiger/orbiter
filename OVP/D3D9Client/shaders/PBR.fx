// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2014 - 2018 Jarmo Nikkanen
// ==============================================================




struct PBRData
{
	float4 posH     : POSITION0;
	float3 camW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
	float4 tanW     : TEXCOORD3;	 // Handiness in .w
#if SHDMAP > 0
	float4 shdH     : TEXCOORD4;
#endif
};



// ========================================================================================================================
// Vertex shader for physics based rendering
//
PBRData PBR_VS(MESH_VERTEX vrt)
{
    // Zero output.
	PBRData outVS = (PBRData)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;

	outVS.nrmW = nrmW;
	outVS.tanW = float4(mul(float4(vrt.tanL, 0.0f), gW).xyz, vrt.tex0.z);
	outVS.posH = mul(float4(posW, 1.0f), gVP);

#if SHDMAP > 0
	outVS.shdH = mul(float4(posW, 1.0f), gLVP);
#endif

    outVS.camW = -posW;
    outVS.tex0 = vrt.tex0.xy;

    return outVS;
}






// ============================================================================
//
float4 PBR_PS(float4 sc : VPOS, PBRData frg) : COLOR
{
	float3 nrmT;
	float3 nrmW;
	float3 cEmis;
	float3 cRefl, cRefl2, cRefl3;
	float3 cFrsl = 1;
	float4 cDiff;
	float4 cSpec;
	float4 sMask = float4(1.0f, 1.0f, 1.0f, 1024.0f);
	float  fRghn;
	float3 cDiffLocal;
	float3 cSpecLocal;


	// ----------------------------------------------------------------------
	// Start fetching texture data
	// ----------------------------------------------------------------------

	if (gTextured) cDiff = tex2D(WrapS, frg.tex0.xy);
	else		   cDiff = 1;

	if (gOITEnable) if (cDiff.a < 0.5f) clip(-1);

	// Fetch a normal map
	//
	if (gCfg.Norm) nrmT = tex2D(Nrm0S, frg.tex0.xy).rgb;


	// Sample specular map
	if (gCfg.Spec) cSpec = tex2D(SpecS, frg.tex0.xy).rgba * sMask;
	else 		   cSpec = gMtrl.specular.rgba;


	// Use _refl color for both
	if (gCfg.Refl) cRefl = tex2D(ReflS, frg.tex0.xy).rgb;
	else		   cRefl = gMtrl.reflect.rgb;


	// Roughness map
	if (gCfg.Rghn) fRghn = tex2D(RghnS, frg.tex0.xy).g;
	else		   fRghn = gMtrl.roughness.r;


	// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
	if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb;
	else		   cEmis = 0;



	// ----------------------------------------------------------------------
	// Now do other calculations while textures are being fetched
	// ----------------------------------------------------------------------

	float3 CamD = normalize(frg.camW);
	float3 cSun = saturate(gSun.Color);


	// Use alpha zero to mask off specular reflections
	cSpec.rgb *= saturate(cSpec.a);

	// ----------------------------------------------------------------------
	// "Legacy/PBR" switch
	// ----------------------------------------------------------------------

	if (gPBRSw) {
		cRefl2 = cRefl*cRefl;
		cRefl3 = cRefl2*cRefl;
		cSpec.rgb = cRefl2;
		cSpec.a = exp2(fRghn * 12.0f);					// Compute specular power
	}
	else {
		cRefl3 = cRefl2 = cRefl;
	}

	float fRefl = cmax(cRefl3);


	// ----------------------------------------------------------------------
	// cSpec.pwr to fRghn Converter
	// ----------------------------------------------------------------------

	if (gRghnSw) {
		fRghn = log2(cSpec.a+1.0f) * 0.1f;
	}




	// ----------------------------------------------------------------------
	// Construct a proper world space normal
	// ----------------------------------------------------------------------

	if (gCfg.Norm) {
		float3 bitW = cross(frg.tanW.xyz, frg.nrmW) * frg.tanW.w;
		nrmT.rg = nrmT.rg * 2.0f - 1.0f;
		nrmW = frg.nrmW*nrmT.z + frg.tanW.xyz*nrmT.x + bitW*nrmT.y;
	}
	else nrmW = frg.nrmW;

	nrmW = normalize(nrmW);



	// ----------------------------------------------------------------------
	// Compute reflection vector and some required dot products
	// ----------------------------------------------------------------------

	float3 RflW = reflect(-CamD, nrmW);				// Reflection vector
	float dRS = saturate(-dot(RflW, gSun.Dir));		// Reflection/sun angle
	float dLN = saturate(-dot(gSun.Dir, nrmW));		// Diffuse lighting term
	float dLNx = saturate(dLN * 80.0f);				// Specular, Fresnel shadowing term


	// ----------------------------------------------------------------------
	// Add vessel self-shadows
	// ----------------------------------------------------------------------

#if SHDMAP > 0
	if (!gCockpit) cSun *= smoothstep(0, 0.72, ComputeShadow(frg.shdH, dLN, sc));
#endif


	// ----------------------------------------------------------------------
	// Compute a fresnel terms fFrsl, iFrsl, fFLbe
	// ----------------------------------------------------------------------

	float fFrsl = 0;	// Fresnel angle co-efficiency factor
	float iFrsl = 0;	// Fresnel intensity
	float fFLbe = 0;	// Fresnel lobe

#if defined(_GLASS)

	if (gFresnel) {

		float dCN = saturate(dot(CamD, nrmW));

		// Compute a fresnel term
		fFrsl = pow(1.0f - dCN, gMtrl.fresnel.x);

		// Compute a specular lobe for fresnel reflection
		fFLbe = pow(dRS, gMtrl.fresnel.z) * dLNx * any(cRefl);

		// Modulate with material
		cFrsl *= gMtrl.fresnel.y;

		// Compute intensity term. Fresnel is always on a top of a multi-layer material
		// therefore it remains strong and attennuates other properties to maintain energy conservation using (1.0 - iFrsl)
		iFrsl = cmax(cFrsl) * fFrsl;
	}
#endif




	// ----------------------------------------------------------------------
	// Compute a specular and diffuse lighting
	// ----------------------------------------------------------------------

	// Compute a specular lobe for base material
	float fLobe = pow(dRS, cSpec.a) * dLNx;


	// ----------------------------------------------------------------------
	// Compute Local Light Sources
	// ----------------------------------------------------------------------

	LocalLightsEx(cDiffLocal, cSpecLocal, nrmW, -frg.camW, cSpec.a, false);


	// ----------------------------------------------------------------------
	// Compute Earth glow
	// ----------------------------------------------------------------------

	float angl = saturate((-dot(gCameraPos, nrmW) - gProxySize) * gInvProxySize);
	cDiffLocal += gAtmColor.rgb * max(0, angl*gGlowConst);

	// Bake material props and lights together
	float3 diffBaked = Light_fx(gMtrl.diffuse.rgb * (dLN * cSun + cDiffLocal) + gMtrl.emissive.rgb + gMtrl.ambient.rgb*gSun.Ambient);

#if LMODE > 0
	cSun = Light_fx(cSun + cSpecLocal);	// Add local light sources
#endif

	// Special alpha only texture in use, set the .rgb to 1.0f
	// Used for panel background lighting in Delta Glider
	cDiff.rgb = saturate(cDiff.rgb + gNoColor.rgb);

	// ------------------------------------------------------------------------
	cDiff.rgb *= diffBaked;				// Lit the texture
	cDiff.a *= gMtrlAlpha;				// Modulate material alpha


	// ------------------------------------------------------------------------
	// Compute total reflected sun light from a material
	//
	float3 cBase = cSpec.rgb * (1.0f - iFrsl) * fLobe;

#if defined(_GLASS)
	cBase += cFrsl.rgb * fFrsl * fFLbe;
#endif

	cSpec.rgb = cSun * saturate(cBase);







	// ----------------------------------------------------------------------
	// Compute a environment reflections
	// ----------------------------------------------------------------------

	float3 cEnv = 0;

#if defined(_ENVMAP)

	if (gEnvMapEnable) {

#if defined(_GLASS)

		if (gFresnel) {

			// Compute LOD level for fresnel reflection
			float fLOD = max(0, (10.0f - log2(gMtrl.fresnel.z)));

			// Always mirror clear reflection for low angles
			fLOD *= (1.0f - fFrsl);

			// Fresnel based environment reflections
			cEnv = (cFrsl * fFrsl) * texCUBElod(EnvMapAS, float4(RflW, fLOD)).rgb;
		}
#endif

		// Compute LOD level for blur effect
		float fLOD = (1.0f - fRghn) * 8.0f;

		// Add a metallic reflections from a base material
		cEnv += cRefl3 * (1.0f-iFrsl) * texCUBElod(EnvMapAS, float4(RflW, fLOD)).rgb;
	}

#endif




	// ----------------------------------------------------------------------
	// Combine all results together
	// ----------------------------------------------------------------------

	// Compute total reflected light
	float fTot = cmax(cEnv + cSpec.rgb);

	// Attennuate diffuse surface beneath
	cDiff.rgb *= (1.0f - fTot);

#if defined(_ENVMAP)
	// Attennuate diffuse surface beneath
	cDiff.rgb *= (1.0f - fRefl);

#if defined(_GLASS)
	// Further attennuate diffuse surface beneath
	cDiff.rgb *= (1.0f - iFrsl*iFrsl);			// note: (1-iFrsl) goes black too quick
#endif
#endif

	// Re-compute output alpha for alpha blending stage
	cDiff.a = saturate(cDiff.a + fTot);

	// Add reflections to output
	cDiff.rgb += cEnv;

	// Add specular to output
	cDiff.rgb += cSpec.rgb;

	// Add emission texture to output, modulate with material
	cDiff.rgb = max(cDiff.rgb, cEmis * gMtrl.emission2.rgb);

#if defined(_DEBUG)
	//if (gDebugHL) cDiff = cDiff*0.5f + gColor;
	cDiff = cDiff * (1 - gColor*0.5f) + gColor;
#endif

	cDiff.rgb *= gSun.Transmission;
	cDiff.rgb += gSun.Inscatter;

	return cDiff;
}







// ============================================================================
// Fast legacy Implementation no additional textures
// ============================================================================


struct FASTData
{
	float4 posH     : POSITION0;
	float3 camW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
#if SHDMAP > 0
	float4 shdH     : TEXCOORD4;
#endif
};


// ============================================================================
// Vertex shader for physics based rendering
//
FASTData FAST_VS(MESH_VERTEX vrt)
{
	// Zero output.
	FASTData outVS = (FASTData)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;

	outVS.nrmW = nrmW;
	outVS.posH = mul(float4(posW, 1.0f), gVP);
	outVS.camW = -posW;
	outVS.tex0 = vrt.tex0.xy;

#if SHDMAP > 0
	outVS.shdH = mul(float4(posW, 1.0f), gLVP);
#endif

	return outVS;
}


// ============================================================================
//
float4 FAST_PS(float4 sc : VPOS, FASTData frg) : COLOR
{

	float3 cEmis;
	float4 cDiff;
	float3 cDiffLocal;
	float3 cSpecLocal;

	// Start fetching texture data -------------------------------------------
	//
	if (gTextured) cDiff = tex2D(WrapS, frg.tex0.xy);
	else		   cDiff = 1;

	if (gOITEnable) if (cDiff.a < 0.5f) clip(-1);

	if (gFullyLit) {
		cDiff.rgb = saturate(cDiff.rgb + gNoColor.rgb);
		cDiff.rgb *= saturate(gMtrl.diffuse.rgb + gMtrl.emissive.rgb);
	}
	else {

		// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
		if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb;
		else		   cEmis = 0;

		float3 nrmW  = normalize(frg.nrmW);
		float4 cSpec = gMtrl.specular.rgba;
		float3 cSun  = saturate(gSun.Color);
		float  dLN   = saturate(-dot(gSun.Dir, nrmW));

		cDiff.rgb = saturate(cDiff.rgb + gNoColor.rgb);

		// ----------------------------------------------------------------------
		// Add vessel self-shadows
		// ----------------------------------------------------------------------

#if SHDMAP > 0
		float fShadow = 1.0f;
		if (!gCockpit) fShadow = smoothstep(0, 0.72, ComputeShadow(frg.shdH, dLN, sc));
		dLN *= fShadow;
#endif

		// ----------------------------------------------------------------------
		// Compute Local Light Sources
		// ----------------------------------------------------------------------

		LocalLightsEx(cDiffLocal, cSpecLocal, nrmW, -frg.camW, cSpec.a, false);


		// ----------------------------------------------------------------------
		// Compute Earth glow
		// ----------------------------------------------------------------------

		float angl = saturate((-dot(gCameraPos, nrmW) - gProxySize) * gInvProxySize);
		cDiffLocal += gAtmColor.rgb * max(0, angl*gGlowConst);

		cDiff.rgb *= saturate( (gMtrl.diffuse.rgb*(dLN * cSun + cDiffLocal)) + (gMtrl.ambient.rgb*gSun.Ambient) + gMtrl.emissive.rgb );

		float3 CamD = normalize(frg.camW);
		float3 HlfW = normalize(CamD - gSun.Dir);
		float  fSun = pow(saturate(dot(HlfW, nrmW)), gMtrl.specular.a);

#if SHDMAP > 0
		fSun *= fShadow;
#endif


		if (dLN == 0) fSun = 0;

#if LMODE > 0
		float3 specLight = saturate((fSun * cSun) + cSpecLocal);
#else
		float3 specLight = (fSun * cSun);
#endif
		cDiff.rgb += (cSpec.rgb * specLight);

		cDiff.rgb += cEmis;
	}

#if defined(_DEBUG)
	//if (gDebugHL) cDiff = cDiff*0.5f + gColor;
	cDiff = cDiff * (1 - gColor*0.5f) + gColor;
#endif

	cDiff.a *= gMtrlAlpha;

	cDiff.rgb *= gSun.Transmission;
	cDiff.rgb += gSun.Inscatter;

	return cDiff;
}



// ========================================================================================================================
//
float4 XRHUD_PS(FASTData frg) : COLOR
{
	return tex2D(WrapS, frg.tex0.xy);
}
