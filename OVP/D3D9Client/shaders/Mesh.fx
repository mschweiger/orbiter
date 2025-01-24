// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012-2016 Jarmo Nikkanen
// ==============================================================


struct TileMeshVS
{
	float4 posH     : POSITION0;
	float3 CamW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
	float4 atten    : COLOR0;			// (Atmospheric haze) Attennuate incoming fragment color
	float4 insca    : COLOR1;			// (Atmospheric haze) "Inscatter" Add to incoming fragment color
};

struct MeshVS
{
	float4 posH     : POSITION0;
	float3 CamW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
};

struct TileMeshNMVS
{
	float4 posH     : POSITION0;
	float3 camW     : TEXCOORD0;
	float4 atten    : TEXCOORD1;
	float4 insca    : TEXCOORD2;
	float2 tex0     : TEXCOORD3;
	float3 nrmT     : TEXCOORD4;
	float3 tanT     : TEXCOORD5;

};

MeshVS TinyMeshTechVS(MESH_VERTEX vrt)
{
	// Zero output.
	MeshVS outVS = (MeshVS)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;	// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;	// Apply world transformation matri
	outVS.nrmW  = normalize(nrmW);
	outVS.CamW  = -posW;
	outVS.tex0  = vrt.tex0.xy;

	return outVS;
}


float4 TinyMeshTechPS(MeshVS frg) : COLOR
{
	return float4(0,1,0,1);

	// Normalize input
	float3 nrmW = normalize(frg.nrmW);
	float3 CamW = normalize(frg.CamW);
	float4 cSpec = gMtrl.specular;
	float4 cTex = 1;

	if (gTextured) cTex = tex2D(WrapS, frg.tex0.xy);

	cTex.rgb = saturate(cTex.rgb + gNoColor.rgb);

	if (gFullyLit) return float4(cTex.rgb*saturate(gMtrl.diffuse.rgb + gMtrl.emissive.rgb), cTex.a);

	cTex.a *= gMtrlAlpha;

	// Sunlight calculations. Saturate with cSpec.a to gain an ability to disable specular light
	float  d = saturate(-dot(gSun.Dir, nrmW));
	float  s = pow(saturate(dot(reflect(gSun.Dir, nrmW), CamW)), cSpec.a) * saturate(cSpec.a);

	if (d == 0) s = 0;

	float3 diff = gMtrl.diffuse.rgb * (d * saturate(gSun.Color)); // Compute total diffuse light
	diff += (gMtrl.ambient.rgb*gSun.Ambient) + (gMtrl.emissive.rgb);

	float3 cTot = cSpec.rgb * (s * gSun.Color);	// Compute total specular light

	cTex.rgb *= saturate(diff);	// Lit the diffuse texture

#if defined(_GLASS)
	cTex.a = saturate(cTex.a + max(max(cTot.r, cTot.g), cTot.b));	// Re-compute output alpha for alpha blending stage
#endif

	cTex.rgb += cTot.rgb;											// Apply reflections to output color

	return cTex;
}



// ============================================================================
// Planet Rings Technique
// ============================================================================

float4 RingTechPS(MeshVS frg) : COLOR
{
	float4 color = tex2D(RingS, frg.tex0);

	float3 pp = gCameraPos*gRadius[2] - frg.CamW*gDistScale;

	float  da = dot(normalize(pp), gSun.Dir);
	float  r  = sqrt(dot(pp,pp) * (1.0-da*da));

	float sh  = max(0.05, smoothstep(gRadius[0], gRadius[1], r));

	if (da<0) sh = 1.0f;

	if ((dot(frg.nrmW, frg.CamW)*dot(frg.nrmW, gSun.Dir))>0) return float4(color.rgb*0.35f*sh, color.a);
	return float4(color.rgb*sh, color.a);
}

float4 RingTech2PS(MeshVS frg) : COLOR
{
	float3 pp  = gCameraPos*gRadius[2] - frg.CamW*gDistScale;
	float  dpp = dot(pp,pp);
	float  len = sqrt(dpp);

	len = saturate(smoothstep(gTexOff.x, gTexOff.y, len));

	float4 color = tex2D(RingS, float2(len, 0.5));
	color.a = color.r*0.75;

	float  da = dot(normalize(pp), gSun.Dir);
	float  r  = sqrt(dpp*(1.0-da*da));

	float sh  = max(0.05, smoothstep(gRadius[0], gRadius[1], r));

	if (da<0) sh = 1.0f;

	color.rgb *= sh;

	if ((dot(frg.nrmW, frg.CamW)*dot(frg.nrmW, gSun.Dir))>0) return float4(color.rgb*0.35f, color.a);
	return float4(color.rgb, color.a);
}


// ============================================================================
// Base Tile Rendering Technique
// ============================================================================

TileMeshVS BaseTileVS(NTVERTEX vrt)
{
	// Null the output
	TileMeshVS outVS = (TileMeshVS)0;

	float3 posW  = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH   = mul(float4(posW, 1.0f), gVP);
	outVS.nrmW   = mul(float4(vrt.nrmL, 0.0f), gW).xyz;
	outVS.tex0   = vrt.tex0;
	outVS.CamW   = -posW;

	// Atmospheric haze -------------------------------------------------------

	AtmosphericHaze(outVS.atten, outVS.insca, outVS.posH.z, posW);

	float4 diffuse;
	float ambi, nigh;

	LegacySunColor(diffuse, ambi, nigh, outVS.nrmW);

	outVS.insca *= (diffuse+ambi);
	outVS.insca.a = nigh;

	return outVS;
}


float4 BaseTilePS(TileMeshVS frg) : COLOR
{
	// Normalize input
	float3 nrmW = normalize(frg.nrmW);
	float3 CamW = normalize(frg.CamW);

	float4 cTex = tex2D(ClampS, frg.tex0);

	float3 r = reflect(gSun.Dir, nrmW);
	float  s = pow(saturate(dot(r, CamW)), 20.0f) * (1.0f-cTex.a);
	float  d = saturate(dot(-gSun.Dir, nrmW));

	if (d<=0) s = 0;

	float3 clr = cTex.rgb * saturate(d * gSun.Color + s * gSun.Color + gSun.Ambient);

	if (gNight) clr += tex2D(Tex1S, frg.tex0).rgb;

	return float4(clr.rgb*frg.atten.rgb+frg.insca.rgb, cTex.a);
	//return float4(clr.rgb*frg.atten.rgb+frg.insca.rgb, cTex.a*(1-frg.insca.a));	// Make basetiles transparent during night
}


// ============================================================================
// Vessel Axis vector technique
// ============================================================================

MeshVS AxisTechVS(MESH_VERTEX vrt)
{
	// Zero output.
	MeshVS outVS = (MeshVS)0;
	float  stretch = vrt.tex0.x * gMix;
	float3 posX = vrt.posL + float3(0.0, stretch, 0.0);
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;		// Apply world transformation matrix

	outVS.nrmW  = normalize(nrmW);
	outVS.CamW  = -posW;

	return outVS;
}


float4 AxisTechPS(MeshVS frg) : COLOR
{
	float3 nrmW = normalize(frg.nrmW);
	float  d = saturate(dot(-gSun.Dir, nrmW));
	float3 clr = gColor.rgb * saturate(max(d,0) + 0.5);
	return float4(clr, gColor.a);
}

technique AxisTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 AxisTechVS();
		pixelShader  = compile ps_3_0 AxisTechPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}



// ============================================================================
// Mesh Shadow Technique
// ============================================================================

ShadowTexVS ShadowMeshTechVS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	float alpha = dot(vrt.posL.xyz, gInScatter.xyz) + gInScatter.w;
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	outVS.tex0  = float3(vrt.tex0.xy, alpha);
	outVS.dstW  = outVS.posH.zw;
	return outVS;
}

ShadowTexVS ShadowMeshTechExVS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float alpha = dot(vrt.posL.xyz, gColor.xyz) + gColor.w;
	float3 posX = mul(float4(vrt.posL.xyz, 1.0f), gGrpT).xyz;
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	outVS.tex0  = float3(vrt.tex0.xy, alpha);
	outVS.dstW  = outVS.posH.zw;
	return outVS;
}

float4 ShadowTechPS(ShadowTexVS frg) : COLOR
{
	if (frg.tex0.b < 0) clip(-1);
	if (gOITEnable) {
		float4 alpha = tex2D(WrapS, frg.tex0.xy);
		if (alpha.a < 0.5f) clip(-1);
	}
	return float4(0.0f, 0.0f, 0.0f, gMix);
}


// -----------------------------------------------------------------------------------
// Shadow Map rendering with plain geometry (without texture) 
//
BShadowVS ShadowMapVS(SHADOW_VERTEX vrt)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), gLVP);
	outVS.dstW = outVS.posH.zw;
	return outVS;
}

float4 ShadowMapPS(BShadowVS frg) : COLOR
{
	return 1 - (frg.dstW.x / frg.dstW.y);
}


// -----------------------------------------------------------------------------------
// Shadow Map rendering with texture alpha included
//
ShadowTexVS ShadowMapOIT_VS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), gLVP);
	outVS.tex0 = float3(vrt.tex0.xy, 0);
	outVS.dstW = outVS.posH.zw;
	return outVS;
}

float4 ShadowMapOIT_PS(ShadowTexVS frg) : COLOR
{
	if (gOITEnable) {
		float alpha = tex2D(WrapS, frg.tex0.xy).a;
		if (alpha < 0.5f) return 1.0f;
	}
	return 1 - (frg.dstW.x / frg.dstW.y);
}

// -----------------------------------------------------------------------------------

technique GeometryTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ShadowMapVS();
		pixelShader = compile ps_3_0 ShadowMapPS();

		AlphaBlendEnable = false;
		ZEnable = true;
		ZWriteEnable = true;
		StencilEnable = false;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 ShadowMapOIT_VS();
		pixelShader = compile ps_3_0 ShadowMapOIT_PS();

		AlphaBlendEnable = false;
		ZEnable = true;
		ZWriteEnable = true;
		StencilEnable = false;
	}
}

technique ShadowTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ShadowMeshTechVS();
		pixelShader  = compile ps_3_0 ShadowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilRef    = 1;
		StencilMask   = 1;
		StencilFunc   = NotEqual;
		StencilPass   = Replace;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 ShadowMeshTechExVS();
		pixelShader  = compile ps_3_0 ShadowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilRef    = 1;
		StencilMask   = 1;
		StencilFunc   = NotEqual;
		StencilPass   = Replace;
	}
}



// =============================================================================
// Mesh Bounding Box Technique
// =============================================================================

BShadowVS BoundingBoxVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 pos;
	pos.x = gAttennuate.x * posL.x + gInScatter.x * (1-posL.x);
	pos.y = gAttennuate.y * posL.y + gInScatter.y * (1-posL.y);
	pos.z = gAttennuate.z * posL.z + gInScatter.z * (1-posL.z);

	float3 posX = mul(float4(pos, 1.0f), gGrpT).xyz;		// Apply meshgroup specific transformation
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	return outVS;
}

BShadowVS BoundingSphereVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(posL, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	return outVS;
}

float4 BoundingBoxPS(BShadowVS frg) : COLOR
{
	return gColor;
}

technique TileBoxTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingSphereVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}

technique BoundingBoxTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingBoxVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}

technique BoundingSphereTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingSphereVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}


technique BaseTileTech
{
	/*pass P0
	{
		vertexShader = compile VS_MOD BaseTileNMVS();
		pixelShader  = compile PS_MOD BaseTileNMPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = CCW;
	}*/

	pass P0
	{
		vertexShader = compile vs_3_0 BaseTileVS();
		pixelShader  = compile ps_3_0 BaseTilePS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = CCW;
	}
}

technique RingTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader  = compile ps_3_0 RingTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = false;
		CullMode = NONE;
	}
}

technique RingTech2
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader  = compile ps_3_0 RingTech2PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = false;
		CullMode = NONE;
	}
}

technique SimplifiedTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader = compile ps_3_0 TinyMeshTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = true;
	}
}
