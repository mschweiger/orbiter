// ==============================================================
// VVessel.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

#ifndef __VVESSEL_H
#define __VVESSEL_H

#define MAX_INTCAM 6

#include "VObject.h"
#include "Mesh.h"
#include "gcCore.h"
#include <unordered_set>
#include <vector>

class oapi::D3D9Client;

typedef struct {
	float fdata;
	VECTOR3 ref, vdata;
	std::vector<VECTOR3> vtx;
} _defstate;



// ==============================================================
// class vVessel (interface)
// ==============================================================

/**
 * \brief Visual representation of a vessel object.
 *
 * The vVessel instance is not persistent: It is created when the
 * object moves into visual range of the camera, and is destroyed
 * when it moves out of visual range.
 *
 * The vVessel contains a set of meshes and animations. At each
 * time step, it synchronises the visual with the logical animation
 * state, and renders the resulting meshes.
 */

class vVessel: public vObject {
public:
	friend class D3D9Client;

	enum Errors { NoVC, ITEMS }; // ITEMS must be last entry

	enum class SMI { VC, Visual, Mesh };

	struct Render {
		static const int VC		= 0x1;	// Camera in VC view
		static const int D2		= 0x2;	// Camera in 2D panel view
		static const int IP		= 0x4;	// Internal pass
	};

	vVessel* vRoot = nullptr;

	/**
	 * \brief Creates a new vessel visual for a scene
	 * \param _hObj vessel object handle
	 * \param scene scene to which the visual is added
	 */
	vVessel (OBJHANDLE _hObj, const Scene *scene);

	~vVessel ();

	/**
	 * \brief Set up global parameters shared by all instances
	 * \param gclient client instance pointer
	 */
	static void GlobalInit (oapi::D3D9Client *gc);

	/**
	 * \brief Release global parameters
	 */
	static void GlobalExit ();

	void clbkEvent(DWORD evnt, DWORD_PTR context);

	D3D9Mesh* GetMesh (UINT idx);
	DWORD GetMeshVisMode(UINT idx); 
	bool GetMinMaxDistance(float *zmin, float *zmax, float *dmin);
	int	 GetMatrixTransform(gcCore::MatrixId matrix_id, DWORD mesh, DWORD group, FMATRIX4 *pMat);
	int  SetMatrixTransform(gcCore::MatrixId matrix_id, DWORD mesh, DWORD group, const FMATRIX4 *pMat);
	bool GetVCPos(D3DXVECTOR3* cpos, D3DXVECTOR3* lpos, float* rad);
	bool GetMeshPosition(int idx, D3DXVECTOR3* cpos, D3DXVECTOR3* lpos, float* rad);
	void BakeLights(ImageProcessing* pBaker);
	void ErrorOnce(Errors e);
	void UpdateBoundingBox();

	// Shadow Map Methods
	bool IsInsideShadows(const SMapInput* shd);
	bool IntersectShadowVolume(const SMapInput* shd);
	bool IntersectShadowTarget(const SMapInput* shd);
	void GetMinMaxLightDist(const SMapInput* shd, float* mind, float* maxd);
	bool GetSMapRenderData(SMI type, int idx, SMapInput* sm);

	void ReloadTextures();
	
	inline DWORD GetMeshCount();

	void PreInitObject();

	VESSEL *GetInterface() { return vessel; }

	/**
	 * \brief Per-frame object parameter updates
	 * \return \e true if update was performed, \e false if skipped.
	 * \action
	 *   - Calls vObject::Update
	 *   - Calls \ref UpdateAnimations
	 */
	bool Update (bool bMainScene);

	/**
	 * \brief Object render call
	 * \param dev render device
	 * \return \e true if render operation was performed (object active),
	 *   \e false if skipped (object inactive)
	 * \action Calls Render(dev,false), i.e. performs the external render pass.
	 * \sa Render(LPDIRECT3DDEVICE9,bool)
	 */
	bool Render (LPDIRECT3DDEVICE9 dev);

	/**
	 * \brief Object render call
	 * \param dev render device
	 * \param internalpass flag for internal render pass
	 * \note This method renders either the external vessel meshes
	 *   (internalpass=false) or internal meshes (internalpass=true), e.g.
	 *   the virtual cockpit.
	 * \note The internal pass is only performed on the focus object, and only
	 *   in cockpit camera mode.
	 * \sa Render(LPDIRECT3DDEVICE9)
	 */
	bool Render (LPDIRECT3DDEVICE9 dev, const SHADOWMAP *sm, DWORD flags);
	bool Render(LPDIRECT3DDEVICE9 dev, bool internalpass, const SHADOWMAP* shd);

	bool RenderExhaust();

	/**
	 * \brief Render the vessel's active light beacons
	 * \param dev render device
	 */
	void RenderLightCone (LPD3DXMATRIX pWT);
	void RenderBeacons (LPDIRECT3DDEVICE9 dev);
	void RenderReentry (LPDIRECT3DDEVICE9 dev);
	void RenderGrapplePoints (LPDIRECT3DDEVICE9 dev);
	void RenderGroundShadow (LPDIRECT3DDEVICE9 dev, OBJHANDLE hPlanet, float depth);
	void RenderVectors (LPDIRECT3DDEVICE9 dev, D3D9Pad *pSkp);
	void RenderClickZones();
	bool RenderENVMap (LPDIRECT3DDEVICE9 pDev, ENVCAMREC* ec, DWORD cnt = 2, DWORD flags = 0xFF);
	bool RenderInteriorENVMap(LPDIRECT3DDEVICE9 pDev, ENVCAMREC* ec, SHADOWMAP* sm);
	bool ProcessEnvMaps(LPDIRECT3DDEVICE9 pDev, DWORD cnt, DWORD flags);

	ENVCAMREC*	GetExteriorEnvMap();
	ENVCAMREC*	CreateEnvCam(EnvCamType ec, int idx = -1);
	ENVCAMREC*	GetEnvCam(EnvCamType ec, int idx = -1);
	bool		GetInteriorCams(std::list<ENVCAMREC*>* pCams);
	bool		HasOwnEnvCam(EnvCamType ec);


	bool IsRoot() const;
	vVessel* GetRoot() const { return vRoot; }
	
	float GetExhaustLength() const { return ExhaustLength; }

	D3D9Pick Pick(const D3DXVECTOR3 *vDir, const PickProp* p);

	bool HasExtPass();
	bool HasShadow();
	bool const Playback() const { return vessel->Playback(); }
	class MatMgr * GetMaterialManager() const { return pMatMgr; }

	/**
	* \brief Update animations of the visual
	*
	* Synchronises the visual animation states with the logical
	* animation states of the vessel object.
	* \param mshidx mesh index
	* \note If mshidx == (UINT)-1 (default), all meshes are updated.
	*/
	void UpdateAnimations(int mshidx = -1);

	void SetVisualProperty(VisualProp prp, int idx, const type_info& t, const void* val);
	bool GetVisualProperty(VisualProp prp, int idx, const type_info& t, void* val);
	

protected:

	void LoadMeshes();
	void InsertMesh(UINT idx);
	void DisposeMeshes();
	void DelMesh(UINT idx);
	void ResetMesh(UINT idx);
	void InitNewAnimation(UINT idx);
	void DisposeAnimations();
	void DelAnimation(UINT idx);
	void ResetAnimations(UINT reset = 1);

	/**
	 * \brief Grow \ref animstate buffer (if needed)
	 *
	 * Increases the size of \ref animstate buffer (if needed).
	 * This method also updates the \ref nanim member.
	 * \param newSize new size of buffer (return value of VESSEL::GetAnimPtr())
	 * \return previous value of \ref nanim
	 */
	void GrowAnimstateBuffer (UINT newSize);

	/**
	 * \brief Modify local lighting due to planet shadow or
	 *   atmospheric dispersion.
	 * \param light pointer to D3D9Light structure receiving modified parameters
	 * \return \e true if lighting modifications should be applied, \e false
	 *   if global lighting conditions apply.
	 */
	bool ModLighting();

	void Animate (UINT an, UINT mshidx);
	void AnimateComponent (ANIMATIONCOMP *comp, const D3DXMATRIX &T);
	void RestoreDefaultState(ANIMATIONCOMP *AC);
	void StoreDefaultState(ANIMATIONCOMP *AC);
	void DeleteDefaultState(ANIMATIONCOMP *AC);


private:

	// Animation database containing 'default' states.
	//
	std::map<MGROUP_TRANSFORM *, _defstate> defstate;
	std::unordered_set<UINT> applyanim;
	std::map<int, double> currentstate;
	ENVCAMREC* InteriorCams[MAX_INTCAM] = { nullptr };

	// Default eCam configurations
	ENVCAMREC ecDefExt;
	FVECTOR3 BakedLightsControl[16];
	FVECTOR3 VCAmbient;
	bool bMustRebake;

	VESSEL *vessel;			// access instance for the vessel
	class MatMgr *pMatMgr;

	struct MESHREC {
		D3D9Mesh *mesh;		// DX9 mesh representation
		D3DXMATRIX *trans;	// mesh transformation matrix (rel. to vessel frame)
		WORD vismode;
	} *meshlist;			// list of associated meshes

	UINT nmesh;				// number of meshes
	UINT vClass;
	ANIMATION *anim;		// list of animations (defined in the vessel object)
	double tCheckLight;		// time for next lighting check
	float ExhaustLength;

	static class SurfNative *defreentrytex, *defexhausttex;
	static class ShaderClass* pRenderZone;
};

#endif // !__VVESSEL_H
