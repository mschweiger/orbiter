// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// class VirtualCockpit
// Definition for a 3D cockpit
// =======================================================================

#ifndef __VCOCKPIT_H
#define __VCOCKPIT_H

// =======================================================================
// class VirtualCockpit

class VirtualCockpit {
	friend class Pane;

public:
	VirtualCockpit (int _id, const Pane *_pane);
	~VirtualCockpit ();

	inline int GetId (void) const { return id; }
	// VC id (>= 0)

	void SetConnections (int left, int right, int top, int bottom);
	// define panel neighbours in four directions. -1=none

	inline int Connect (int dir) const { return connect[dir]; }
	// return connected panel id for specified direction

	void Shift (const Vector &shift);
	// shift the HUD position and active areas by vector 'shift'

	void DefineArea (int aid, const RECT &texrect, int draw_mode, int mouse_mode, int bkmode, SURFHANDLE tgt);
	// create a new active area

	int AreaIndex (int aid);
	// return list index (>= 0) of area 'aid', or -1 if not found

	SURFHANDLE GetArea (int idx);
	bool BltAreaBackground (int idx, SURFHANDLE s);
	void SetArea (int idx, SURFHANDLE s);
	void RedrawArea (int idx, int event);
	void RedrawAllAreas (int event);
	void ReleaseAreas ();
	void ShiftAreas (const Vector &shift);

	SURFHANDLE CreateHUDSurface (const VCHUDSPEC *spec, COLORREF col = 0x00ff00, double intens = 1.0);
	// create a surface for the VC HUD (returns surface handle)

	void DestroyHUDSurface ();
	// release surface for VC HUD

	void ClearHUD ();
	void SetHUDCol (COLORREF col = 0, double intens = 0.0);
	void ShiftHUDPos (const Vector &shift);

	inline const SURFHANDLE GetHUDSurf () const { return hud.surf; }
	inline const VCHUDSPEC *GetHUDParams () const { return &hud.spec; /*return (hud.surf ? &hud.spec : NULL);*/ }

	void RedrawHUD ();

	bool SetClickZone_Spherical (int i, const Vector &cnt, double rad);
	bool SetClickZone_Quadrilateral (int i, const Vector &p1, const Vector &p2, const Vector &p3, const Vector &p4);
	void GetClickZones(std::list<VCClickZone>* p_List);

	bool ProcessMouse (UINT event, DWORD state, int x, int y);
	void GetMouseState (int &idx, int &state, Vector &xs) const;
	inline void SetMouseState (int state) { mstate = state; }

	static bool Read (std::ifstream &ifs);
	void Write (std::ostream &ofs) const;

	/**
     * \brief Called when the user interactively changes a simulation option
     * \param cat option category (see \ref optcat)
     * \param item option item (see \ref optitem)
     */
	virtual void OptionChanged(DWORD cat, DWORD item);

private:

	inline int AreaIndex (int aid) const
	{ 
		for (int i = 0; i < narea; i++) if (area[i]->id == aid) return i;
		return -1;
	}
	// returns list index of area given by its identifier (or -1 if doesn't exist)

	const Pane *pane;
	oapi::GraphicsClient *gc;
	int id;
	int connect[4];   // connected panels (left,right,up,down) -1=none
	int idx_mfocus;   // index of area currently receiving mouse focus
	int mstate;       // current mouse state
	mutable Vector mouse_r;   // mouse position coefficients (area-type dependent)
	HWND cwnd;        // window handle for mouse position offset calculations

	struct {          // HUD parameters
		VCHUDSPEC spec;             // VC HUD specs
		SURFHANDLE surf;            // surface for VC HUD
		COLORREF col;               // HUD colour
		BYTE intens;                // HUD intensity
	} hud;

	struct Area {
		Area() {}
		int id;       // area identifier
		RECT texrect; // area rectangle in texture
		int w, h;     // width, height of texture area
		int bltmode;  // blt mode: 0=return black surface, 1=return background, 2=return current panel state
		int redraw;   // redraw trigger code: 0=no redraw, 1=each frame, 2=on mouseclick in area
		int mouse;    // mouse states to cause callbacks
		SURFHANDLE tgt, surf, bksurf; // target, area, background surfaces

		// mouse click area definition - currently only spherical click areas are supported
		enum ClickMode { CMODE_NONE, CMODE_SPHERICAL, CMODE_QUAD } cmode;
		union {
			struct {
				Vector cnt;   // centre of click area in local vessel coords
				double rad;   // radius of click area
			};
			struct {
				Vector p[4];            // corner points
				float a, b, c, d;       // coeffs for equation of plane: ax+by+cz+d = 0
				float u[4], v[4];       // coefficients for transforming to local quad frame
			};
		};
	} **area;
	int narea, nareabuf;
};

#endif // !__VCOCKPIT_H