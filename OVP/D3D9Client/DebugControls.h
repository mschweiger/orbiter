// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

#ifndef __DEBUGCONTROLS_H
#define __DEBUGCONTROLS_H

/// \defgroup dbgprm Debug control configuration parameter identifiers
/// Used by DebugControls::GetConfigParam()
/// @{

/// Boolean flag for "Highlight selected mesh" element.
/// \par Parameter type:
///   DWORD
#define CFGPRM_GETSELECTEDMESH			0x2001

/// Boolean flag for "Highlight selected group" element.
/// \par Parameter type:
///   DWORD
#define CFGPRM_GETSELECTEDGROUP			0x2002

/// Bit flag for "debug-flags" elements.
/// For a description of the available bit flags, see \ref dbgflgs
/// \par Parameter type:
///   DWORD
#define CFGPRM_GETDEBUGFLAGS			0x2003

/// Display mode setting
/// \par Parameter type:
///   DWORD
#define CFGPRM_GETDISPLAYMODE			0x2004

/// Camera mode setting
/// \par Parameter type:
///   DWORD
#define CFGPRM_GETCAMERAMODE			0x2005

/// Value of camera speed setting
/// \par Parameter type:
///   double
#define CFGPRM_GETCAMERASPEED			0x2006

/// @}


/// \defgroup dbgflgs Debug control parameter identifiers
/// @{
#define DBG_FLAGS_SELMSHONLY			0x0001	///< Selected mesh only
#define DBG_FLAGS_SELGRPONLY			0x0002	///< Selected group only
#define DBG_FLAGS_BOXES					0x0004	///< Boxes
#define DBG_FLAGS_SPHERES				0x0008	///< Spheres
#define DBG_FLAGS_HLMESH				0x0010	///< Highlight selected mesh
#define DBG_FLAGS_HLGROUP				0x0020	///< Highlight selected group
#define DBG_FLAGS_TILES					0x0040	///< Planet tile debugger
#define DBG_FLAGS_SELVISONLY			0x0080	///< Selected visual only
#define DBG_FLAGS_AMBIENT				0x0100	///< Add Ambient Light for Visual
#define DBG_FLAGS_WIREFRAME				0x0200	///< Enable WireFrame
#define DBG_FLAGS_DUALSIDED				0x0400	///< Dual Sided
#define DBG_FLAGS_PICK					0x1000	///< Enable mesh picking with mouse
#define DBG_FLAGS_FPSLIM				0x2000	///< FPS Limiter enabled
#define DBG_FLAGS_TILEBOXES				0x4000	///< Tile Boxes
/// @}


class vObject;

// ==============================================================

namespace DebugControls {
	class GFXDialog;

	extern  DWORD sMesh;
	extern  DWORD sGroup;
	extern  DWORD debugFlags;
	extern  DWORD dspMode;
	extern  DWORD camMode;
	extern	DWORD sEmitter;
	extern  double camSpeed;
	extern  double resbias;
	extern  std::map<int, const LightEmitter*> Emitters;
	extern  GFXDialog *gfxDlg;
	
	/**
	 * \brief Same functionality than 'official' GetConfigParam, but for
	 * non-provided debug-control config parameters
	 *
	 * This function can be used to access various configuration parameters
	 * defined in the DebugControls core (e.g. debugFlags or camera settings).
	 * \param paramtype Parameter identifier (see \ref dbgprm)
	 * \return Pointer to parameter
	 * \note The pointer must be cast into the appropriate variable type.
	 *   The variable types can be found in the parameter type list (\ref
	 *   cfgprm).
	 * \par Example:
	 * \code
	 * double speed = *(double*)GetConfigParam(CFGPRM_GETCAMERASPEED);
	 * \endcode
	 */
	const void *GetConfigParam (DWORD paramtype);

	void		Create();
	void		Close();
	void		Release();
	void		OpenDlgClbk(void *context);

	void		SetVisual(vObject *vo);
	void		RemoveVisual(vObject *vo);
	vObject *	GetVisual();
	void		SetPickPos(D3DXVECTOR3 pos);

	void		SetupMeshGroups();
	void		UpdateVisual();
	void		UpdateFlags();
	double		GetVisualSize();
	DWORD		GetSelectedMesh();
	void		SelectGroup(DWORD idx);
	void		SelectMesh(D3D9Mesh *pMesh);
	void		SetGroupHighlight(bool bStat);
	int			GetSceneDebug();
	int			GetSelectedEnvMap();
	
	bool		IsActive();
	bool		IsSelectedGroupRendered();

	void		Append(const char *format, ...);
	void		Refresh();

	INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR CALLBACK ViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif // !__DEBUGCONTROLS_H
