// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

// ----------------------------------------------------------------------------
// D3D9Client rendering techniques for Orbiter Spaceflight simulator
// ----------------------------------------------------------------------------


#define NIGHT_CLOUDS 0.05f          // range(0.0f-0.1f) Cloud ambient level at night
#define CLOUD_INTENSITY 1.8f        // range(0.5f-2.0f)
#define NIGHT_LIGHTS 0.7f           // range(0.2f-1.0f)

struct Mat
{
	float4 diffuse;
	float4 ambient;
	float4 specular;
	float4 emissive;
	float  specPower;
};

struct Mtrl
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
	float3 emissive;
	float3 reflect;
	float3 emission2;
	float3 fresnel;
	float2 roughness;
	float  metalness;
	float4 specialfx;			// x = Heat 
};

struct Sun
{
	float3 Dir;
	float3 Color;			// Color and Intensity of received sunlight 
	float3 Ambient;			// Ambient light level (Base Objects Only, Vessels are using dynamic methods)
	float3 Transmission;	// Visibility through atmosphere (1.0 = fully visible, 0.0 = obscured)
	float3 Inscatter;		// Amount of incattered light from haze
};

struct Light
{
	int      type;       	   /* Is is spotlight */
	float    dst2;			   /* Camera-Light Emitter distance squared */
	float4   diffuse;          /* diffuse color of light */
	float3   position;         /* position in world space */
	float3   direction;        /* direction in world space */
	float3   attenuation;      /* Attenuation */
	float4   param;            /* range, falloff, theta, phi */
};

// Must match with counterpart in D3D9Effect.h

struct Flow
{
	bool Emis;		// Enable Emission Maps
	bool Spec;		// Enable Specular Maps
	bool Refl;		// Enable Reflection Maps
	bool Transl;	// Enable translucent effect
	bool Transm;	// Enable transmissive effect
	bool Rghn;		// Enable roughness map
	bool Norm;		// Enable normal map
	bool Metl;		// Enable metalness map
	bool Heat;		// Enable heat map
	bool Baked;		// Enable pre-baked local light map
	bool BakedAO;	// Enable pre-baked AO map
	bool BakedAmb;	// Enable pre-baked Ambient light map
};


#define Range   0
#define Falloff 1
#define Theta   2
#define Phi     3


#define SH_SIZE		0
#define SH_INVSIZE	1

uniform extern float3    kernel[KERNEL_SIZE];

// -------------------------------------------------------------------------
uniform extern float4x4  gW;			    // World matrix
uniform extern float4x4  gLVP;			    // Light view projection
uniform extern float4x4  gVP;			    // Combined View and Projection matrix
uniform extern float4x4  gGrpT;	            // Mesh group transformation matrix
uniform extern float4    gAttennuate;       // (Mesh Constant Fog) Attennuation of fragment color
uniform extern float4    gInScatter;        // (Mesh Constant Fog) In scattering light
uniform extern float4    gColor;            // General purpose color parameter
uniform extern float4    gFogColor;         // Distance fog color in "Legacy" implementation
uniform extern float4    gAtmColor;         // Earth glow color
uniform extern float4    gTexOff;			// Texture offsets used by surface manager
uniform extern float4    gRadius;           // PlanetRad, AtmOuterLimit, CameraRad, CameraAlt
uniform extern float4    gSHD;				// ShadowMap data
uniform extern float4    gSHDPx;			// Shadow resolution [Pixels / meter] for each cascade
uniform extern float4	 gSHDSubRect[3];	// Shadow cascade sub-rects
uniform extern float4	 gVCIrrad;			// Virtual Cockpit ambient lighting control
uniform extern float3    gCameraPos;        // Planet relative camera position, Unit vector
uniform extern float3    gNorth;
uniform extern float3    gEast;
uniform extern float3	 gVCAmbient;		// Ambient level inside virtual cockpit
uniform extern float3	 gNoColor;			// No Color option.
uniform extern Sun		 gSun;				// Sun light direction
uniform extern Mat       gMat;			    // Material input structure  TODO:  Remove all reference to this. Use gMtrl
uniform extern Mat       gWater;			// Water material input structure
uniform extern Mtrl      gMtrl;			    // Material input structure
uniform extern Light	 gLights[MAX_LIGHTS];
uniform extern bool		 gLightsEnabled;
uniform extern bool      gModAlpha;		    // Configuration input
uniform extern bool      gFullyLit;			// Always fully lit bypass lighting calculations
uniform extern bool      gTextured;			// Enable Diffuse Texturing
uniform extern bool      gFresnel;			// Enable fresnel material
uniform extern bool      gPBRSw;			// Legacy / PBR Switch
uniform extern bool      gRghnSw;			// Roughness converter switch
uniform extern bool      gNight;			// Nighttime/Daytime
uniform extern bool      gShadowsEnabled;	// Enable shadow maps
uniform extern bool      gEnvMapEnable;		// Enable Environment mapping
uniform extern bool		 gInSpace;			// True if a mesh is located in space
uniform extern bool		 gBaseBuilding;
uniform extern bool		 gOITEnable;
uniform extern bool		 gCockpit;
uniform extern int       gSpecMode;
uniform extern int       gHazeMode;
uniform extern float     gProxySize;		// Cosine of the angular size of the Proxy Gbody. (one half)
uniform extern float	 gInvProxySize;		// = 1.0 / (1.0f-gProxySize)
uniform extern float     gPointScale;
uniform extern float     gDistScale;
uniform extern float     gFogDensity;
uniform extern float     gTime;
uniform extern float     gMix;				// General purpose parameter (multible uses)
uniform extern float 	 gMtrlAlpha;
uniform extern float	 gGlowConst;
uniform extern float	 gNightTime;		// 1 for nighttime, 0 for daytime
uniform extern Flow		 gCfg;

// Textures -----------------------------------------------------------------

uniform extern texture   gTex0;			    // Diffuse texture
uniform extern texture   gTex1;			    // Nightlights
uniform extern texture   gTex3;				// Normal Map / Cloud Microtexture
uniform extern texture   gSpecMap;			// Specular Map
uniform extern texture   gRghnMap;			// Roughness Map
uniform extern texture   gEmisMap;	    	// Emission Map
uniform extern texture   gEnvMapA;	    	// Environment Map (Mirror clear)
uniform extern texture   gEnvMapB;	    	// Environment Map (Mipmapped with different levels of blur)
uniform extern texture   gReflMap;   		// Reflectivity Map
uniform extern texture   gMetlMap;   		// Metalness Map
uniform extern texture   gHeatMap;   		// Heat Map
uniform extern texture   gTranslMap;		// Translucence Map
uniform extern texture   gTransmMap;		// Transmittance Map
uniform extern texture   gIrradianceMap;    // Irradiance Map
uniform extern texture   gAmbientMap;		// Baked Ambient occlusion map
uniform extern texture   gCombinedMap;		// Combined baked light map
uniform extern texture   gCombinedSunMap;	// Combined baked light map

// Legacy Atmosphere --------------------------------------------------------

uniform extern float     gGlobalAmb;        // Global Ambient Level
uniform extern float     gSunAppRad;        // Sun apparent size (Radius / Distance)
uniform extern float     gDispersion;
uniform extern float     gAmbient0;


// ----------------------------------------------------------------------------
// Vertex layouts
// ----------------------------------------------------------------------------

struct MESH_VERTEX {                        // D3D9Client Mesh vertex layout
	float3 posL   : POSITION0;
	float3 nrmL   : NORMAL0;
	float3 tanL   : TANGENT0;
	float3 tex0   : TEXCOORD0;
};

struct NTVERTEX {                           // Orbiter Mesh vertex layout
	float3 posL     : POSITION0;
	float3 nrmL     : NORMAL0;
	float2 tex0     : TEXCOORD0;
};

struct TILEVERTEX {                         // Vertex declaration used for surface tiles and cloud layer
	float3 posL     : POSITION0;
	float3 normalL  : NORMAL0;
	float2 tex0     : TEXCOORD0;
	float  elev     : TEXCOORD1;
};

struct HZVERTEX {
	float3 posL     : POSITION0;
	float4 color    : COLOR0;
	float2 tex0     : TEXCOORD0;
};

struct POSTEX {
	float3 posL     : POSITION0;
	float2 tex0     : TEXCOORD0;
};

struct SHADOW_VERTEX {
	float4 posL     : POSITION0;
};


// ----------------------------------------------------------------------------
// Vertex shader outputs
// ----------------------------------------------------------------------------

struct SimpleVS
{
	float4 posH     : POSITION0;
	float2 tex0     : TEXCOORD0;
	float3 nrmW     : TEXCOORD1;
	float3 toCamW   : TEXCOORD2;
};

struct HazeVS
{
	float4 posH    : POSITION0;
	float4 color   : TEXCOORD0;
	float2 tex0    : TEXCOORD1;
};

struct BShadowVS
{
	float4 posH    : POSITION0;
	float2 dstW    : TEXCOORD0;
	float  alpha   : TEXCOORD1;
};

struct ShadowTexVS
{
	float4 posH    : POSITION0;
	float2 dstW    : TEXCOORD0;
	float3 tex0	   : TEXCOORD1;
};

// ----------------------------------------------------------------------------
// Texture Sampler implementations
// ----------------------------------------------------------------------------

sampler IrradS = sampler_state      // Irradiance map sampler
{
	Texture = <gIrradianceMap>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler WrapS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler ClampS = sampler_state      // Base tile sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler SpecS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gSpecMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler EmisS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gEmisMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler BakedLightS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gCombinedMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler BakedSunS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gCombinedSunMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler BakedAOS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gAmbientMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};


sampler ReflS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gReflMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler MetlS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gMetlMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler HeatS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gHeatMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler RghnS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gRghnMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler TranslS = sampler_state       // Translucence texture sampler
{
	Texture = <gTranslMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};
sampler TransmS = sampler_state       // Transmittance texture sampler
{
	Texture = <gTransmMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler Tex1S = sampler_state       // Secundary mesh texture sampler (i.e. night texture)
{
	Texture = <gTex1>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler Nrm0S = sampler_state       // Normal Map Sampler
{
	Texture = <gTex3>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler MFDSamp = sampler_state     // Virtual Cockpit MFD screen sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Panel0S = sampler_state     // Sampler for mesh based panels, Panel MFDs. Must be compatible with Non-power of two conditional due to MFD screens.
{
	Texture = <gTex0>;
	MinFilter = POINT;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
};

sampler SimpleS = sampler_state       // Sampler used for SimpleTech. (Star, VC HUD)
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = CLAMP; // Modified for RC29 to fix the line issue in top-right corner
	AddressV = CLAMP;
};

sampler ExhaustS = sampler_state
{
	Texture = <gTex0>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler RingS = sampler_state       // Planetary rings sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler EnvMapAS = sampler_state
{
	Texture = <gEnvMapA>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};

sampler EnvMapBS = sampler_state
{
	Texture = <gEnvMapB>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};


// Planet surface samplers ----------------------------------------------------

sampler Planet0S = sampler_state    // Planet/Cloud diffuse texture sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Planet1S = sampler_state    // Planet nightlights/specular mask sampler
{
	Texture = <gTex1>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Planet3S = sampler_state    // Planet/Cloud micro texture sampler
{
	Texture = <gTex3>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};



// ----------------------------------------------------------------------------
// Atmospheric Haze implementation
//
// att = attennuation, ins = inscatter, depth = pixel depth [0 to 1],
// posW = camera centric world space position of the vertex
// ----------------------------------------------------------------------------

void AtmosphericHaze(out float4 att, out float4 ins, in float depth, in float3 posW)
{
	if (gHazeMode==0) {
		att = 1;
		ins = 0;
		return;
	}
	else if (gHazeMode==1) {
		att = gAttennuate;
		ins = gInScatter;
		return;
	}
	else if (gHazeMode==2) {
		float fogFact = 1.0f / exp(max(0,depth) * gFogDensity);
		att = fogFact;
		ins = half4((1.0f-fogFact) * gFogColor.rgb, 0.0f);
		return;
	}
}


// ----------------------------------------------------------------------------
// Legacy sun color on planet surface. Used for planet surface, base tiles and
// buildings.  See SurfaceLighting() in D3D9Util.cpp
// ----------------------------------------------------------------------------

void LegacySunColor(out float4 diff, out float ambi, out float nigh, in float3 normalW)
{
	float   h = dot(-gSun.Dir, normalW);
	float   s = saturate((h+gSunAppRad)/(2.0f*gSunAppRad));
	float3 r0 = 1.0 - float3(0.65, 0.75, 1.0) * gDispersion;

	if (gDispersion!=0) { // case 1: planet has atmosphere
		float3 di = (r0 + (1.0-r0) * saturate(h*5.780)) * s;
		float  ni = (h+0.242)*2.924;
		float  am = saturate(max(gAmbient0*saturate(ni)-0.05, gGlobalAmb));

		diff = float4(di*(1.0-am*0.5),1);
		ambi = am;
		nigh = saturate(-ni-0.2);
	}
	else { // case 2: planet has no atmosphere
		diff = float4(r0*s, 1);
		ambi = gGlobalAmb;
		nigh = 0;
	}
}



// ----------------------------------------------------------------------------
// Vertex shader implementations
// ----------------------------------------------------------------------------


SimpleVS BasicVS(NTVERTEX vrt)
{
	SimpleVS outVS = (SimpleVS)0;
	float3 posW  = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH   = mul(float4(posW, 1.0f), gVP);
	outVS.nrmW   = mul(float4(vrt.nrmL, 0.0f), gW).xyz;
	outVS.toCamW = -posW;
	outVS.tex0   = vrt.tex0;
	return outVS;
}



// ----------------------------------------------------------------------------
// PixelShader Implementations
// ----------------------------------------------------------------------------

float4 SimpleTechPS(SimpleVS frg) : COLOR
{
	float4 c = tex2D(SimpleS, frg.tex0);
	return float4(c.rgb, c.a * gMix);
}

float4 PanelTechPS(SimpleVS frg) : COLOR
{
	float4 cTex = tex2D(SimpleS, frg.tex0);
	return float4(cTex.rgb, cTex.a*gMix);
}

float4 PanelTechBPS(SimpleVS frg) : COLOR
{
	float4 cTex = tex2D(Panel0S, frg.tex0);
	return float4(cTex.rgb, cTex.a*gMix);
}

float4 ExhaustTechPS(SimpleVS frg) : COLOR
{
	float4 c = tex2D(ExhaustS, frg.tex0);
	return float4(c.rgb, c.a*gMix);
}

float4 SpotTechPS(SimpleVS frg) : COLOR
{
	return (tex2D(SimpleS, frg.tex0) * gColor) * gMix;
}

#include "Particle.fx"
#include "Mesh.fx"
#include "Vessel.fx"
#include "HorizonHaze.fx"
#include "Planet.fx"
#include "BeaconArray.fx"


BShadowVS ArrowTechVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(posL, 1.0f), gW).xyz; // Apply world transformation matrix
	outVS.posH = mul(float4(posW, 1.0f), gVP); // Apply view projection matrix
	return outVS;
}

float4 ArrowTechPS(BShadowVS frg) : COLOR
{
	return gColor;
}


// This is used for rendering grapple points ----------------------------------
//
technique ArrowTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ArrowTechVS();
		pixelShader = compile ps_3_0 ArrowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}


// This is used for many simple renderings ------------------------------------
//
technique SimpleTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 SimpleTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

// This is used for 2DPanel and Glass cockpit ---------------------------------
//
technique PanelTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 PanelTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

technique PanelTechB
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 PanelTechBPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}


// Thil will render exhaust textures ------------------------------------------
//
technique ExhaustTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 ExhaustTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}

// This is used for rendering beacons -----------------------------------------
//
technique SpotTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 SpotTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}
