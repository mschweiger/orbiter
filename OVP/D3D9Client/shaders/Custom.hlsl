// ===================================================
// Copyright (C) 2022 Jarmo Nikkanen
// licensed under MIT
// ===================================================


float ilerp(float a, float b, float x)
{
	return saturate((x - a) / (b - a));
}


struct MESH_VERTEX {
	float3 posL   : POSITION0;
	float3 nrmL   : NORMAL0;
	float3 tanL   : TANGENT0;
	float3 tex0   : TEXCOORD0;
};

struct MData {
	float4 posH : POSITION0;
	float3 camW : TEXCOORD0;
};



// ==================================================================================================
// Stage-Set for renderin auxiliary camera views
// ==================================================================================================

sampler tTex;

uniform extern struct {
	float4x4 mVP;
	float4x4 mW;
} cbPS;

MData StageVS(MESH_VERTEX vrt)
{
	MData outVS = (MData)0;
	float3 posW = mul(float4(vrt.posL, 1.0f), cbPS.mW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), cbPS.mVP);
	outVS.camW = -posW;
	return outVS;
}

float4 StagePS(MData frg) : COLOR
{
	float3 cA = texCUBElod(tTex, float4(normalize(-frg.camW), 0.0)).rgb;
	return float4(cA, 1);
}



// ==================================================================================================
// Quadrilateral VC Click Zones
// ==================================================================================================

struct {
	float3 pt[4];
	float4 color;
	float4x4 mW;
	float4x4 mVP;
	bool bSphere;
} cb;
	
MData QuadVS(MESH_VERTEX vrt)
{
	MData outVS = (MData)0;
	float3 pt;
	float3 posL = vrt.posL;

	if (cb.bSphere) {
		pt = cb.pt[0] + posL * cb.pt[1].x;
	}
	else {
		posL = (posL + 1.0f) * 0.5f;
		float3 h0 = normalize(cross(cb.pt[1] - cb.pt[0], cb.pt[2] - cb.pt[0])) * posL.z * 0.02f;
		float3 p0 = lerp(cb.pt[0], cb.pt[1], posL.x);
		float3 p1 = lerp(cb.pt[2], cb.pt[3], posL.x);
		pt = lerp(p0, p1, posL.y) + h0;
	}

	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), cb.mW).xyz;
	float3 posW = mul(float4(pt, 1.0f), cb.mW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), cb.mVP);

	if (cb.bSphere) outVS.camW = 1.0f - abs(dot(normalize(posW), nrmW)) * 0.9f;
	else outVS.camW = posL;

	return outVS;
}

float4 QuadPS(MData frg) : COLOR
{
	if (cb.bSphere) {
		float b = frg.camW.z;
		return float4(cb.color.rgb, cb.color.a * b);
	}
	else {
		float a = 1 - frg.camW.z;
		float b = a > 0.9 ? 0.2 : a;
		return float4(cb.color.rgb, cb.color.a * b);
	}
}
