// ==============================================================
// Utilities
// Part of the ORBITER VISUALISATION PROJECT (OVP) D3D9 Client
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
//				 2012-2016 �mile "Bibi Uncle" Gr�goire
// ==============================================================

#define STRICT

#include "D3D9util.h"
#include "AABBUtil.h"
#include "D3D9Client.h"
#include "VectorHelpers.h"
#include "D3D9Config.h"
#include "VPlanet.h"
#include "Mesh.h"
#include <functional>
#include <cctype>
#include <unordered_map>
#include <algorithm>

extern D3D9Client* g_client;
extern unordered_map<MESHHANDLE, class SketchMesh*> MeshMap;

DWORD BuildDate()
{
	const char *months[] = { "???","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	char month[8];
	unsigned int day = 0, year = 0;
	assert(sscanf_s(__DATE__, "%s %u %u", month, 8, &day, &year) == 3);
	DWORD m = 0;
	for (DWORD i = 1; i <= 12; i++) if (strncmp(month, months[i], 3) == 0) { m = i; break; }
	assert(m != 0);
	return (year % 100) * 10000 + m * 100 + day;
}

WORD crc16(const char *data, int length)
{
	DWORD crc = 0;
	for (int i = 0; i < length; ++i) {
		crc = crc ^ (DWORD(data[i]) << 8);
		for (int j = 0; j < 8; j++) {
			if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
			else crc = (crc << 1);
			crc &= 0xFFFF;
		}
	}
	return WORD(crc & 0xFFFF);	
}

// ===========================================================================================
// Sun occlusion by planet hObj for a given global position gpos
//
float SunOcclusionByPlanet(OBJHANDLE hObj, VECTOR3 gpos)
{
	VECTOR3 gsun, gpln;
	OBJHANDLE hSun = oapiGetObjectByIndex(0);
	
	oapiGetGlobalPos(hSun, &gsun);
	oapiGetGlobalPos(hObj, &gpln);

	VECTOR3 rpos = gpln - gpos;
	VECTOR3 spos = gsun - gpos;	
	double	sd = length(spos);				
	double  sz = oapiGetSize(hObj);
	VECTOR3 usd = spos / sd;					
	VECTOR3 up = unit(rpos);
	double r  = length(rpos);
	double ca = -dot(up, usd);
	double qr = sqrt(saturate(1.0 - ca * ca)) * r;
	double dp = r * r - sz * sz;
	double hd = dp > 1e4 ? sqrt(dp) : 1000.0; // Distance to horizon
	double sr = oapiGetSize(hSun) * abs(hd) / sd;
	// How much of the sun's "disc" is shadowed by planet (APPROXIMATION)
	double svb = ca > 0.0 ? 1.0 : ilerp(sz - sr * 0.33, sz + sr, qr); 
	return svb;
}

// Check if object 'body' is casting shadows on 'ref' ---------------------
//
bool IsCastingShadows(vObject* body, vObject* ref, double* sunsize_out)
{
	double sz = oapiGetSize(oapiGetGbodyByIndex(0));
	VECTOR3 bc = body->GlobalPos() - ref->GlobalPos();
	double x = dot(bc, ref->SunDirection());			// Distance to projection plane
	double s = abs(x) * sz / ref->SunDistance();		// Size of the sun at projection plane
	double refrad = body->GetSize() + ref->GetSize() + s;
	if (sunsize_out) *sunsize_out = s;

	if (x < 0) return false; // 'body' is behind 'ref'
	if (sqrt(dotp(bc, bc) - x * x) < refrad) return true;
	return false;
}


double Distance(vObject* a, vObject* b)
{
	return length(a->GlobalPos() - b->GlobalPos());
}


float OcclusionFactor(float x, float sunrad, float plnrad)
{
	bool bReverse = sunrad > plnrad;
	return OcclusionFactor(x, sunrad, plnrad, bReverse);
}


// =================================================================================================================================
// Occlusion area of two circles, 1.0f = zero occlusion, 0.0f = full occlusion of smaller circle by bigger one  
// if bReverse then occlusion of bigger by smaller one
//
float OcclusionFactor(float x, float r1, float r2, bool bReverse)
{
	if (x > (r1 + r2)) return 1.0f;

	float rmax = std::max(r1, r2);
	float rmin = std::min(r1, r2);

	float a2 = rmin * rmin;
	float b2 = rmax * rmax;

	if (x < (rmax - rmin)) {
		if (bReverse) return 1.0f - a2 / b2;
		return 0.0f;
	}

	bool bInv = x < sqrt(b2 - a2);

	float s = (r1 + r2 + x) * 0.5f;
	float A = sqrt(s * (s - r1) * (s - r2) * (s - x)); //Heron's area formula
	float h = 2.0f * A / x;

	/*float x2 = x * x;
	float bx = b2 - x2;
	float d = -(a2 * a2) + 2.0f * a2 * (b2 + x2) - bx * bx;
	if (d < 0.0f) d = 0.0f;
	float h = sqrt(d) / (2.0*x);
	*/

	float s1 = asin(saturate(h / rmin)); // Sector 1
	float s2 = asin(saturate(h / rmax)); // Sector 2

	if (bInv) s1 = float(PI) - s1;

	s1 *= a2;
	s2 *= b2;

	float h2 = h * h;
	float t1 = h * sqrt(std::max(0.0f, a2 - h2)); // Triangle 1
	float t2 = h * sqrt(std::max(0.0f, b2 - h2)); // Triangle 2

	if (bInv) t1 = -t1;

	float area = (s1 - t1) + (s2 - t2);


	//LogAlw("x=%f, area=%f, h=%f, s1=%f, t1=%f, s2=%f, t2=%f", x, area, h, s1, t1, s2, t2);
	

	return 1.0f - area / (float(PI) * (bReverse ? b2 : a2));
}


#if _WIN64
#define PTR_FMT_STRING "0x%llX"
#else // 32 bit
#define PTR_FMT_STRING "0x%lX"
#endif

const char *_PTR(const void *p)
{
	static long i = 0; static char buf[8][32];	i++;
	sprintf_s(buf[i & 0x7], 32, PTR_FMT_STRING, LONG_PTR(p));
	return buf[i & 0x7];
}

bool CopyBuffer(LPDIRECT3DRESOURCE9 _pDst, LPDIRECT3DRESOURCE9 _pSrc)
{

	void *pSrcData = NULL;
	void *pDstData = NULL;

	if (_pSrc->GetType()==D3DRTYPE_VERTEXBUFFER && _pDst->GetType()==D3DRTYPE_VERTEXBUFFER) {

		LPDIRECT3DVERTEXBUFFER9 pSrc = (LPDIRECT3DVERTEXBUFFER9)_pSrc;
		LPDIRECT3DVERTEXBUFFER9 pDst = (LPDIRECT3DVERTEXBUFFER9)_pDst;

		D3DVERTEXBUFFER_DESC src_desc, dst_desc;

		HR(pSrc->GetDesc(&src_desc));
		HR(pDst->GetDesc(&dst_desc));

		if (dst_desc.Size<src_desc.Size) return false;

		HR(pSrc->Lock(0, 0, &pSrcData, D3DLOCK_READONLY));
		HR(pDst->Lock(0, 0, &pDstData, 0));

		memcpy(pDstData, pSrcData, src_desc.Size);

		HR(pSrc->Unlock());
		HR(pDst->Unlock());

		return true;
	}

	if (_pSrc->GetType()==D3DRTYPE_INDEXBUFFER && _pDst->GetType()==D3DRTYPE_INDEXBUFFER) {

		LPDIRECT3DINDEXBUFFER9 pSrc = (LPDIRECT3DINDEXBUFFER9)_pSrc;
		LPDIRECT3DINDEXBUFFER9 pDst = (LPDIRECT3DINDEXBUFFER9)_pDst;

		D3DINDEXBUFFER_DESC src_desc, dst_desc;

		HR(pSrc->GetDesc(&src_desc));
		HR(pDst->GetDesc(&dst_desc));

		if (dst_desc.Size<src_desc.Size) return false;
		if (dst_desc.Format!=src_desc.Format) return false;

		HR(pSrc->Lock(0, 0, &pSrcData, D3DLOCK_READONLY));
		HR(pDst->Lock(0, 0, &pDstData, 0));

		memcpy(pDstData, pSrcData, src_desc.Size);

		HR(pSrc->Unlock());
		HR(pDst->Unlock());

		return true;
	}

	return false;
}

inline D3DXVECTOR4 CV2VEC4(const D3DCOLORVALUE &in)
{
	return D3DXVECTOR4(in.r, in.g, in.b, in.a);
}

inline D3DXVECTOR4 CV2VEC4(const D3DCOLORVALUE &in, float w)
{
	return D3DXVECTOR4(in.r, in.g, in.b, w);
}

inline D3DXVECTOR3 CV2VEC3(const D3DCOLORVALUE &in)
{
	return D3DXVECTOR3(in.r, in.g, in.b);
}

inline D3DCOLORVALUE VECtoCV(const D3DXVECTOR3 &in, float w)
{
	D3DCOLORVALUE c = { in.x, in.y, in.z, w };
	return c;
}

inline D3DCOLORVALUE VECtoCV(const D3DXVECTOR4 &in)
{
	D3DCOLORVALUE c = { in.x, in.y, in.z, in.w };
	return c;
}

void UpdateMatExt(const D3DMATERIAL9 *pIn, D3D9MatExt *pOut)
{
	pOut->Ambient = CV2VEC3(pIn->Ambient);
	pOut->Diffuse = CV2VEC4(pIn->Diffuse);
	pOut->Emissive = CV2VEC3(pIn->Emissive);
	pOut->Specular = CV2VEC4(pIn->Specular, pIn->Power);
}

void GetMatExt(const D3D9MatExt *pIn, D3DMATERIAL9 *pOut)
{
	pOut->Ambient = VECtoCV(pIn->Ambient, 0);
	pOut->Diffuse = VECtoCV(pIn->Diffuse);
	pOut->Emissive = VECtoCV(pIn->Emissive, 0);
	pOut->Specular = VECtoCV(pIn->Specular);
	pOut->Specular.a = 0.0f;
	pOut->Power	= pIn->Specular.w;
}

void CreateMatExt(const D3DMATERIAL9 *pIn, D3D9MatExt *pOut)
{
	pOut->Ambient = CV2VEC3(pIn->Ambient);
	pOut->Diffuse = CV2VEC4(pIn->Diffuse);
	pOut->Emissive = CV2VEC3(pIn->Emissive);
	pOut->Specular = CV2VEC4(pIn->Specular, pIn->Power);
	pOut->Reflect = D3DXVECTOR3(0, 0, 0);
	pOut->Fresnel = D3DXVECTOR3(1, 0, 1024.0f);
	pOut->Emission2 = D3DXVECTOR3(1, 1, 1);
	pOut->Roughness = D3DXVECTOR2(1.0f, 1.0f);
	pOut->SpecialFX = D3DXVECTOR4(0, 0, 0, 0);
	pOut->Metalness = 0.0f;
	pOut->ModFlags = 0;
}

void CreateDefaultMat(D3D9MatExt *pOut)
{
	pOut->Ambient = D3DXVECTOR3(0, 0, 0);
	pOut->Diffuse = D3DXVECTOR4(1, 1, 1, 1);
	pOut->Emissive = D3DXVECTOR3(0, 0, 0);
	pOut->Specular = D3DXVECTOR4(0.2f, 0.2f, 0.2f, 50.0f);
	pOut->Reflect = D3DXVECTOR3(0, 0, 0);
	pOut->Fresnel = D3DXVECTOR3(1, 0, 1024.0f);
	pOut->Emission2 = D3DXVECTOR3(1, 1, 1);
	pOut->Roughness = D3DXVECTOR2(1.0f, 1.0f);
	pOut->SpecialFX = D3DXVECTOR4(0, 0, 0, 0);
	pOut->Metalness = 0.0f;
	pOut->ModFlags = 0;
}


void SurfaceLighting(D3D9Sun *light, OBJHANDLE hP, OBJHANDLE hO, float ao)
{
	// hP=hPlanet, hS=hSun
	VECTOR3 GO, GS, GP;

	D3DXVECTOR3 _one(1,1,1);

	OBJHANDLE hS = oapiGetGbodyByIndex(0);	// the central star
	oapiGetGlobalPos (hO, &GO);				// object position
	oapiGetGlobalPos (hS, &GS);				// sun position
	oapiGetGlobalPos (hP, &GP);				// planet position

	VECTOR3 S = GS-GO;							// sun's position from base
	VECTOR3 P = unit(GO-GP);

	float s  = float(length(S));				// sun's distance
	float rs = float(oapiGetSize(hS)) / s;
	float h  = float(dotp(S,P)) / s;			// sun elevation
	float d  = 0.173f;							// sun elevation for dispersion
	float ae = 0.242f;							// sun elevation for ambient
	float aq = 0.342f;

	float amb0 = 0.0f;
	float disp = 0.0f;
	float amb  = 0.0f;

	const ATMCONST *atm = (oapiGetObjectType(hP)==OBJTP_PLANET ? oapiGetPlanetAtmConstants (hP) : NULL);

	if (atm) {
		amb0 = float(min (0.7, log1p(atm->rho0)*0.4));
		disp = float(max (0.02, min(0.9, log1p(atm->rho0))));
	}

	D3DXVECTOR3 lcol;
	D3DXVECTOR3 r0 = _one - D3DXVECTOR3(0.65f, 0.75f, 1.0f) * disp;

	if (atm) { // case 1: planet has atmosphere
		lcol = (r0 + (_one-r0) * saturate(h/d)) * saturate((h+rs)/(2.0f*rs));
		amb  = saturate((h+ae)/aq);
		amb  = saturate(max(amb0*amb-0.05f,ao));
		lcol *= 1.0f-amb*0.5f; // reduce direct light component to avoid overexposure
	}
	else {   // case 2: planet has no atmosphere
		lcol = r0 * saturate((h+rs)/(2.0f*rs));
		amb  = ao;
		lcol *= 1.0f-amb*0.5f; // reduce direct light component to avoid overexposure
	}

	light->Color = D3DXCOLOR(lcol.x, lcol.y, lcol.z, 1.0f);
	light->Ambient = D3DXCOLOR(amb, amb, amb, 1.0f);
	light->Dir = D3DXVEC(S) * (-1.0f/s);
}
// ===========================================
// Remove unecessary spaces and tablations
// when reading a file.
// ===========================================
/*
char* _fgets(char* cbuf, int num, FILE* stream, bool keepOneSpace)
{

	cbuf[0] = '\0';

	char* temp = new char[num];

	if(fgets(temp, num, stream) == NULL) {
		delete []temp;
		temp = NULL;
		return NULL;
	}

	bool equalSign = false;
	bool firstLetter = false;
	int cbufLine = 0;

	for(int i=0; i<num; i++)
	{
		if(temp[i] == ' ' || temp[i] == '\t')
		{
			if(keepOneSpace && temp[max(0, i-1)] != temp[i])
			{
				cbuf[cbufLine] = temp[i];
				cbufLine++;
				continue;
			}

			else if(equalSign == false)
				continue;

			else if(equalSign == true)
			{
				if(!firstLetter)
					continue;

				else
				{
					cbuf[cbufLine] = temp[i];
					cbufLine++;
					continue;
				}
			}
		}

		else if(temp[i] == '\0' || temp[i] == ';' || temp[i] == 0x0A || temp[i] == 0x0D)
			break;

		else
		{
			if(equalSign == true && firstLetter == false)
				firstLetter = true;

			if(temp[i] == '=')
				equalSign = true;

			cbuf[cbufLine] = temp[i];
			cbufLine++;
		}
	}

	cbuf[cbufLine] = '\0';

	while(cbufLine>0) {
		cbufLine--;
		if (cbuf[cbufLine]==' ' || cbuf[cbufLine]=='\t') cbuf[cbufLine] = '\0';
		else break;
	}

	delete[] temp;

	return cbuf;
}
*/

void strremchr(char *str, int idx)
{
	while (str[idx]!='\0') {
		str[idx] = str[idx+1];
		idx++;
	}
}

// --------------------------------------------------------------
// Improved version of fgets
// Copyright (C) 2012 Jarmo Nikkanen
// Return:
// -1 = eof
//  0 = invalid string
//  1 = success without '=' in string
//  2 = success with '=' in string
//
// param:
//  0x01 = Don't Remove spaces from both sides of '='
//  0x02 = Don't convert '/' to '\'
//  0x04 = Convert to upper case
//  0x08 = Remove '=' if exists

int fgets2(char *buf, int cmax, FILE *file, DWORD param)  //bool bEquality, bool bSlash)
{
	bool bEql = false;
	bool bEquality = (param&0x01)==0;
	bool bSlash = (param&0x02)==0;
	bool bEqlRem = (param&0x08)!=0;
	bool bUpper = (param&0x04)!=0;

	if (fgets(buf, cmax, file)==NULL) return -1;

	int num = lstrlen(buf);

	if (num==(cmax-1)) LogErr("Insufficient buffer size in fgets2() size=%d, string=(%s)",cmax,buf);

	// Replace tabs with spaces and cut a comment parts and unwanted chars
	// Check the existance of equality sign '='
	for (int i=0;i<num;i++) {
		char c = buf[i];
		if (c=='=') bEql=true;
		if (c=='=' && bEqlRem) buf[i]=' ';
		if (c=='\t') buf[i]=' ';
		if (c=='/' && bSlash) buf[i]='\\';
		if (c==';' || c==0xA || c==0xD) {
			buf[i]='\0';
			break;
		}
	}

	num = lstrlen(buf);
	if (num==0) return 0;

	// Remove spaces from the end of the line
	while (num>0) {
		num--;
		if (buf[num]==' ') buf[num]='\0';
		else break;
	}

	// Remove spaces from the front of the line
	while (buf[0]==' ') strremchr(buf,0);

	num = lstrlen(buf);
	if (num==0) return 0;

	// Remove repeatitive spaces if exists. (double trible spaces and so on)
	// At this point a space can not be the last char, therefore [i+1] is not a problem
	for (int i=0;i<num;) {
		if (buf[i]==' ' && buf[i+1]==' ') {
			strremchr(buf, i);
			num--;
		}
		else i++;
	}

	num = lstrlen(buf);
	if (num==0) return 0;

	// Remove spaces from both sides of '=' if exists
	if (bEql && bEquality) {
		if (buf[0]=='=' || buf[num-1]=='=') return 0;
		for (int i=0;i<num;i++) if (buf[i]=='=') {
			if (buf[i+1]==' ') strremchr(buf,i+1);
			if (buf[i-1]==' ') strremchr(buf,i-1);
			break;
		}
	}

	if (bUpper) _strupr_s(buf, strlen(buf));

	// Done
	if (bEql) return 2;
	return 1;
}

// -----------------------------------------------------------------------------------
// String helper
// ------------------------------------------------------------------------------------

// trim from start
std::string &ltrim (std::string &s)
{
	//s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](auto c) { return !std::isspace(c); }));
	return s;
}

// trim from end
std::string &rtrim (std::string &s)
{
	//s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	s.erase(std::find_if(s.rbegin(), s.rend(), [](auto c) { return !std::isspace(c); }).base(), s.end());
	return s;
}

// trim from both ends
std::string &trim (std::string &s) {
	return ltrim(rtrim(s));
}

// uppercase complete string
void toUpper (std::string &s) {
	std::transform(s.begin(), s.end(), s.begin(), std::toupper);
}

// lowercase complete string
//void toLower (std::string &s) {
//	std::transform(s.begin(), s.end(), s.begin(), std::tolower);
//}

// string to double (returns quiet_NaN if conversion failed)
double toDoubleOrNaN (const std::string &str)
{
	return (str[0] == 'N' || str[0] == 'n') // "NaN" or "nan"?
		? std::numeric_limits<double>::quiet_NaN()
		: atof(str.c_str());
}

// case insensitive compare
bool startsWith (const std::string &haystack, const std::string &needle)
{
	auto it = std::search(
		haystack.cbegin(), haystack.cend(),
		needle.cbegin(), needle.cend(),
		[](char a, char b) { return std::toupper(a) == std::toupper(b); }
	);
	return it != haystack.cend();
}

// case insensitive conatins
bool contains (const std::string &haystack, const std::string &needle)
{
	auto it = std::search(
		haystack.cbegin(), haystack.cend(), needle.cbegin(), needle.cend(),
		[](char a, char b) { return std::toupper(a) == std::toupper(b); }
	);
	return it != haystack.cend();
}

// case insensitive find
size_t find_ci (const std::string &haystack, const std::string &needle)
{
	auto it = std::search(
		haystack.cbegin(), haystack.cend(), needle.cbegin(), needle.cend(),
		[](char a, char b) { return std::toupper(a) == std::toupper(b); }
	);
	return it != haystack.cend()
		? static_cast<size_t>(it - haystack.cbegin())
		: std::string::npos;
}

// case insensitive rfind
size_t rfind_ci (const std::string &haystack, const std::string &needle)
{
	auto it = std::search(
		haystack.rbegin(), haystack.rend(), needle.rbegin(), needle.rend(),
		[](char a, char b) { return std::toupper(a) == std::toupper(b); }
	);
	return it != haystack.rend()
		? static_cast<size_t>(haystack.rend() - it)
		: std::string::npos;
}

// parse assignments like "foo=bar", "foo = bar" or even "foo= bar ; with comment"
std::pair<std::string, std::string> &splitAssignment (const std::string &line, const char delim /* = '=' */)
{
	static std::pair<std::string, std::string> ret;

	//const char delim = '=';
	const char comment = ';';
	size_t delPos = line.find(delim),  // delimiter position
		cmtPos = line.find(comment);// comment pos...

									// ...convert to 'comment part length' if comment found
	cmtPos -= cmtPos != std::string::npos ? delPos + 1 : 0;

	ret.first = line.substr(0, delPos);
	trim(ret.first);
	ret.second = line.substr(delPos + 1, cmtPos);
	trim(ret.second);

	return ret;
}

// replace all occurances of 's' in 'subj' by 't'
std::string::size_type replace_all (std::string &subj, const std::string &s, const std::string &t)
{
	std::string::size_type n = 0, c = 0;
	while ((n = subj.find(s, n)) != std::string::npos) {
		subj.replace(n, s.size(), t);
		n += t.size();
		++c;
	}
	return c;
}

// =======================================================================
// Some utility methods for D3D vectors and matrices
// ============================================================================

float D3DXVec3Angle(D3DXVECTOR3 a, D3DXVECTOR3 b)
{
	D3DXVec3Normalize(&a,&a);
	D3DXVec3Normalize(&b,&b);
	float x = D3DXVec3Dot(&a,&b);
	if (x<-1.0f) x=-1.0f;
	if (x> 1.0f) x= 1.0f;
	return acos(x);
}

// ============================================================================
//
D3DXVECTOR3 Perpendicular(D3DXVECTOR3 *a)
{
	float x = fabs(a->x);
	float y = fabs(a->y);
	float z = fabs(a->z);
	float m = min(min(x, y), z);
	if (m==x) return D3DXVECTOR3(0, a->z,  a->y);
	if (m==y) return D3DXVECTOR3(a->z, 0, -a->x);
	else      return D3DXVECTOR3(a->y, -a->x, 0);
}

// Cleate a billboarding matrix. X-axis of the vertex data will be pointing to the camera
//
void D3DMAT_CreateX_Billboard(const D3DXVECTOR3 *toCam, const D3DXVECTOR3 *pos, float size, D3DXMATRIX *pOut)
{
	float hz  = 1.0f/sqrt(toCam->x*toCam->x + toCam->z*toCam->z);

	pOut->_11 =  toCam->x;
	pOut->_12 =  toCam->y;
	pOut->_13 =  toCam->z;
	pOut->_31 = -toCam->z*hz;
	pOut->_32 =  0.0f;
	pOut->_33 =  toCam->x*hz;
	pOut->_21 = -pOut->_12*pOut->_33;
	pOut->_22 =  pOut->_33*pOut->_11 - pOut->_13*pOut->_31;
	pOut->_23 =  pOut->_31*pOut->_12;
	pOut->_41 =  pos->x;
	pOut->_42 =  pos->y;
	pOut->_43 =  pos->z;
	pOut->_14 = pOut->_24 = pOut->_34 = pOut->_44 = 0.0f;
	pOut->_11 *= size; pOut->_12 *= size; pOut->_13 *= size;
	pOut->_21 *= size; pOut->_22 *= size; pOut->_23 *= size;
	pOut->_31 *= size;					  pOut->_33 *= size;
}


// Cleate a billboarding matrix. X-axis of the vertex data will be pointing to the camera
//
void D3DMAT_CreateX_Billboard(const D3DXVECTOR3 *toCam, const D3DXVECTOR3 *pos, const D3DXVECTOR3 *dir, float size, float stretch, D3DXMATRIX *pOut)
{
	D3DXVECTOR3 q,w;
	D3DXVec3Normalize(&q, D3DXVec3Cross(&q, dir, toCam));
	D3DXVec3Normalize(&w, D3DXVec3Cross(&w, &q,  dir));

	pOut->_11 = w.x * size;
	pOut->_12 = w.y * size;
	pOut->_13 = w.z * size;

	pOut->_21 = q.x * size;
	pOut->_22 = q.y * size;
	pOut->_23 = q.z * size;

	pOut->_31 = dir->x * stretch;
	pOut->_32 = dir->y * stretch;
	pOut->_33 = dir->z * stretch;

	pOut->_41 = pos->x;
	pOut->_42 = pos->y;
	pOut->_43 = pos->z;

	pOut->_14 = pOut->_24 = pOut->_34 = pOut->_44 = 0.0f;
}

// ============================================================================
//
void D3DMAT_ZeroMatrix(D3DXMATRIX *mat)
{
	ZeroMemory(mat, sizeof (D3DXMATRIX));
}

// ============================================================================
// Matrix identity

void D3DMAT_Identity (D3DXMATRIX *mat)
{
	ZeroMemory(mat, sizeof (D3DXMATRIX));
	mat->_11 = mat->_22 = mat->_33 = mat->_44 = 1.0f;
}

// ============================================================================
// Copy a D3DXMATRIX

void D3DMAT_Copy (D3DXMATRIX *tgt, const D3DXMATRIX *src)
{
	 memcpy(tgt, src, sizeof (D3DXMATRIX));
}

// ============================================================================
//
void D3DMAT_FromAxis(D3DXMATRIX *mat, const D3DVECTOR *x, const D3DVECTOR *y, const D3DVECTOR *z)
{
	mat->_11 = x->x;
	mat->_21 = x->y;
	mat->_31 = x->z;

	mat->_12 = y->x;
	mat->_22 = y->y;
	mat->_32 = y->z;

	mat->_13 = z->x;
	mat->_23 = z->y;
	mat->_33 = z->z;
}

// ============================================================================
//
void D3DMAT_FromAxis(D3DXMATRIX *mat, const VECTOR3 *x, const VECTOR3 *y, const VECTOR3 *z)
{
	mat->_11 = float(x->x);
	mat->_21 = float(x->y);
	mat->_31 = float(x->z);

	mat->_12 = float(y->x);
	mat->_22 = float(y->y);
	mat->_32 = float(y->z);

	mat->_13 = float(z->x);
	mat->_23 = float(z->y);
	mat->_33 = float(z->z);
}

// ============================================================================
//
void D3DMAT_FromAxisT(D3DXMATRIX *mat, const D3DVECTOR *x, const D3DVECTOR *y, const D3DVECTOR *z)
{
	mat->_11 = x->x;
	mat->_12 = x->y;
	mat->_13 = x->z;

	mat->_21 = y->x;
	mat->_22 = y->y;
	mat->_23 = y->z;

	mat->_31 = z->x;
	mat->_32 = z->y;
	mat->_33 = z->z;
}

// ============================================================================
// Copy a rotation matrix into a D3DXMATRIX

void D3DMAT_SetRotation (D3DXMATRIX *mat, const MATRIX3 *rot)
{
	mat->_11 = (FLOAT)rot->m11;
	mat->_12 = (FLOAT)rot->m12;
	mat->_13 = (FLOAT)rot->m13;
	mat->_21 = (FLOAT)rot->m21;
	mat->_22 = (FLOAT)rot->m22;
	mat->_23 = (FLOAT)rot->m23;
	mat->_31 = (FLOAT)rot->m31;
	mat->_32 = (FLOAT)rot->m32;
	mat->_33 = (FLOAT)rot->m33;
}

// ============================================================================
// Copy the transpose of a matrix as rotation of a D3D transformation matrix

void D3DMAT_SetInvRotation (D3DXMATRIX *mat, const MATRIX3 *rot)
{
	mat->_11 = (FLOAT)rot->m11;
	mat->_12 = (FLOAT)rot->m21;
	mat->_13 = (FLOAT)rot->m31;
	mat->_21 = (FLOAT)rot->m12;
	mat->_22 = (FLOAT)rot->m22;
	mat->_23 = (FLOAT)rot->m32;
	mat->_31 = (FLOAT)rot->m13;
	mat->_32 = (FLOAT)rot->m23;
	mat->_33 = (FLOAT)rot->m33;
}

// ============================================================================
// Define a rotation matrix from a rotation axis & rotation angle

void D3DMAT_RotationFromAxis (const D3DXVECTOR3 &axis, float angle, D3DXMATRIX *rot)
{
	// Calculate quaternion
	angle *= 0.5f;
	float w = cosf(angle), sina = sinf(angle);
	float x = sina * axis.x;
	float y = sina * axis.y;
	float z = sina * axis.z;

	// Rotation matrix
	float xx = x*x, yy = y*y, zz = z*z;
	float xy = x*y, xz = x*z, yz = y*z;
	float wx = w*x, wy = w*y, wz = w*z;

	rot->_11 = 1 - 2 * (yy+zz);
	rot->_12 =     2 * (xy+wz);
	rot->_13 =     2 * (xz-wy);
	rot->_21 =     2 * (xy-wz);
	rot->_22 = 1 - 2 * (xx+zz);
	rot->_23 =     2 * (yz+wx);
	rot->_31 =     2 * (xz+wy);
	rot->_32 =     2 * (yz-wx);
	rot->_33 = 1 - 2 * (xx+yy);

	rot->_14 = rot->_24 = rot->_34 = rot->_41 = rot->_42 = rot->_43 = 0.0f;
	rot->_44 = 1.0f;
}

// ============================================================================
// Set up a as matrix for ANTICLOCKWISE rotation r around x/y/z-axis

void D3DMAT_RotX  (D3DXMATRIX *mat, double r)
{
	double sinr = sin(r), cosr = cos(r);
	ZeroMemory (mat, sizeof (D3DXMATRIX));
	mat->_22 = mat->_33 = (FLOAT)cosr;
	mat->_23 = -(mat->_32 = (FLOAT)sinr);
	mat->_11 = mat->_44 = 1.0f;
}

// ============================================================================
//
void D3DMAT_RotY (D3DXMATRIX *mat, double r)
{
	double sinr = sin(r), cosr = cos(r);
	ZeroMemory (mat, sizeof (D3DXMATRIX));
	mat->_11 = mat->_33 = (FLOAT)cosr;
	mat->_31 = -(mat->_13 = (FLOAT)sinr);
	mat->_22 = mat->_44 = 1.0f;
}

// ============================================================================
//
float D3DMAT_BSScaleFactor(const D3DXMATRIX *mat)
{
	float lx = mat->_11*mat->_11 + mat->_12*mat->_12 + mat->_13*mat->_13;
    float ly = mat->_21*mat->_21 + mat->_22*mat->_22 + mat->_23*mat->_23;
    float lz = mat->_31*mat->_31 + mat->_32*mat->_32 + mat->_33*mat->_33;
	return sqrt(max(max(lx,ly),lz));
}

// ============================================================================
// Apply a translation vector toa D3D transformation matrix

void D3DMAT_SetTranslation (D3DXMATRIX *mat, const VECTOR3 *trans)
{
	mat->_41 = (FLOAT)trans->x;
	mat->_42 = (FLOAT)trans->y;
	mat->_43 = (FLOAT)trans->z;
}

void D3DMAT_SetTranslation(D3DXMATRIX *mat, const D3DXVECTOR3 *trans)
{
	mat->_41 = (FLOAT)trans->x;
	mat->_42 = (FLOAT)trans->y;
	mat->_43 = (FLOAT)trans->z;
}

// ============================================================================
//
bool D3DMAT_VectorMatrixMultiply (D3DXVECTOR3 *res, const D3DXVECTOR3 *v, const D3DXMATRIX *mat)
{
    float x = v->x*mat->_11 + v->y*mat->_21 + v->z* mat->_31 + mat->_41;
    float y = v->x*mat->_12 + v->y*mat->_22 + v->z* mat->_32 + mat->_42;
    float z = v->x*mat->_13 + v->y*mat->_23 + v->z* mat->_33 + mat->_43;
    float w = v->x*mat->_14 + v->y*mat->_24 + v->z* mat->_34 + mat->_44;

    if (fabs (w) < 1e-5f) return false;

    res->x = x/w;
    res->y = y/w;
    res->z = z/w;
    return true;
}

// =======================================================================
// Name: D3DMath_MatrixInvert()
// Desc: Does the matrix operation: [Q] = inv[A]. Note: this function only
//       works for matrices with [0 0 0 1] for the 4th column.
// =======================================================================

HRESULT D3DMAT_MatrixInvert (D3DXMATRIX *res, D3DXMATRIX *a)
{
    if( fabs(a->_44 - 1.0f) > .001f)
        return E_INVALIDARG;
    if( fabs(a->_14) > .001f || fabs(a->_24) > .001f || fabs(a->_34) > .001f )
        return E_INVALIDARG;

    FLOAT fDetInv = 1.0f / ( a->_11 * ( a->_22 * a->_33 - a->_23 * a->_32 ) -
                             a->_12 * ( a->_21 * a->_33 - a->_23 * a->_31 ) +
                             a->_13 * ( a->_21 * a->_32 - a->_22 * a->_31 ) );

    res->_11 =  fDetInv * ( a->_22 * a->_33 - a->_23 * a->_32 );
    res->_12 = -fDetInv * ( a->_12 * a->_33 - a->_13 * a->_32 );
    res->_13 =  fDetInv * ( a->_12 * a->_23 - a->_13 * a->_22 );
    res->_14 = 0.0f;

    res->_21 = -fDetInv * ( a->_21 * a->_33 - a->_23 * a->_31 );
    res->_22 =  fDetInv * ( a->_11 * a->_33 - a->_13 * a->_31 );
    res->_23 = -fDetInv * ( a->_11 * a->_23 - a->_13 * a->_21 );
    res->_24 = 0.0f;

    res->_31 =  fDetInv * ( a->_21 * a->_32 - a->_22 * a->_31 );
    res->_32 = -fDetInv * ( a->_11 * a->_32 - a->_12 * a->_31 );
    res->_33 =  fDetInv * ( a->_11 * a->_22 - a->_12 * a->_21 );
    res->_34 = 0.0f;

    res->_41 = -( a->_41 * res->_11 + a->_42 * res->_21 + a->_43 * res->_31 );
    res->_42 = -( a->_41 * res->_12 + a->_42 * res->_22 + a->_43 * res->_32 );
    res->_43 = -( a->_41 * res->_13 + a->_42 * res->_23 + a->_43 * res->_33 );
    res->_44 = 1.0f;

    return S_OK;
}

// ============================================================================
//
LPDIRECT3DPIXELSHADER9 CompilePixelShader(LPDIRECT3DDEVICE9 pDev, const char *file, const char *function, const char *name, const char* options, LPD3DXCONSTANTTABLE *pConst)
{
	ID3DXBuffer* pErrors = NULL;
	ID3DXBuffer* pCode = NULL;
	LPDIRECT3DPIXELSHADER9 pShader = NULL;
	DWORD flags = 0;
	char *str = NULL;
	char *tok = NULL;

	WORD crc = 0;
	if (options) crc = crc16(options, strlen(options));

	string path(file);
	char filename[MAX_PATH];

	string last = path.substr(path.find_last_of("\\/") + 1);
	sprintf_s(filename, MAX_PATH, "Cache/D3D9Client/Shaders/%s_%s_%hX_%s.bin", name, function, crc, last.c_str());

	if (Config->ShaderCacheUse)
	{
		// Browse Shader Cache --------------------
		//
		HANDLE hCacheRead = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hCacheRead != INVALID_HANDLE_VALUE) {
			FILETIME cacheWrite, mainWrite;
			if (GetFileTime(hCacheRead, NULL, NULL, &cacheWrite))
			{
				HANDLE hRead = CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hRead != INVALID_HANDLE_VALUE)
				{
					if (GetFileTime(hRead, NULL, NULL, &mainWrite))
					{
						if (CompareFileTime(&cacheWrite, &mainWrite) == 1)
						{
							DWORD size = GetFileSize(hCacheRead, NULL);
							DWORD* buffer = new DWORD[(size >> 2) + 1];
							DWORD bytesRead;
							if (ReadFile(hCacheRead, buffer, size, &bytesRead, NULL)) {
								HR(pDev->CreatePixelShader(buffer, &pShader));
								HR(D3DXGetShaderConstantTable(buffer, pConst));
							}
							delete[] buffer;
						}
					}
					CloseHandle(hRead);
				}
			}
			CloseHandle(hCacheRead);

			if (pShader) {
				//LogOapi("Shader Created From Cache: %s", filename);
				return pShader;
			}
		}
	}



	// Invalid Cache data, Recompile the shader --------------------
	//
	D3DXMACRO macro[16];
	memset(&macro, 0, 16*sizeof(D3DXMACRO));
	bool bDisassemble = false;

	LogAlw("Compiling a Shader [%s] function [%s] name [%s]...", file, function, name);

	if (options) {
		int m = 0;
		int l = lstrlen(options) + 1;
		str = new char[l];
		strcpy_s(str, l, options);
		tok = strtok(str,";, ");
		while (tok!=NULL && m<16) {
			if (strcmp(tok, "PARTIAL") == 0) flags |= D3DXSHADER_PARTIALPRECISION;
			if (strcmp(tok, "DISASM") == 0) bDisassemble = true;
			else macro[m++].Name = tok;
			tok = strtok(NULL, ";, ");
			LogAlw("Macro (%s)", tok);
		}
	}

	HR(D3DXCompileShaderFromFileA(file, macro, NULL, function, "ps_3_0", flags, &pCode, &pErrors, pConst));

	if (pErrors) {
		LogErr("Compiling a Shader [%s] function [%s] Failed:\n %s", file, function, (char*)pErrors->GetBufferPointer());
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "Failed to compile a shader", 0);
		FatalAppExitA(0, "Failed to compile shader code. Exiting...");
	}

	if (!pCode) {
		LogErr("Failed to compile a shader [%s] [%s]", file, function);
		SAFE_DELETEA(str);
		return NULL;
	}


	// Save Shader into a Cache
	//
	if (Config->ShaderCacheUse)
	{
		HANDLE hCache = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hCache != INVALID_HANDLE_VALUE) {
			DWORD bytesWritten;
			if (!WriteFile(hCache, pCode->GetBufferPointer(), pCode->GetBufferSize(), &bytesWritten, NULL))
			{
				LogErr("CreateShaderCache: WriteFile Error: 0x%X", GetLastError());
			}
			CloseHandle(hCache);
		}
		else {
			LogErr("CreateShaderCache: CreateFile Error: 0x%X", GetLastError());
			LogErr("Path=[%s]", filename);
		}
	}


	if (bDisassemble && pCode) {
		LPD3DXBUFFER pBuffer = NULL;
		if (D3DXDisassembleShader((DWORD*)pCode->GetBufferPointer(), true, NULL, &pBuffer) == S_OK) {
			FILE *fp = NULL;
			char name[256];
			sprintf_s(name, 256, "%s_%s_asm.html", RemovePath(file), function);
			if (!fopen_s(&fp, name, "w")) {
				fwrite(pBuffer->GetBufferPointer(), 1, pBuffer->GetBufferSize(), fp);
				fclose(fp);
			}
			pBuffer->Release();
		}
	}

	HR(pDev->CreatePixelShader((DWORD*)pCode->GetBufferPointer(), &pShader));

	SAFE_RELEASE(pCode);
	SAFE_RELEASE(pErrors);
	SAFE_DELETEA(str);

	return pShader;
}

// ============================================================================
//
LPDIRECT3DVERTEXSHADER9 CompileVertexShader(LPDIRECT3DDEVICE9 pDev, const char *file, const char *function, const char* name, const char *options, LPD3DXCONSTANTTABLE *pConst)
{
	ID3DXBuffer* pErrors = NULL;
	ID3DXBuffer* pCode = NULL;
	LPDIRECT3DVERTEXSHADER9 pShader = NULL;
	DWORD flags = 0;

	WORD crc = 0;
	if (options) crc = crc16(options, strlen(options));

	string path(file);
	char filename[MAX_PATH];

	string last = path.substr(path.find_last_of("\\/") + 1);
	sprintf_s(filename, MAX_PATH, "Cache/D3D9Client/Shaders/%s_%s_%hX_%s.bin", name, function, crc, last.c_str());

	if (Config->ShaderCacheUse)
	{
		// Browse Shader Cache --------------------
		//
		HANDLE hCacheRead = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hCacheRead != INVALID_HANDLE_VALUE) {
			FILETIME cacheWrite, mainWrite;
			if (GetFileTime(hCacheRead, NULL, NULL, &cacheWrite))
			{
				HANDLE hRead = CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hRead != INVALID_HANDLE_VALUE)
				{
					if (GetFileTime(hRead, NULL, NULL, &mainWrite))
					{
						if (CompareFileTime(&cacheWrite, &mainWrite) == 1)
						{
							DWORD size = GetFileSize(hCacheRead, NULL);
							DWORD* buffer = new DWORD[(size >> 2) + 1];
							DWORD bytesRead;
							if (ReadFile(hCacheRead, buffer, size, &bytesRead, NULL)) {
								HR(pDev->CreateVertexShader(buffer, &pShader));
								HR(D3DXGetShaderConstantTable(buffer, pConst));
							}
							delete[] buffer;
						}
					}
					CloseHandle(hRead);
				}
			}
			CloseHandle(hCacheRead);

			if (pShader) {
				//LogOapi("Shader Created From Cache: %s", filename);
				return pShader;
			}
		}
	}

	char *str = NULL;
	char *tok = NULL;

	D3DXMACRO macro[32];
	memset(&macro, 0, 32*sizeof(D3DXMACRO));

	if (options) {
		int m = 0;
		int l = lstrlen(options) + 1;
		str = new char[l];
		strcpy_s(str, l, options);
		tok = strtok(str,";, ");
		while (tok!=NULL && m<16) {
			macro[m++].Name = tok;
			tok = strtok(NULL, ";, ");
		}
	}

	LogAlw("Compiling a Shader [%s] function [%s]...", file, function);

	HR(D3DXCompileShaderFromFileA(file, macro, NULL, function, "vs_3_0", flags, &pCode, &pErrors, pConst));

	if (pErrors) {
		LogErr("Compiling a Shader [%s] function [%s] Failed:\n %s", file, function, (char*)pErrors->GetBufferPointer());
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "Failed to compile a shader", 0);
		FatalAppExitA(0, "Failed to compile shader code. Exiting...");
	}

	if (!pCode) {
		LogErr("Failed to compile a shader [%s] [%s]", file, function);
		SAFE_DELETEA(str);
		return NULL;
	}


	// Save Shader into a Cache
	//
	if (Config->ShaderCacheUse)
	{
		HANDLE hCache = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hCache != INVALID_HANDLE_VALUE) {
			DWORD bytesWritten;
			if (!WriteFile(hCache, pCode->GetBufferPointer(), pCode->GetBufferSize(), &bytesWritten, NULL))
			{
				LogErr("CreateShaderCache: WriteFile Error: 0x%X", GetLastError());
			}
			CloseHandle(hCache);
		}
		else {
			LogErr("CreateShaderCache: CreateFile Error: 0x%X", GetLastError());
			LogErr("Path=[%s]", filename);
		}
	}

	HR(pDev->CreateVertexShader((DWORD*)pCode->GetBufferPointer(), &pShader));

	SAFE_RELEASE(pCode);
	SAFE_RELEASE(pErrors);
	SAFE_DELETEA(str);

	return pShader;
}

// ============================================================================
//
const char *RemovePath(const char *in)
{
	int len = lstrlen(in);
	const char *cptr = in;
	for (int i=0;i<len;i++) if (in[i]=='\\' || in[i]=='/') cptr = &in[i+1];
	return cptr;
}

// ============================================================================
//
bool CreateVolumeTexture(LPDIRECT3DDEVICE9 pDevice, int count, LPDIRECT3DTEXTURE9 *pIn, LPDIRECT3DVOLUMETEXTURE9 *pOut)
{
	if (count==0 || pDevice==NULL || pIn==NULL || pOut==NULL) return false;
	if (pIn[0]==NULL) return false;

	LPDIRECT3DVOLUMETEXTURE9 pTemp = NULL;

	D3DSURFACE_DESC desc;
	D3DVOLUME_DESC vd;
	D3DLOCKED_BOX  box;
	D3DLOCKED_RECT rect;

	pIn[0]->GetLevelDesc(0, &desc);
	DWORD mips = pIn[0]->GetLevelCount();

	if (D3DXCreateVolumeTexture(pDevice, desc.Width, desc.Height, count, mips, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTemp)==S_OK) {

		DWORD height = desc.Height;

		for (DWORD m=0; m < mips; m++) {
			pTemp->GetLevelDesc(m, &vd);
			if (pTemp->LockBox(m, &box, NULL, 0)==S_OK) {
				char *pDst = (char*)box.pBits;
				for (int i=0; i < count; i++) {
					pIn[i]->GetLevelDesc(m, &desc);
					if (pIn[i]->LockRect(m, &rect, NULL, D3DLOCK_READONLY)==S_OK) {
						if ((box.RowPitch == rect.Pitch) && (box.SlicePitch == rect.Pitch*height)) {
							memcpy(pDst, rect.pBits, box.SlicePitch);
							pDst += box.SlicePitch;
							pIn[i]->UnlockRect(m);
							continue;
						}
						LogErr("CreateVolumeTexture: Pitch miss-match");
						pIn[i]->UnlockRect(m);
						pTemp->UnlockBox(m);
						return false;
					}
					else {
						LogErr("CreateVolumeTexture: Failed to lock a rect");
						return false;
					}
				}
			}
			else {
				LogErr("CreateVolumeTexture: Failed to lock a box");
				return false;
			}
			height>>=1;
			pTemp->UnlockBox(m);
		}

		if (D3DXCreateVolumeTexture(pDevice, desc.Width, desc.Height, count, mips, 0, desc.Format, D3DPOOL_DEFAULT, pOut)==S_OK) {
			HR(pDevice->UpdateTexture(pTemp, (*pOut)));
			(*pOut)->GenerateMipSubLevels();
			pTemp->Release();
			return true;
		}
		return false;
	}
	return false;
}

// Light Emitter ============================================================================
//
D3D9Light::D3D9Light(const LightEmitter *le, const class vObject *vo) :
	cosp(0), tanp(0), cosu(0),
	range(0), range2(0),
	intensity(-1.0)
{
	UpdateLight(le, vo);
}

// ============================================================================
//
D3D9Light::D3D9Light() :
	cosp(0), tanp(0), cosu(0),
	range(0), range2(0),
	intensity(-1.0),
	le(NULL), cone(1.0f), GPUId(-1)
{

}

// ============================================================================
//
D3D9Light::~D3D9Light()
{

}

// ============================================================================
//
void D3D9Light::Reset()
{
	intensity = -1.0f;
	cone = 1.0f;
	GPUId = -1;
}

// ============================================================================
//
float D3D9Light::GetIlluminance(D3DXVECTOR3 &_pos, float r) const
{
	if (intensity < 0) return -1.0f;

	D3DXVECTOR3 pos = _pos - Position;

	float d = D3DXVec3Length(&pos);
	float d2 = d*d;

	if (d < r) return 1e6;	// Light is inside the sphere
	if (d > (r + range)) return -1.0f; // Light can't reach the sphere

	if ((Type == 1) && (cosp>0.1)) {
		float x = D3DXVec3Dot(&pos, &Direction);
		if (x < -r) return -1.0f;	// The sphere is a way behind the spotlight
		if ((sqrt(d2 - x*x) - x*tanp) * cosp > r) return -1.0f; // Light cone doesn't intersect the sphere
	}

	// The sphere is lit from outside
	return intensity / (Attenuation.x + Attenuation.y*d + Attenuation.z*d2);
}

// ============================================================================
//
const LightEmitter *D3D9Light::GetEmitter() const
{
	return le;
}

// ============================================================================
//
void D3D9Light::UpdateLight(const LightEmitter *_le, const class vObject *vo)
{
	le = _le;

	// -----------------------------------------------------------------------------

	D3DXVec3TransformCoord(&Position, ptr(D3DXVEC(le->GetPosition())), vo->MWorld());
	Dst2 = D3DXVec3Dot(&Position, &Position);

	// -----------------------------------------------------------------------------

	const double *att = ((PointLight*)le)->GetAttenuation();
	Attenuation = D3DXVECTOR3((float)att[0], (float)att[1], (float)att[2]);

	// -----------------------------------------------------------------------------

	tanp = 0.0f;
	cosu = 1.0f;
	cosp = 1.0f;
	cone = 1.0f;
	float P = 0.0f;
	float U = 0.0f;

	if (le->GetType() == LightEmitter::LT_SPOT) {
		P = float(((SpotLight*)le)->GetPenumbra());
		U = float(((SpotLight*)le)->GetUmbra());
		if (P > 3.05f) P = 3.05f;
		if (U > 2.96f) U = 2.96f;
	}

	// -----------------------------------------------------------------------------
	switch (le->GetType()) {

		case LightEmitter::LT_POINT: {
			Type = 0;
		} break;

		case LightEmitter::LT_SPOT: {
			Type = 1;
			cosp = cos(P * 0.5f);
			cosu = cos(U * 0.5f);
			tanp = tan(P * 0.5f);
			Param[D3D9LFalloff] = 1.0f;
			Param[D3D9LPhi] = cosp;
			Param[D3D9LTheta] = 1.0f / (cosu - cosp);
		} break;

		default:
			LogErr("Invalid Light Emitter Type");
			break;
	}

	// -----------------------------------------------------------------------------
	intensity = float(le->GetIntensity());
	const COLOUR4 &col_d = le->GetDiffuseColour();
	Diffuse.r = (col_d.r*intensity);
	Diffuse.g = (col_d.g*intensity);
	Diffuse.b = (col_d.b*intensity);
	Diffuse.a = (col_d.a*intensity);


	float c = float(att[0]);
	float b = float(att[1]);
	float a = float(att[2]);
	float limit = 0.01f; // Intensity limit for max range
	float Q = intensity - c*limit;
	float d = b*b*limit + 4.0f*a*Q;

	range = (sqrt(limit * d) - b*limit) / (2.0f*a*limit);

	//oapiWriteLogV("LightEmitter[0x%X] R=%f(m), P=%f(deg), U=%f(deg)", this, range, P*DEG, U*DEG);
	range = min(range, float(((PointLight*)le)->GetRange()));

	range2 = range*range;
	Param[D3D9LRange] = range;


	// -----------------------------------------------------------------------------
	if (Type != 0) {
		D3DXVec3TransformNormal(&Direction, ptr(D3DXVEC(le->GetDirection())), vo->MWorld());
		float angle = acos(dot(unit(Position), Direction));
		cone = ilerp(U * 0.5f, P * 0.5f, angle);
	}
}



// Planet Texture Loader ------------------------------------------------------------------------------
//
#include <ddraw.h>
#pragma pack(push, 1)
typedef struct _DDDESC2_x64
{
	DWORD               dwSize;                 // size of the DDSURFACEDESC structure
	DWORD               dwFlags;                // determines what fields are valid
	DWORD               dwHeight;               // height of surface to be created
	DWORD               dwWidth;                // width of input surface
	union
	{
		LONG            lPitch;                 // distance to start of next line (return value only)
		DWORD           dwLinearSize;           // Formless late-allocated optimized surface size
	} DUMMYUNIONNAMEN(1);
	union
	{
		DWORD           dwBackBufferCount;      // number of back buffers requested
		DWORD           dwDepth;                // the depth if this is a volume texture 
	} DUMMYUNIONNAMEN(5);
	union
	{
		DWORD           dwMipMapCount;          // number of mip-map levels requestde
												// dwZBufferBitDepth removed, use ddpfPixelFormat one instead
		DWORD           dwRefreshRate;          // refresh rate (used when display mode is described)
		DWORD           dwSrcVBHandle;          // The source used in VB::Optimize
	} DUMMYUNIONNAMEN(2);
	DWORD               dwAlphaBitDepth;        // depth of alpha buffer requested
	DWORD               dwReserved;             // reserved
	DWORD               lpSurface;              // pointer to the associated surface memory
	union
	{
		DDCOLORKEY      ddckCKDestOverlay;      // color key for destination overlay use
		DWORD           dwEmptyFaceColor;       // Physical color for empty cubemap faces
	} DUMMYUNIONNAMEN(3);
	DDCOLORKEY          ddckCKDestBlt;          // color key for destination blt use
	DDCOLORKEY          ddckCKSrcOverlay;       // color key for source overlay use
	DDCOLORKEY          ddckCKSrcBlt;           // color key for source blt use
	union
	{
		DDPIXELFORMAT   ddpfPixelFormat;        // pixel format description of the surface
		DWORD           dwFVF;                  // vertex format description of vertex buffers
	} DUMMYUNIONNAMEN(4);
	DDSCAPS2            ddsCaps;                // direct draw surface capabilities
	DWORD               dwTextureStage;         // stage in multitexture cascade
} DDSURFACEDESC2_x64;
#pragma pack(pop)

int LoadPlanetTextures(const char* fname, LPDIRECT3DTEXTURE9* ppdds, DWORD flags, int amount)
{
	_TRACE;

	char path[MAX_PATH];

	if (g_client->TexturePath(fname, path)) {

		FILE* f;

		if (fopen_s(&f, path, "rb")) return 0;

		int ntex = 0;
		char* buffer, * location;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		long BytesLeft = size;
		buffer = new char[size + 1];
		rewind(f);
		fread(buffer, 1, size, f);
		fclose(f);

		location = buffer;
		while (ntex < amount && BytesLeft > 0)
		{
			DWORD Magic = *(DWORD*)location;
			if (Magic != MAKEFOURCC('D', 'D', 'S', ' ')) break;

			DDSURFACEDESC2* header = (DDSURFACEDESC2*)(location + sizeof(Magic));

			if ((header->dwFlags & DDSD_LINEARSIZE) == 0 && (header->dwFlags & DDSD_PITCH) == 0) {
				header->dwFlags |= DDSD_LINEARSIZE;
				if (header->ddpfPixelFormat.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5')) header->dwLinearSize = header->dwHeight * header->dwWidth;
				else if (header->ddpfPixelFormat.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3')) header->dwLinearSize = header->dwHeight * header->dwWidth;
				else if (header->ddpfPixelFormat.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1')) header->dwLinearSize = header->dwHeight * header->dwWidth / 2;
				else header->dwLinearSize = header->dwHeight * header->dwWidth * header->ddpfPixelFormat.dwRGBBitCount / 8;
			}

			long bytes = (header->dwFlags & DDSD_LINEARSIZE) ? header->dwLinearSize : (header->dwHeight * header->dwWidth * header->ddpfPixelFormat.dwRGBBitCount / 8);

			bytes += sizeof(Magic) + sizeof(DDSURFACEDESC2_x64);

			D3DXIMAGE_INFO Info;
			LPDIRECT3DTEXTURE9 pTex = NULL;

			if (D3DXCreateTextureFromFileInMemoryEx(g_client->GetDevice(), location, bytes, 0, 0, 1, 0, D3DFMT_FROM_FILE,
				D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, &Info, NULL, &pTex) == S_OK) {
				ppdds[ntex] = pTex;
				//LogAlw("Loaded a texture from %s, 0x%X (%u x %u)", fname, pTex, Info.Width, Info.Height);
			}
			else {
				delete[] buffer;
				LogErr("Failed to surface tile (%d tiles loaded for %s)", ntex, fname);
				return ntex;
			}

			location += bytes;
			BytesLeft -= bytes;
			ntex++;
		}
		delete[] buffer;
		LogOk("Loaded %d textures for %s", ntex, fname);
		return ntex;
	}
	LogWrn("File %s not found", fname);
	return 0;
}






// ======================================================================================
// SketchMesh Interface
// ======================================================================================

SketchMesh* GetSketchMesh(const MESHHANDLE hMesh)
{
	if (MeshMap.find(hMesh) == MeshMap.end())
	{
		SketchMesh* pMesh = new SketchMesh(g_client->GetDevice());

		if (pMesh->LoadMeshFromHandle(hMesh))
		{
			MeshMap[hMesh] = pMesh;
			return pMesh;
		}
		delete pMesh;
		return NULL;
	}
	else return MeshMap[hMesh];
}


SketchMesh::SketchMesh(LPDIRECT3DDEVICE9 _pDev) :
	MaxVert(0), MaxIdx(0),
	nGrp(0), nMtrl(0), nTex(0),
	pDev(_pDev),
	Tex(NULL),
	Grp(NULL),
	Mtrl(NULL),
	pIB(NULL),
	pVB(NULL)
{
}


// ===============================================================================================
//
SketchMesh::~SketchMesh()
{
	SAFE_DELETEA(Mtrl);
	SAFE_DELETEA(Tex);
	SAFE_DELETEA(Grp);
	SAFE_RELEASE(pVB);
	SAFE_RELEASE(pIB);
}


// ===============================================================================================
//
bool SketchMesh::LoadMeshFromHandle(MESHHANDLE hMesh)
{
	pVB = NULL;
	pIB = NULL;
	Mtrl = NULL;
	Tex = NULL;
	Grp = NULL;

	MaxVert = MaxIdx = 0;

	nGrp = oapiMeshGroupCount(hMesh);
	if (nGrp == 0) return false;

	Grp = new SKETCHGRP[nGrp];
	memset(Grp, 0, sizeof(SKETCHGRP) * nGrp);

	// -----------------------------------------------------------------------

	nTex = oapiMeshTextureCount(hMesh) + 1;
	Tex = new SURFHANDLE[nTex];
	Tex[0] = 0; // 'no texture'
	for (DWORD i = 1; i < nTex; i++) Tex[i] = SURFACE(oapiGetTextureHandle(hMesh, i));

	// -----------------------------------------------------------------------

	nMtrl = oapiMeshMaterialCount(hMesh);
	if (nMtrl) Mtrl = new D3DXCOLOR[nMtrl];
	for (DWORD i = 0; i < nMtrl; i++) {
		MATERIAL* pMat = oapiMeshMaterial(hMesh, i);
		if (pMat) {
			Mtrl[i].r = pMat->diffuse.r;
			Mtrl[i].g = pMat->diffuse.g;
			Mtrl[i].b = pMat->diffuse.b;
			Mtrl[i].a = pMat->diffuse.a;
		}
	}

	// -----------------------------------------------------------------------

	for (DWORD i = 0; i < nGrp; i++) {
		MESHGROUPEX* pEx = oapiMeshGroupEx(hMesh, i);
		Grp[i].MtrlIdx = pEx->MtrlIdx;
		Grp[i].TexIdx = pEx->TexIdx;
		Grp[i].nVert = pEx->nVtx;
		Grp[i].nIdx = pEx->nIdx;
		Grp[i].VertOff = MaxVert;
		Grp[i].IdxOff = MaxIdx;
		MaxVert += pEx->nVtx;
		MaxIdx += pEx->nIdx;
	}

	if (MaxVert == 0 || MaxIdx == 0) return false;

	// -----------------------------------------------------------------------

	if (Grp[0].MtrlIdx == SPEC_INHERIT) Grp[0].MtrlIdx = SPEC_DEFAULT;
	if (Grp[0].TexIdx == SPEC_INHERIT) Grp[0].TexIdx = SPEC_DEFAULT;

	for (DWORD i = 0; i < nGrp; i++) {

		if (Grp[i].MtrlIdx == SPEC_INHERIT) Grp[i].MtrlIdx = Grp[i - 1].MtrlIdx;

		if (Grp[i].TexIdx == SPEC_DEFAULT) Grp[i].TexIdx = 0;
		else if (Grp[i].TexIdx == SPEC_INHERIT) Grp[i].TexIdx = Grp[i - 1].TexIdx;
		else Grp[i].TexIdx++;
	}

	// -----------------------------------------------------------------------

	HR(pDev->CreateVertexBuffer(MaxVert * sizeof(NTVERTEX), 0, 0, D3DPOOL_DEFAULT, &pVB, NULL));
	HR(pDev->CreateIndexBuffer(MaxIdx * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIB, NULL));

	NTVERTEX* pVert = NULL;
	WORD* pIndex = NULL;

	for (DWORD i = 0; i < nGrp; i++) {
		MESHGROUPEX* pEx = oapiMeshGroupEx(hMesh, i);
		HR(pIB->Lock(Grp[i].IdxOff * sizeof(WORD), Grp[i].nIdx * sizeof(WORD), (LPVOID*)&pIndex, 0));
		HR(pVB->Lock(Grp[i].VertOff * sizeof(NTVERTEX), Grp[i].nVert * sizeof(NTVERTEX), (LPVOID*)&pVert, 0));
		memcpy(pIndex, pEx->Idx, sizeof(WORD) * pEx->nIdx);
		memcpy(pVert, pEx->Vtx, sizeof(NTVERTEX) * pEx->nVtx);
		HR(pIB->Unlock());
		HR(pVB->Unlock());
	}

	return true;
}


// ===============================================================================================
//
void SketchMesh::Init()
{
	pDev->SetVertexDeclaration(pNTVertexDecl);
	pDev->SetStreamSource(0, pVB, 0, sizeof(NTVERTEX));
	pDev->SetIndices(pIB);
}


// ===============================================================================================
//
void SketchMesh::RenderGroup(DWORD idx)
{
	if (!pVB) return;
	pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, Grp[idx].VertOff, 0, Grp[idx].nVert, Grp[idx].IdxOff, Grp[idx].nIdx / 3);
}


// ===============================================================================================
//
SURFHANDLE SketchMesh::GetTexture(DWORD idx)
{
	assert(idx < nGrp);
	if (Grp[idx].TexIdx) return Tex[Grp[idx].TexIdx];
	return NULL;
}


// ===============================================================================================
//
D3DXCOLOR SketchMesh::GetMaterial(DWORD idx)
{
	assert(idx < nGrp);
	if (Grp[idx].MtrlIdx != SPEC_DEFAULT && Mtrl) return Mtrl[Grp[idx].MtrlIdx];
	return D3DXCOLOR(1, 1, 1, 1);
}





ShaderClass::ShaderClass(LPDIRECT3DDEVICE9 pDev, const char* file, const char* vs, const char* ps, const char *name, const char* options) :
	pPS(), pVS(), pPSCB(NULL), pVSCB(NULL), pDev(pDev), fn(file), psn(ps), vsn(vs), sn(name)
{
	for (int i = 0; i < ARRAYSIZE(pTextures); i++) pTextures[i] = {0};
	pPS = CompilePixelShader(pDev, file, ps, name, options, &pPSCB);
	pVS = CompileVertexShader(pDev, file, vs, name, options, &pVSCB);
}



ShaderClass::~ShaderClass()
{
	SAFE_RELEASE(pPS);
	SAFE_RELEASE(pVS);
	SAFE_RELEASE(pPSCB);
	SAFE_RELEASE(pVSCB);
}


void ShaderClass::ClearTextures()
{
	for (int idx = 0; idx < ARRAYSIZE(pTextures); idx++)
	{
		pTextures[idx].pAssigned = NULL;
		pTextures[idx].pTex = NULL;
		pTextures[idx].bSamplerSet = false;
	}
}

void ShaderClass::DetachTextures()
{
	ClearTextures();
	for (int i = 0; i < 16; i++) HR(pDev->SetTexture(i, NULL));

	HR(pDev->SetTexture(D3DVERTEXTEXTURESAMPLER0, NULL));
	HR(pDev->SetTexture(D3DVERTEXTEXTURESAMPLER1, NULL));
	HR(pDev->SetTexture(D3DVERTEXTEXTURESAMPLER2, NULL));
	HR(pDev->SetTexture(D3DVERTEXTEXTURESAMPLER3, NULL));
}


void ShaderClass::UpdateTextures()
{
	// Set textures and samplers -----------------------------------------------
	//
	for (int idx = 0; idx < ARRAYSIZE(pTextures); idx++)
	{
		int sid = idx > 15 ? idx - 16 + D3DVERTEXTEXTURESAMPLER0 : idx;

		if (pTextures[idx].pTex == NULL) continue;

		// If sampler state is not set, then set it
		//
		if (!pTextures[idx].bSamplerSet)
		{
			pTextures[idx].bSamplerSet = true;
			DWORD flags = pTextures[idx].Flags;

			if (flags & IPF_CLAMP_U)		pDev->SetSamplerState(sid, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			else if (flags & IPF_MIRROR_U)	pDev->SetSamplerState(sid, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
			else							pDev->SetSamplerState(sid, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

			if (flags & IPF_CLAMP_V)		pDev->SetSamplerState(sid, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			else if (flags & IPF_MIRROR_V)	pDev->SetSamplerState(sid, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
			else							pDev->SetSamplerState(sid, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

			if (flags & IPF_CLAMP_W)		pDev->SetSamplerState(sid, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
			else if (flags & IPF_MIRROR_W)	pDev->SetSamplerState(sid, D3DSAMP_ADDRESSW, D3DTADDRESS_MIRROR);
			else							pDev->SetSamplerState(sid, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);

			DWORD filter = D3DTEXF_POINT;

			if (flags & IPF_LINEAR) filter = D3DTEXF_LINEAR;
			if (flags & IPF_PYRAMIDAL) filter = D3DTEXF_PYRAMIDALQUAD;
			if (flags & IPF_GAUSSIAN) filter = D3DTEXF_GAUSSIANQUAD;
			if (flags & IPF_ANISOTROPIC) filter = D3DTEXF_ANISOTROPIC;

			HR(pDev->SetSamplerState(sid, D3DSAMP_SRGBTEXTURE, false));
			HR(pDev->SetSamplerState(sid, D3DSAMP_MAXANISOTROPY, pTextures[idx].AnisoLvl));
			HR(pDev->SetSamplerState(sid, D3DSAMP_MAGFILTER, filter));
			HR(pDev->SetSamplerState(sid, D3DSAMP_MINFILTER, filter));
			if (idx <= 15) { HR(pDev->SetSamplerState(sid, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR)); }
			else { HR(pDev->SetSamplerState(sid, D3DSAMP_MIPFILTER, D3DTEXF_POINT)); }
		}

		// If texture has changed then assign it
		if (pTextures[idx].pTex != pTextures[idx].pAssigned)
		{
			pTextures[idx].pAssigned = pTextures[idx].pTex;
			HR(pDev->SetTexture(sid, pTextures[idx].pTex));
		}
	}
}


void ShaderClass::Setup(LPDIRECT3DVERTEXDECLARATION9 pDecl, bool bZ, int blend)
{
	D3DSURFACE_DESC desc;
	LPDIRECT3DSURFACE9 pTgt = NULL;

	HR(pDev->GetRenderTarget(0, &pTgt));

	if (pTgt) pTgt->GetDesc(&desc);
	else {
		LogErr("ShaderClass::Setup No render target is set");
		return;
	}

	SAFE_RELEASE(pTgt);

	D3DVIEWPORT9 VP;
	VP.X = 0;
	VP.Y = 0;
	VP.Width = desc.Width;
	VP.Height = desc.Height;
	VP.MinZ = 0.0f;
	VP.MaxZ = 1.0f;

	HR(pDev->SetViewport(&VP));

	HR(pDev->SetVertexShader(pVS));
	HR(pDev->SetPixelShader(pPS));

	if (pDecl) HR(pDev->SetVertexDeclaration(pDecl));

	HR(pDev->SetRenderState(D3DRS_POINTSPRITEENABLE, false));
	HR(pDev->SetRenderState(D3DRS_ALPHATESTENABLE, false));
	HR(pDev->SetRenderState(D3DRS_STENCILENABLE, false));
	HR(pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF));

	HR(pDev->SetRenderState(D3DRS_ZENABLE, bZ));
	HR(pDev->SetRenderState(D3DRS_ZWRITEENABLE, bZ));

	HR(pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, (blend != 0)));

	if (blend == 1) {
		HR(pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
		HR(pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
		HR(pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
	}

	if (blend == 2) {
		HR(pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
		HR(pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
		HR(pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
	}

	if (blend == 3) {
		HR(pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MAX));
		HR(pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
		HR(pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
	}

	if (blend == 4) {
		HR(pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
		HR(pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
		HR(pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
	}
}


HANDLE ShaderClass::GetPSHandle(const char* name)
{
	D3DXHANDLE hVar = pPSCB->GetConstantByName(NULL, name);
	return HANDLE(hVar);
}


HANDLE ShaderClass::GetVSHandle(const char* name)
{
	D3DXHANDLE hVar = pVSCB->GetConstantByName(NULL, name);
	return HANDLE(hVar);
}



void ShaderClass::SetTexture(const char* name, LPDIRECT3DBASETEXTURE9 pTex, UINT flags, UINT aniso)
{
	D3DXHANDLE hVar = pPSCB->GetConstantByName(NULL, name);
	SetTexture((HANDLE)hVar, pTex, flags, aniso);
}


void ShaderClass::SetTextureVS(const char* name, LPDIRECT3DBASETEXTURE9 pTex, UINT flags, UINT aniso)
{
	D3DXHANDLE hVar = pVSCB->GetConstantByName(NULL, name);
	SetTextureVS((HANDLE)hVar, pTex, flags, aniso);
}

void ShaderClass::SetPSConstants(const char* name, void* data, UINT bytes)
{
	D3DXHANDLE hVar = pPSCB->GetConstantByName(NULL, name);
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetPSConstants() Invalid variable name [%s]. File[%s], Entrypoint[%s], Shader[%s]", name, fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	if (pPSCB->SetValue(pDev, hVar, data, bytes) != S_OK) {
		LogErr("Shader::SetPSConstants() Failed. Variable[%s], File[%s], Entrypoint[%s]", name, fn.c_str(), psn.c_str());
	}
}

void ShaderClass::SetVSConstants(const char* name, void* data, UINT bytes)
{
	D3DXHANDLE hVar = pVSCB->GetConstantByName(NULL, name);
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetVSConstants() Invalid variable name [%s]. File[%s], Entrypoint[%s], Shader[%s]", name, fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	if (pVSCB->SetValue(pDev, hVar, data, bytes) != S_OK) {
		LogErr("Shader::SetVSConstants() Failed. Variable[%s], File[%s], Entrypoint[%s]", name, fn.c_str(), vsn.c_str());
	}
}
	


void ShaderClass::SetTexture(HANDLE hVar, LPDIRECT3DBASETEXTURE9 pTex, UINT flags, UINT aniso)
{
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetTexture() Invalid handle. File[%s], Entrypoint[%s], Shader[%s]", fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	DWORD idx = pPSCB->GetSamplerIndex(D3DXHANDLE(hVar));

	if (!pTex) {
		pTextures[idx].pTex = NULL;
		return;
	}

	if (pTextures[idx].Flags != flags) pTextures[idx].bSamplerSet = false;
	if (pTextures[idx].AnisoLvl != aniso) pTextures[idx].bSamplerSet = false;

	pTextures[idx].pTex = pTex;
	pTextures[idx].Flags = flags;
	pTextures[idx].AnisoLvl = aniso;
}


void ShaderClass::SetTextureVS(HANDLE hVar, LPDIRECT3DBASETEXTURE9 pTex, UINT flags, UINT aniso)
{
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetTextureVS() Invalid handle. File[%s], Entrypoint[%s], Shader[%s]", fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	DWORD idx = pVSCB->GetSamplerIndex(D3DXHANDLE(hVar)) + 16;
	assert(idx < 20);

	if (!pTex) {
		pTextures[idx].pTex = NULL;
		return;
	}

	if (pTextures[idx].Flags != flags) pTextures[idx].bSamplerSet = false;
	if (pTextures[idx].AnisoLvl != aniso) pTextures[idx].bSamplerSet = false;

	pTextures[idx].pTex = pTex;
	pTextures[idx].Flags = flags | IPF_VERTEXTEX;
	pTextures[idx].AnisoLvl = aniso;
}


void ShaderClass::SetPSConstants(HANDLE hVar, void* data, UINT bytes)
{
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetPSConstants() Invalid handle. File[%s], Entrypoint[%s], Shader[%s]", fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	if (pVSCB->SetValue(pDev, D3DXHANDLE(hVar), data, bytes) != S_OK) {
		LogErr("Shader::SetPSConstants() Failed. File[%s], Entrypoint[%s]", fn.c_str(), vsn.c_str());
	}
}


void ShaderClass::SetVSConstants(HANDLE hVar, void* data, UINT bytes)
{
#ifdef SHDCLSDBG
	if (!hVar) {
		LogErr("Shader::SetVSConstants() Invalid handle. File[%s], Entrypoint[%s], Shader[%s]", fn.c_str(), psn.c_str(), sn.c_str());
		assert(false);
	}
#endif
	if (!hVar) return;
	if (pVSCB->SetValue(pDev, D3DXHANDLE(hVar), data, bytes) != S_OK) {
		LogErr("Shader::SetVSConstants() Failed. File[%s], Entrypoint[%s]", fn.c_str(), vsn.c_str());
	}
}

