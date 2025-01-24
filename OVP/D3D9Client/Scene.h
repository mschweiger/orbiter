// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

// ==============================================================
// Class Scene (interface)
//
// A "Scene" represents the 3-D world as seen from a specific
// viewpoint ("camera"). Each scene therefore has a camera object
// associated with it. The Orbiter core supports a single
// camera, but in principle a graphics client could define
// multiple scenes and render them simultaneously into separate
// windows (or into MFD display surfaces, etc.)
// ==============================================================

#ifndef __SCENE_H
#define __SCENE_H

#include "D3D9Client.h"
#include "CelSphere.h"
#include "VObject.h"
#include <stack>
#include <list>
#include <set>

class vObject;
class vPlanet;
class vVessel;
class D3D9ParticleStream;
class D3D9Text;
class D3D9Pad;

#define GBUF_COLOR				0
#define GBUF_BLUR				1
#define GBUF_TEMP				2
#define GBUF_DEPTH				3
#define GBUF_GDI				4
#define GBUF_COUNT				5	// Buffer count

#define TEX_NOISE				0
#define TEX_CLUT				1
#define TEX_COUNT				2

#define RENDERPASS_UNKNOWN		0x0000
#define RENDERPASS_MAINSCENE	0x0001
#define RENDERPASS_ENVCAM		0x0002
#define RENDERPASS_CUSTOMCAM	0x0003
#define RENDERPASS_SHADOWMAP	0x0004
#define RENDERPASS_PICKSCENE	0x0005
#define RENDERPASS_SKETCHPAD	0x0006
#define RENDERPASS_MAINOVERLAY	0x0007
#define RENDERPASS_NORMAL_DEPTH	0x0008
#define RENDERPASS_VC_SHADOWMAP 0x0009
#define RENDERPASS_STAGESET		0x000A

#define RESTORE ((LPDIRECT3DSURFACE9)(-1))
#define CURRENT ((LPDIRECT3DSURFACE9)(-2))

#define RENDERTURN_ENVCAM		0
#define RENDERTURN_CUSTOMCAM	1
#define RENDERTURN_LAST			1

#define SMAP_MODE_FOCUS			1
#define SMAP_MODE_SCENE			2

#define OBJTP_BUILDING			1000

// Secundary scene render flags
#define SCN_PLANETS		0x1
#define SCN_VESSELS		0x2
#define SCN_EXHAUST		0x4
#define SCN_BEACONS		0x8
#define SCN_PARTICLES	0x10
#define SCN_BASESTRUCT	0x20
#define SCN_ALLEXT		0x3F	///< All exterior features
#define SCN_VC			0x40	///< Virtual cockpit
#define SCN_STAGE		0x1000	///< Render a stage around the world. Cude texture needed.
#define SCN_NOCLEAR		0x2000	///< Do not clear render target

#define CAMERA(x) ((Scene::CAMREC*)x)




class Scene {

	friend class D3D9CelestialSphere;

	// Visual record ===================================================================
	//
	struct VOBJREC {           // linked list of object visuals
		vObject *vobj;         // visual instance
		int	type;
		float apprad;
	};


public:

	FVECTOR3 vPickRay;
	bool bStageSet = false;

	struct FRUSTUM {
		float znear;
		float zfar;
	};

	// Custom camera parameters ========================================================
	//
	struct CAMREC {
		MATRIX3		mRotation;
		VECTOR3		vPosition;
		double		dAperture;
		SURFHANDLE	hSurface;
		OBJHANDLE	hVessel;
		DWORD		dwFlags;
		int			iError;
		bool		bActive;
		__gcRenderProc pRenderProc;
		void*		pUser;
	};

	std::list<ENVCAMREC*> InteriorCams;
	std::list<vObject*> Planets;
	std::list<VOBJREC*> Visuals;
	std::set<vVessel*> RootList;
	std::set<vVessel*> Vessels;
	std::set<vVessel*> eCamRenderList;
	std::set<CAMREC*> CustomCams;
	std::set<CAMREC*>::const_iterator camCurrent;
	std::set<vVessel*>::const_iterator vobjEnv, vobjIP;
	std::list<ENVCAMREC*>::const_iterator itIC;


	// Camera frustum parameters ========================================================
	//
	struct CAMERA {
		float		corner;		// corner to center aperture [rad]
		float		aperture;   // aperture [rad]
		float		aspect;     // aspect ratio
		float		nearplane;  // frustum nearplane distance
		float		farplane;   // frustum farplane distance
		float		apsq;
		float		vh, vw, vhf, vwf;

		VECTOR3		pos;		// Global camera position
		VECTOR3		relpos;		// Relative camera position (Used by Mesh Debugger)
		VECTOR3		dir;		// Camera direction

		D3DXVECTOR3 x;			// Camera axis vector
		D3DXVECTOR3 y;			// Camera axis vector
		D3DXVECTOR3 z;			// Camera axis vector
		D3DXVECTOR3 upos;		// Camera position unit vector

		D3DXMATRIX	mView;		// D3DX view matrix for current camera state
		D3DXMATRIX	mProj;		// D3DX projection matrix for current camera state
		D3DXMATRIX	mProjView;	// D3DX combined projection view matrix
		D3DXMATRIX  mProjViewInf; // D3DX combined projection view matrix, far plane at infinity
		OBJHANDLE	hTarget;	// Current camera target, Mesh Debugger Related

		OBJHANDLE	hObj_proxy;	// closest celestial body
		vPlanet *	vProxy;		// closest celestial body (visual)
		double		alt_proxy;	// camera distance to surface of hObj_proxy

		OBJHANDLE	hNear;		// closest celestial body
		vPlanet *	vNear;		// closest celestial body (visual)

		OBJHANDLE	hGravRef;	// closest celestial body
		vObject*	vGravRef;	// closest celestial body (visual)

		double		alt_near;
		double		lng, lat, elev;
	};

	// Screen space sun visual parameters ==================================================
	//
	struct SUNVISPARAMS {
		float		brightness;
		bool		visible;
		D3DXVECTOR2 position;
		D3DXCOLOR	color;
	};

	SHADOWMAP* smEX = nullptr;	// Exterior shadow map
	SHADOWMAP* smVC = nullptr;	// Virtual Cockpit shadow map
	SHADOWMAP* smSS = nullptr;	// Shadow map for rendering in a stage-set

	static void D3D9TechInit(LPDIRECT3DDEVICE9 pDev, const char *folder);

	/**
	 * \brief Release global parameters
	 */
	static void GlobalExit();

	Scene (oapi::D3D9Client *_gc, DWORD w, DWORD h);
	~Scene ();

	/**
	 * \brief Get a pointer to the client
	 */
	//inline const oapi::D3D9Client *GetClient() const { return gc; }
	inline oapi::D3D9Client *GetClient() const { return gc; }


	void clbkOnOptionChanged(int cat, int item);
	void clbkScenarioChanged(OBJHANDLE hV, ScnChgEvent e);
	void clbkInitialise();

	/**
	 * \brief Update camera position, visuals, etc.
	 */
	void clbkUpdate();

	/**
	 * \brief Render the whole main scene
	 */
	void clbkRenderMainScene();

	/**
	 * \brief Create a visual for a new vessel if within visual range.
	 * \param hVessel vessel object handle
	 */
	void clbkNewVessel(OBJHANDLE hVessel);

	/**
	 * \brief Delete a vessel visual prior to destruction of the logical vessel.
	 * \param hVessel vessel object handle
	 */
	void clbkDeleteVessel(OBJHANDLE hVessel);



	const D3D9Sun *GetSun() const { return &sunLight; }
	const D3D9Light *GetLight(int index) const;
	const D3D9Light *GetLights() const { return Lights; }
	DWORD GetLightCount() const { return nLights; }
	D3D9Pad* GetPooledSketchpad(int id);
	void RecallDefaultState();
	float GetDisplayScale() const { return fDisplayScale; }
	void CreateSunGlare();


	DWORD GetRenderPass() const;
	DWORD GetRenderFlags() const { return RenderFlags; }
	void BeginPass(DWORD dwPass);
	void PopPass();

	const SHADOWMAP* GetSMapData(ShdPackage tp) const;

	inline DWORD GetStencilDepth() const { return stencilDepth; }
	
	/**
	 * \brief Get the ambient background colour
	 */
	inline D3DCOLOR GetBgColour() const { return bg_rgba; }

	/**
	 * \brief Get the viewport dimension (width)
	 */
	inline const DWORD ViewW() const { return viewW; }

	/**
	 * \brief Get the viewport dimension (height)
	 */
	inline const DWORD ViewH() const { return viewH; }

	bool UpdateCamVis();
	

	
	/**
	 * \brief Returns screen space sun visual parameters for Lens Flare rendering.
	*/
	SUNVISPARAMS GetSunScreenVisualState();

	/**
	 * \brief Gets sun diffuse colour (accounting for atmospheric shift)
	 */
	D3DXCOLOR GetSunDiffColor();

	/**
	 * \brief Render a secondary scene. (Env Maps, Shadow Maps, MFD Camera Views)
	 */
	void RenderStageSet(const LPDIRECT3DCUBETEXTURE9 pCT);
	void RenderSecondaryScene(std::set<class vVessel*> &RndList,
		std::set<class vVessel*> &AdditionalLightsList, DWORD flags = SCN_ALLEXT,
		const LPDIRECT3DCUBETEXTURE9 pCT = nullptr, SHADOWMAP* sm = nullptr);

	int RenderShadowMap(SMapInput* smp, SHADOWMAP* out, std::list<vVessel*>& Casters, bool bInternal = false);
	int RenderVCShadowMap(D3DXVECTOR3& cdir, D3DXVECTOR3& ld, std::list<vVessel*>& Casters);
	bool RenderVCProbes(vVessel *vFocus);

	bool IntegrateIrradiance(vVessel *vV, ENVCAMREC* ec, bool bInterior);
	bool RenderBlurredMap(LPDIRECT3DDEVICE9 pDev, LPDIRECT3DCUBETEXTURE9 pSrc);
	bool RenderBlurredMap(LPDIRECT3DDEVICE9 pDev, LPDIRECT3DTEXTURE9 pSrc);
	void RenderMesh(DEVMESHHANDLE hMesh, const oapi::FMATRIX4 *pWorld);
	void RenderStage(LPDIRECT3DCUBETEXTURE9 pCT);

	LPDIRECT3DSURFACE9 GetEnvDepthStencil() const { return pEnvDS; }
	LPDIRECT3DSURFACE9 GetBuffer(int id) const { return psgBuffer[id]; }
	LPDIRECT3DTEXTURE9 GetSunTexture() const { return pSunTex; }
	LPDIRECT3DTEXTURE9 GetSunGlareAtm() const { return pSunGlareAtm; }

	LPDIRECT3DSURFACE9 GetDepthStencilMatch(LPDIRECT3DSURFACE9 pRef);
	LPDIRECT3DSURFACE9 GetDepthStencil(DWORD size);

	/**
	 * \brief Render any shadows cast by vessels on planet surfaces
	 * \param hPlanet handle of planet to cast shadows on
	 * \param depth shadow darkness parameter (0=none, 1=black)
	 * \note Uses stencil buffering if available and requested. Otherwise shadows
	 *   are pure black.
	 * \note Requests for any planet other than that closest to the camera
	 *   are ignored.
	 */
	void RenderVesselShadows(OBJHANDLE hPlanet, float depth) const;

	

	void AddParticleStream (class D3D9ParticleStream *_pstream);
	void DelParticleStream (DWORD idx);

	void AddLocalLight(const LightEmitter *le, const vObject *vo);
	void ClearLocalLights();

	/**
	 * \brief Get object radius in pixels using oapiCameraGlobalPos()
	 * \param hObj object handle
	 */
	double GetObjectAppRad(OBJHANDLE hObj) const;

	/**
	 * \brief Get object radius in pixels using a custom camera location.
	 * \param hObj object handle
	 */
	double GetObjectAppRad2(OBJHANDLE hObj) const;

	// Picking Functions ============================================================================================================
	//
	D3DXVECTOR3		GetPickingRay(short x, short y);
	D3D9Pick		PickScene(short xpos, short ypos, const PickProp* p);
	TILEPICK		PickSurface(short xpos, short ypos);
	D3D9Pick		PickMesh(DEVMESHHANDLE hMesh, const LPD3DXMATRIX pW, short xpos, short ypos);

	void			ClearOmitFlags();
	bool			IsRendering() const { return bRendering; }


	// Custom Camera Interface ======================================================================================================
	//
	CAMERAHANDLE	SetupCustomCamera(CAMERAHANDLE hCamera, OBJHANDLE hVessel, MATRIX3 &mRot, VECTOR3 &pos, double fov, SURFHANDLE hSurf, DWORD flags);
	int				DeleteCustomCamera(CAMERAHANDLE hCamera);
	void			DeleteAllCustomCameras();
	void			CustomCameraOnOff(CAMERAHANDLE hCamera, bool bOn);
	void			RenderCustomCameraView(CAMREC *cCur);


	// Camera Matrix Access =========================================================================================================
	//
	void			   GetAdjProjViewMatrix(LPD3DXMATRIX mP, float znear, float zfar);
	const LPD3DXMATRIX GetProjectionViewMatrix() const { return (LPD3DXMATRIX)&Camera.mProjView; }
	const LPD3DXMATRIX GetProjectionMatrix() const { return (LPD3DXMATRIX)&Camera.mProj; }
	const LPD3DXMATRIX GetViewMatrix() const { return (LPD3DXMATRIX)&Camera.mView; }


	// Main Camera Interface =========================================================================================================
	//
	void			SetCameraAperture(float _ap, float _as);
	void			SetCameraFrustumLimits(double nearlimit, double farlimit);
	float			GetDepthResolution(float dist) const;
	float			CameraInSpace() const;
	void			ResetOrigin(VECTOR3 pos);

					// Acquire camera information from the Orbiter and initialize internal camera setup
	bool			UpdateCameraFromOrbiter(DWORD dwPass);

					// Manually initialize client's internal camera setup
	bool			SetupInternalCamera(D3DXMATRIX *mView, VECTOR3 *pos, double apr, double asp);
	void			CameraOffOrigin90(D3DXMATRIX* mView, FVECTOR3 pos);

					// Pan Camera in a mesh debugger
	bool			CameraPan(VECTOR3 pan, double speed);

					// Check if a sphere located in pCnt (relative to cam) with a specified radius is visible in a camera
	bool			IsVisibleInCamera(const D3DXVECTOR3 *pCnt, float radius);
	bool			IsProxyMesh();
	bool            CameraDirection2Viewport(const VECTOR3 &dir, int &x, int &y);
	double			GetTanAp() const { return tan(Camera.aperture); }
	float			GetCameraAspect() const { return (float)Camera.aspect; }
	float			GetCameraFarPlane() const { return Camera.farplane; }
	float			GetCameraNearPlane() const { return Camera.nearplane; }
	float			GetCameraAperture() const { return (float)Camera.aperture; }
	float			GetCameraApertureCorner() const { return (float)Camera.corner; }
	VECTOR3			GetCameraGPos() const { return Camera.pos; }
	VECTOR3			GetCameraGDir() const { return Camera.dir; }
	OBJHANDLE		GetCameraProxyBody() const { return Camera.hObj_proxy; }
	vPlanet *		GetCameraProxyVisual() const { return Camera.vProxy; }
	double			GetCameraAltitude() const { return Camera.alt_proxy; }
	OBJHANDLE		GetCameraNearBody() const { return Camera.hNear; }
	vPlanet *		GetCameraNearVisual() const { return Camera.vNear; }
	double			GetCameraNearAltitude() const { return Camera.alt_near; }
	double			GetCameraElevation() const { return Camera.elev; }
	void			GetCameraLngLat(double *lng, double *lat) const;
	bool			WorldToScreenSpace(const VECTOR3& rdir, oapi::IVECTOR2* pt, D3DXMATRIX* pVP = NULL, float clip = 1.0f);
	bool			WorldToScreenSpace2(const VECTOR3& rdir, oapi::FVECTOR2* pt, D3DXMATRIX* pVP = NULL, float clip = 1.0f);

	DWORD			GetFrameId() const { return dwFrameId; }

	const D3DXVECTOR3 *GetCameraX() const { return &Camera.x; }
	const D3DXVECTOR3 *GetCameraY() const { return &Camera.y; }
	const D3DXVECTOR3 *GetCameraZ() const { return &Camera.z; }

	const CAMERA *	GetCamera() const { return &Camera; }

	void			PushCamera();	// Push current camera onto a stack
	void			PopCamera();	// Restore a camera from a stack
	FMATRIX4		PushCameraFrustumLimits(float nearlimit, float farlimit);
	FMATRIX4		PopCameraFrustumLimits();
	


	// Visual Management =========================================================================================================
	//
	void			GetLVLH(vVessel *vV, D3DXVECTOR3 *up, D3DXVECTOR3 *nr, D3DXVECTOR3 *cp);
	class vObject *	GetVisObject(OBJHANDLE hObj) const;
	class vVessel *	GetFocusVisual() const { return vFocus; }
	void			CheckVisual(OBJHANDLE hObj);
	double			GetFocusGroundAltitude() const;
	double			GetTargetGroundAltitude() const;
	double			GetTargetElevation() const;
	std::set<vVessel *> GetVessels(double max_dst, bool bActive = true);

	// Locate the visual for hObj in the list if present, or return
	// NULL if not found

protected:

	/**
	 * \brief Render a single marker at a given global position
	 * \param hDC device context
	 * \param gpos global position (ecliptic frame)
	 * \param label1 label above marker
	 * \param label2 label below marker
	 * \param mode marker shape
	 * \param scale marker size
	 */
	void RenderObjectMarker(oapi::Sketchpad *pSkp, const VECTOR3 &gpos, const std::string& label1, const std::string& label2, int mode, int scale);

	void RenderGlares();

private:
	void		ActivateLocalLights(vObject* vO, bool bInterior);
	void		ActivateAllLocalLights(bool bInterior);
	void		ComputeLocalLightsVisibility();
	DWORD		GetActiveParticleEffectCount();
	float		ComputeNearClipPlane();
	void		VisualizeCubeMap(LPDIRECT3DCUBETEXTURE9 pCube, int mip);
	void		VisualizeShadowMap(SHADOWMAP *sm);
	VOBJREC *	FindVisual (OBJHANDLE hObj) const;
	void		RenderVesselMarker(vVessel *vV, D3D9Pad *pSketch);
	float		GetLODLevel(SMapInput* smi);
	void		CombineSMaps(SMapInput* a, SMapInput* b, SMapInput* out);

	LPDIRECT3DTEXTURE9 RenderObjectsInShadow(SMapInput* smi, list<vVessel*>& rList, D3D9Pad *pSkp = nullptr);

	// Locate the visual for hObj in the list if present, or return
	// NULL if not found

	void DelVisualRec (VOBJREC *pv);
	void DeleteAllVisuals();
	// Delete entry pv from the list of visuals

	VOBJREC *AddVisualRec (OBJHANDLE hObj);
	// Add an entry for object hObj in the list of visuals

	VECTOR3 SkyColour ();
	// Sky background colour based on atmospheric parameters of closest planet

	void InitGDIResources();
	void ExitGDIResources();

	void FreePooledSketchpads();      ///< Release pooled Sketchpad instances



	// Scene variables ================================================================
	//

	struct _cascfg {
		float size;
		float dist;
	} cascfg[SHM_CASCADE_COUNT] = {};

	oapi::D3D9Client* gc = {};
	LPDIRECT3DDEVICE9 pDevice = {}; // render device
	DWORD viewW = {};
	DWORD viewH = {};				// render viewport size
	DWORD stencilDepth = {};        // stencil buffer bit depth
	D3D9CelestialSphere* m_celSphere = {}; // celestial sphere background
	DWORD iVCheck = {};             // index of last object checked for visibility
	bool  bLocalLight = {};         // enable local light sources
	bool  surfLabelsActive = {};    // v.2 surface labels activated?

	OBJHANDLE hSun = {};

	D3D9ParticleStream **pstream = {}; // list of particle streams
	DWORD nstream = {};				// number of streams

	DEVMESHHANDLE dmCubeMesh = {};
	D3DCOLOR bg_rgba = {};			// ambient background colour

	// GDI resources ====================================================================
	//
	oapi::Font *label_font[4] = {};

	std::list<vVessel*> Shadowed;
	std::list<vVessel*> RenderList;
	std::list<vVessel*> ObjectsToShadowMap;
	std::list<vVessel*> Casters;
	std::stack<CAMERA>	CameraStack;
	std::stack<DWORD>	PassStack;
	std::stack<FRUSTUM> FrustumStack;


	CAMERA		Camera = {};
	D3D9Light*	Lights = {};
	D3D9Sun	    sunLight = {};

	VECTOR3		origin = {};
	VECTOR3		sky_color = {};
	double      bglvl = {};

	float		fCascadeRatio = {};
	float		fDisplayScale = {};
	float		lmaxdst2 = {};
	DWORD		nLights = {};
	DWORD		dwTurn = {};
	DWORD		dwFrameId = {};
	DWORD		camIndex = {};
	DWORD		RenderFlags = {};
	bool		bRendering = {};

	oapi::Font *pAxisFont = {};
	oapi::Font *pLabelFont = {};
	oapi::Font *pDebugFont = {};

	SurfNative *pLblSrf = {};

	class ImageProcessing* pLightBlur = {};
	class ImageProcessing* pBlur = {};
	class ImageProcessing* pBlur2D = {};
	class ImageProcessing* pGDIOverlay = {};
	class ImageProcessing* pIrradiance = {};
	class ImageProcessing* pVisDepth = {};
	class ImageProcessing* pCreateGlare = {};
	class ImageProcessing* pBakeLights = {};
	class ShaderClass* pLocalCompute = {};
	class ShaderClass* pRenderGlares = {};
	class ShaderClass* pRenderStage = {};

	class vVessel *vFocus = {};
	double dVisualAppRad = {};

	FVECTOR2 DepthSampleKernel[57] = {};

	LPDIRECT3DTEXTURE9 pSunTex = {};
	LPDIRECT3DTEXTURE9 pLightGlare = {};
	LPDIRECT3DTEXTURE9 pSunGlare = {};
	LPDIRECT3DTEXTURE9 pSunGlareAtm = {};
	LPDIRECT3DTEXTURE9 pLocalResults = {};
	LPDIRECT3DSURFACE9 pLocalResultsSL = {};

	// Blur Sampling Kernel ==============================================================
	LPDIRECT3DCUBETEXTURE9 pBlrTemp[5] = {};
	LPDIRECT3DTEXTURE9 pBlrTemp2D[5] = {};
	LPDIRECT3DTEXTURE9 pIrradTemp = {};
	LPDIRECT3DTEXTURE9 ptRandom = {};

	// Deferred Experiment ===============================================================
	//
	LPDIRECT3DSURFACE9 psgBuffer[GBUF_COUNT] = {};
	LPDIRECT3DTEXTURE9 ptgBuffer[GBUF_COUNT] = {};
	LPDIRECT3DSURFACE9 pOffscreenTarget = {};
	LPDIRECT3DTEXTURE9 pTextures[TEX_COUNT] = {};

	LPDIRECT3DSURFACE9 pEnvDS = {};
	LPDIRECT3DSURFACE9 pDepthNormalDS = {};
	LPDIRECT3DSURFACE9 psShmDS[SHM_LOD_COUNT] = {};

	LocalLightsCompute LLCBuf[MAX_SCENE_LIGHTS + 1] = {};

	// Rendering Technique related parameters ============================================
	//
	static ID3DXEffect	*FX;
	static D3DXHANDLE	eLine;
	static D3DXHANDLE	eStar;
	static D3DXHANDLE	eWVP;
	static D3DXHANDLE	eColor;
	static D3DXHANDLE	eTex0;

};

#endif // !__SCENE_H
