// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// licensed under LGPL v2
// ==============================================================





// ============================================================================
// A Shader for a typical "Metalness" PBR workflow.
// ============================================================================

float4 BakedVC_PS(float4 sc : VPOS, PBRData frg) : COLOR
{
	float3 nrmT;
	float3 nrmW;
	float3 cEmis;
	float4 cDiff;
	float3 cBL;
	float3 cBA;
	float3 cBAO;
	float  fSmth, fMetal;
	
	// ======================================================================
	// Start fetching texture data
	// ======================================================================

	// Fetch a normal map
	//
	if (gCfg.Norm) nrmT = tex2D(Nrm0S, frg.tex0.xy).rgb;

	if (gTextured) cDiff = tex2D(WrapS, frg.tex0.xy);
	else		   cDiff = 1;

	// Fetch Smoothness map (i.e. *_rghn.dds)
	//
	if (gCfg.Rghn) fSmth = tex2D(RghnS, frg.tex0.xy).g;
	else		   fSmth = 1.0f;

	// Fetch Roughness map
	//
	if (gCfg.Metl) fMetal = tex2D(MetlS, frg.tex0.xy).g;
	else		   fMetal = gMtrl.metalness;

	// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
	//
	if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb;
	else		   cEmis = 0;
	
	// Load baked light emitters
	//
	if (gCfg.Baked) cBL = tex2D(BakedLightS, frg.tex0.xy).rgb;
	else			cBL = 0.0f;

	// Load baked ambient lights
	//
	if (gCfg.BakedAmb) cBA = tex2D(BakedSunS, frg.tex0.xy).rgb;
	else			   cBA = 1.0f;

	// Load baked ambient occlusion (note: gCfg.BakedAO ahouldn't be enabled at the same time with gCfg.BakedAmb)
	//
	if (gCfg.BakedAO) cBAO = tex2D(BakedAOS, frg.tex0.xy).rgb;
	else			  cBAO = 1.0f;
	


	// ----------------------------------------------------------------------
	// Now do other calculations while textures are being fetched
	// ----------------------------------------------------------------------

	float3 camW = normalize(frg.camW);
	float3 cSun = gSun.Color;


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
	// Sample reflections and irradiance
	// ======================================================================

#if defined(_ENVMAP)
	if (gEnvMapEnable)
	{
		SampleEnvMap(cEnv, dCN, fRgh, fMetal, rflW, nrmW);

		// Use dynamic ambient light if baked doesn't exists
		if (!gCfg.BakedAmb) {
			float3 cP = Paraboloidal_LVLH(IrradS, nrmW).rgb;
			cBA *= pow(abs(cP), gVCIrrad.w) * cBAO * gVCIrrad.rgb;
		}
	}
#else
#endif

	cBA *= (1.0f - fMetal);		// No ambient for metals


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
	
	
	// Base material color for reflections. Use cDiff for metals and very rough plastics, white for the rest. 
	// cDiff for rough plastics is to avoid washed-out(white) look of black and rough parts.
	float3 cSpec = lerp(cDiff.rgb, float3(1, 1, 1), (1.0f - fMetal) * (1.0f - fRgh3));

	// Fresnel power 2.5 for glossy, 5.0 for rough
	float fFrs = pow(1.0f - dCN, fRgh*2.5 + 2.5f);

	// Fresnel cut-off below X of fSmth
	fFrs *= saturate(0.3f - fRgh*fRgh) * 3.3f;

	// Assume that plastics absorve 50-90% of specular light
	float  fP = lerp(0.1f + (1.0f - fRgh)*0.4f, 1.0f, fMetal);

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

	// Light
	float3 zL = Sq(cSun * fR * dLN) + Sq(cBL) + Sq(cBA) + Sq(gVCAmbient);

	// gVCAmbient is an application and debug controls controllable variable
	float3 zD = cDiff.rgb * fA * LightFXSq(gMtrl.diffuse.rgb * zL + Sq(gMtrl.emissive.rgb));

	// Combine specular terms
	float3 zS = cS * (cSun * dLN);		

	// Combine Diffuse, Specular and Environment
	cDiff.rgb = zD + zS + cE;

	// Override material alpha to make reflections visible
	cDiff.a = saturate(cDiff.a + cmax(zS + cE));

	// Add emission texture to output, modulate with material
	cDiff.rgb = max(cDiff.rgb, cEmis * gMtrl.emission2.rgb);


#if defined(_DEBUG)
	cDiff = cDiff * (1.0f - gColor*0.5f) + gColor;
#endif


	// Is post-processing shader enabled on not ?
	//
#if defined(_LIGHTGLOW)
	return cDiff;
#else
	float3 h2 = cDiff.rgb*cDiff.rgb;
	return float4(cDiff.rgb * pow(max(0, 1.0f + h2*h2), -0.25), cDiff.a);
#endif
}
