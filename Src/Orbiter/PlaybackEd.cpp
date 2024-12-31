// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "PlaybackEd.h"
#include "DlgMgr.h" // remove
#include "Camera.h"
#include "Log.h"
#include "resource.h"

using namespace std;

extern Orbiter *g_pOrbiter;
extern Camera *g_camera;
extern TimeData td;
extern char DBG_MSG[256];

INT_PTR CALLBACK RecPlayAnn_DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

// =========================================================

PlaybackEvent *PlaybackEvent::Create (PlaybackEditor *editor, char *event)
{
	char *s;
	double t;
	s = strtok (event, " \t\n");
	if (!s || sscanf (s, "%lf", &t) != 1) return NULL;
	s = strtok (NULL, " \t\n");
	if (!s) return NULL;
	if (!_stricmp (s, "TACC")) {
		double acc, delay;
		s = strtok (NULL, " \t\n");
		if (!s || sscanf (s, "%lf", &acc) != 1) return NULL;
		s = strtok (NULL, " \t\n");
		if (!s || sscanf (s, "%lf", &delay) != 1)
			delay = 0.0;
		TRACENEW; return new TaccEvent (editor, t, acc, delay);
	} else if (!_stricmp (s, "CAMERA")) {
		DWORD pr;
		s = strtok (NULL, " \t\n");
		if (!s) return NULL;
		if (!_stricmp (s, "PRESET")) {
			s = strtok (NULL, " \t\n");
			if (!s || sscanf (s, "%d", &pr) != 1) return NULL;
			TRACENEW; return new CameraEvent (editor, t, pr);
		} else if (!strcmp (s, "SET")) {
			TRACENEW; return new CameraEvent (editor, t, s+4);
		}
	} else if (!_stricmp (s, "NOTE")) {
		TRACENEW; return new NoteEvent (editor, t, strtok (NULL, "\n"));
	} else if (!_stricmp (s, "NOTEPOS")) {
		double x0, y0, x1, y1;
		int res = sscanf (s+8, "%lf %lf %lf %lf", &x0, &y0, &x1, &y1);
		if (res != 4) return NULL;
		else { TRACENEW; return new NoteposEvent (editor, t, x0, y0, x1, y1); }
	} else if (!_stricmp (s, "NOTECOL")) {
		double r, g, b;
		int res = sscanf (s+8, "%lf %lf %lf", &r, &g, &b);
		if (res != 3) return NULL;
		else { TRACENEW; return new NotecolEvent (editor, t, r, g, b); }
	} else if (!_stricmp (s, "NOTESIZE")) {
		double scale;
		int res = sscanf (s+9, "%lf", &scale);
		if (!res) return NULL;
		else { TRACENEW; return new NotesizeEvent (editor, t, scale); }
	} else {
		TRACENEW; return new GenericEvent (editor, t, s, strtok (NULL, "\n"));
	}
	return NULL;
}

PlaybackEvent::PlaybackEvent (PlaybackEditor *_editor, double _t0)
{
	editor = _editor;
	t0 = _t0;
}

void PlaybackEvent::TimeStr (char *str)
{
	sprintf (str, "%0.2f", t0);
}

void PlaybackEvent::WriteEvent (ofstream &ofs, const char *eventtype, const char *event)
{
	ofs << t0 << ' ' << eventtype;
	if (event) ofs << ' ' << event;
	ofs << endl;
}

void PlaybackEvent::EditEvent (PlaybackEditor *editor)
{
	HWND hEditor = editor->EditTab();
	if (hEditor) {
		char cbuf[64];
		sprintf (cbuf, "%0.3f", t0);
		SetWindowText (GetDlgItem (hEditor, IDC_EVTIME), cbuf);
	}
}

void PlaybackEvent::CommitEdit ()
{
	HWND hEdit = editor->EditTab();
	if (hEdit) {
		char cbuf[256];
		double t;
		GetWindowText (GetDlgItem (hEdit, IDC_EVTIME), cbuf, 256);
		if (sscanf (cbuf, "%lf", &t) == 1 && fabs (t-t0) > 1e-3) {
			t0 = t;
			// check if we need to re-sort the event list
			editor->SortEvent (this);
		}
	}
}

INT_PTR PlaybackEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->idFrom == IDC_SPIN1) {
			if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
				NMUPDOWN *nmud = (NMUPDOWN*)lParam;
				char cbuf[256];
				double et;
				GetWindowText (GetDlgItem (hDlg, IDC_EVTIME), cbuf, 256);
				if (sscanf (cbuf, "%lf", &et)) {
					et -= nmud->iDelta;
					et = max (et, 0.0);
					sprintf (cbuf, "%f", et);
					SetWindowText (GetDlgItem (hDlg, IDC_EVTIME), cbuf);
					CommitEdit();
				}
			}
		}
		break;
	}
	return FALSE;
}

// =========================================================

GenericEvent::GenericEvent (PlaybackEditor *_editor, double _t0, const char *_tag, const char *_content): PlaybackEvent (_editor, _t0)
{
	content = tag = 0;
	SetTag (_tag);
	SetContent (_content);
}

GenericEvent::~GenericEvent ()
{
	if (tag) {
		delete []tag;
		tag = NULL;
	}
	if (content) {
		delete []content;
		content = NULL;
	}
}

void GenericEvent::SetTag (const char *_tag)
{
	if (tag) {
		delete []tag;
		tag = NULL;
	}
	if (_tag) {
		tag = new char[strlen(_tag)+1]; TRACENEW
		strcpy (tag, _tag);
	} else tag = 0;
}

void GenericEvent::SetContent (const char *_content)
{
	if (content) {
		delete []content;
		content = NULL;
	}
	if (_content) {
		content = new char[strlen(_content)+1]; TRACENEW
		strcpy (content, _content);
	} else content = 0;
}

void GenericEvent::TagStr (char *str)
{
	strcpy (str, tag);
}

void GenericEvent::DescStr (char *str)
{
	if (content) strncpy (str, content, 100);
	else str[0] = '\0';
}

void GenericEvent::Write (ofstream &ofs)
{
	WriteEvent (ofs, tag, content);
}

void GenericEvent::EditEvent (PlaybackEditor *editor)
{
	editor->OpenEditTab (this, IDD_PBEDITOR_GENERIC, EditProc);
	PlaybackEvent::EditEvent (editor);
	if (tag)
		SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT3), tag);
	if (content)
		SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT1), content);
}

void GenericEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[2048];
	HWND hEdit = editor->EditTab();
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT3), cbuf, 2048);
	SetTag (cbuf);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 2048);
	SetContent (cbuf);
}

INT_PTR GenericEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return PlaybackEvent::MsgProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK GenericEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GenericEvent *event = (uMsg == WM_INITDIALOG ?
		(GenericEvent*)lParam : (GenericEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

TaccEvent::TaccEvent (PlaybackEditor *_editor, double _t0, double _tacc, double _delay): PlaybackEvent (_editor, _t0)
{
	tacc = _tacc;
	delay = _delay;
}

void TaccEvent::SetTacc (double _tacc)
{
	tacc = min (1e5, max (0.1, _tacc));
	char cbuf[256];
	sprintf (cbuf, "%0.2f", tacc);
	SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT1), cbuf);
}

void TaccEvent::SetDelay (double _delay)
{
	delay = max (0.0, _delay);
}

void TaccEvent::TagStr (char *str)
{
	strcpy (str, "TACC");
}

void TaccEvent::DescStr (char *str)
{
	if (delay) {
		sprintf (str, "%0.2fx (delay %0.1f)", tacc, delay);
	} else {
		sprintf (str, "%0.2fx", tacc);
	}
}

void TaccEvent::Write (ofstream &ofs)
{
	char cbuf[256];
	if (delay) sprintf (cbuf, "%f %f", tacc, delay);
	else       sprintf (cbuf, "%f", tacc);
	WriteEvent (ofs, "TACC", cbuf);
}

void TaccEvent::EditEvent (PlaybackEditor *editor)
{
	editor->OpenEditTab (this, IDD_PBEDITOR_TACC, EditProc);
	PlaybackEvent::EditEvent (editor);
	char cbuf[128];
	sprintf (cbuf, "%0.2f", tacc);
	SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT1), cbuf);
	sprintf (cbuf, "%0.2f", delay);
	SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT2), cbuf);
}

void TaccEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[2048];
	double ta, dl;
	HWND hEdit = editor->EditTab();
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 2048);
	sscanf (cbuf, "%lf", &ta);
	SetTacc (ta);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT2), cbuf, 2048);
	sscanf (cbuf, "%lf", &dl);
	SetDelay (dl);
}

INT_PTR TaccEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			SetTacc (0.1);
			return FALSE;
		case IDC_BUTTON2:
			SetTacc (1.0);
			return FALSE;
		case IDC_BUTTON3:
			SetTacc (10.0);
			return FALSE;
		case IDC_BUTTON4:
			SetTacc (1e2);
			return FALSE;
		case IDC_BUTTON5:
			SetTacc (1e3);
			return FALSE;
		case IDC_BUTTON6:
			SetTacc (1e4);
			return FALSE;
		}
		break;
	}
	return PlaybackEvent::MsgProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK TaccEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TaccEvent *event = (uMsg == WM_INITDIALOG ?
		(TaccEvent*)lParam : (TaccEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

CameraEvent::CameraEvent (PlaybackEditor *_editor, double _t0, DWORD _preset): PlaybackEvent (_editor, _t0)
{
	if (_preset != (DWORD)-1) {
		SetPreset (_preset);
	} else {
		char cbuf[256];
		CameraMode *cm = g_camera->GetCMode();
		cm->Store (cbuf);
		SetInlineMode (cbuf);
	}
}

CameraEvent::CameraEvent (PlaybackEditor *_editor, double _t0, char *_modestr): PlaybackEvent (_editor, _t0)
{
	SetInlineMode (_modestr);
}

void CameraEvent::SetPreset (DWORD _preset)
{
	preset = _preset;
	sprintf (modestr, "PRESET %d", preset);
}

void CameraEvent::SetInlineMode (char *mode)
{
	preset = (DWORD)-1;
	strcpy (modestr, "SET ");
	strcat (modestr, mode);
}

void CameraEvent::TagStr (char *str)
{
	strcpy (str, "CAMERA");
}

void CameraEvent::DescStr (char *str)
{
	strcpy (str, modestr);
}

void CameraEvent::Write (ofstream &ofs)
{
	WriteEvent (ofs, "CAMERA", modestr);
}

void CameraEvent::EditEvent (PlaybackEditor *editor)
{
	editor->OpenEditTab (this, IDD_PBEDITOR_CAMPRESET, EditProc);
	PlaybackEvent::EditEvent (editor);
	if (preset != (DWORD)-1)
		SendDlgItemMessage (editor->EditTab(), IDC_COMBO1, CB_SETCURSEL, preset, 0);
}

void CameraEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	HWND hEdit = editor->EditTab();
	DWORD idx = SendDlgItemMessage (hEdit, IDC_COMBO1, CB_GETCURSEL, 0, 0);
	if (idx != CB_ERR) SetPreset (idx);
}

void CameraEvent::ScanPresets (HWND hTab)
{
	// populate the list of available presets
	DWORD i, np = g_camera->nPreset();
	char cbuf[256], descr[256];
	SendDlgItemMessage (hTab, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	for (i = 0; i < np; i++) {
		CameraMode *cm = g_camera->GetPreset (i);
		cm->GetDescr (descr, 256);
		sprintf (cbuf, "%d (%s)", i, descr);
		SendDlgItemMessage (hTab, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)cbuf);
	}
	if (preset < np)
		SendDlgItemMessage (hTab, IDC_COMBO1, CB_SETCURSEL, preset, 0);
	else
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1), modestr+4);
}

void CameraEvent::AddCurrentView (HWND hTab)
{
	char cbuf[256];
	CameraMode *cm = g_camera->GetCMode();
	cm->Store (cbuf);
	SetInlineMode (cbuf);
	SendDlgItemMessage (hTab, IDC_COMBO1, CB_SETCURSEL, -1, 0);
	SetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf);
}

INT_PTR CameraEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		ScanPresets (hDlg);
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_COMBO1:
			if (HIWORD (wParam) == CBN_SELCHANGE) {
				DWORD idx = SendDlgItemMessage (hDlg, IDC_COMBO1, CB_GETCURSEL, 0, 0);
				SetPreset (idx);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), "");
				return FALSE;
			}
			break;
		case IDC_BUTTON1:
			AddCurrentView (hDlg);
			return FALSE;
		}
		break;
	}
	return PlaybackEvent::MsgProc (hDlg, uMsg, wParam, lParam);
}


INT_PTR CALLBACK CameraEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CameraEvent *event = (uMsg == WM_INITDIALOG ?
		(CameraEvent*)lParam : (CameraEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

NoteEvent::NoteEvent (PlaybackEditor *_editor, double _t0, const char *_note): PlaybackEvent (_editor, _t0)
{
	note = NULL;
	SetNote (_note);
}

NoteEvent::~NoteEvent ()
{
	if (note) {
		delete []note;
		note = NULL;
	}
}

void NoteEvent::SetNote (const char *_note)
{
	if (note) {
		delete []note;
		note = NULL;
	}
	if (_note) {
		note = new char[strlen(_note)+1]; TRACENEW
		strcpy (note, _note);
	} else {
		note = 0;
	}
}

void NoteEvent::TagStr (char *str)
{
	strcpy (str, "NOTE");
}

void NoteEvent::DescStr (char *str)
{
	if (note) strncpy (str, note, 100);
	else str[0] = '\0';
}

void NoteEvent::Write (ofstream &ofs)
{
	WriteEvent (ofs, "NOTE", note ? note : "");
}

void NoteEvent::EditEvent (PlaybackEditor *editor)
{
	editor->OpenEditTab (this, IDD_PBEDITOR_NOTE, EditProc);
	PlaybackEvent::EditEvent (editor);
	SetWindowText (GetDlgItem (editor->EditTab(), IDC_EDIT1), note);
}

void NoteEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[2048];
	HWND hEdit = editor->EditTab();
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 2048);
	SetNote (cbuf);
}

INT_PTR CALLBACK NoteEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NoteEvent *event = (uMsg == WM_INITDIALOG ?
		(NoteEvent*)lParam : (NoteEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

NoteposEvent::NoteposEvent (PlaybackEditor *_editor, double _t0, double _x0, double _y0, double _x1, double _y1): PlaybackEvent (_editor, _t0)
{
	SetPos (_x0, _y0, _x1, _y1);
}

void NoteposEvent::SetPos (double _x0, double _y0, double _x1, double _y1)
{
	x0 = _x0;
	y0 = _y0;
	x1 = _x1;
	y1 = _y1;
}

void NoteposEvent::TagStr (char *str)
{
	strcpy (str, "NOTEPOS");
}

void NoteposEvent::DescStr (char *str)
{
	sprintf (str, "%g %g %g %g", x0, y1, x1, y1);
}

void NoteposEvent::Write (ofstream &ofs)
{
	char cbuf[256];
	sprintf (cbuf, "%g %g %g %g", x0, y0, x1, y1);
	WriteEvent (ofs, "NOTEPOS", cbuf);
}

void NoteposEvent::EditEvent (PlaybackEditor *editor)
{
	char cbuf[256];
	editor->OpenEditTab (this, IDD_PBEDITOR_NOTEPOS, EditProc);
	PlaybackEvent::EditEvent (editor);
	HWND hEdit = editor->EditTab();
	sprintf (cbuf, "%0.2f", x0); SetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf);
	sprintf (cbuf, "%0.2f", y0); SetWindowText (GetDlgItem (hEdit, IDC_EDIT2), cbuf);
	sprintf (cbuf, "%0.2f", x1); SetWindowText (GetDlgItem (hEdit, IDC_EDIT3), cbuf);
	sprintf (cbuf, "%0.2f", y1); SetWindowText (GetDlgItem (hEdit, IDC_EDIT4), cbuf);
}

void NoteposEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[256];
	HWND hEdit = editor->EditTab();
	double _x0, _y0, _x1, _y1;
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 256); sscanf (cbuf, "%lf", &_x0);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT2), cbuf, 256); sscanf (cbuf, "%lf", &_y0);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT3), cbuf, 256); sscanf (cbuf, "%lf", &_x1);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT4), cbuf, 256); sscanf (cbuf, "%lf", &_y1);
	SetPos (_x0, _y0, _x1, _y1);
}

INT_PTR CALLBACK NoteposEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NoteposEvent *event = (uMsg == WM_INITDIALOG ?
		(NoteposEvent*)lParam : (NoteposEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

NotecolEvent::NotecolEvent (PlaybackEditor *_editor, double _t0, double _r, double _g, double _b): PlaybackEvent (_editor, _t0)
{
	SetCol (_r, _g, _b);
}

void NotecolEvent::SetCol (double _r, double _g, double _b)
{
	r = _r;
	g = _g;
	b = _b;
}

void NotecolEvent::TagStr (char *str)
{
	strcpy (str, "NOTECOL");
}

void NotecolEvent::DescStr (char *str)
{
	sprintf (str, "%g %g %g", r, g, b);
}

void NotecolEvent::Write (ofstream &ofs)
{
	char cbuf[256];
	sprintf (cbuf, "%g %g %g", r, g, b);
	WriteEvent (ofs, "NOTECOL", cbuf);
}

void NotecolEvent::EditEvent (PlaybackEditor *editor)
{
	char cbuf[256];
	editor->OpenEditTab (this, IDD_PBEDITOR_NOTECOL, EditProc);
	PlaybackEvent::EditEvent (editor);
	HWND hEdit = editor->EditTab();
	sprintf (cbuf, "%g", r); SetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf);
	sprintf (cbuf, "%g", g); SetWindowText (GetDlgItem (hEdit, IDC_EDIT2), cbuf);
	sprintf (cbuf, "%g", b); SetWindowText (GetDlgItem (hEdit, IDC_EDIT3), cbuf);
}

void NotecolEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[256];
	HWND hEdit = editor->EditTab();
	double _r, _g, _b;
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 256); sscanf (cbuf, "%lf", &_r);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT2), cbuf, 256); sscanf (cbuf, "%lf", &_g);
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT3), cbuf, 256); sscanf (cbuf, "%lf", &_b);
	SetCol (_r, _g, _b);
}

INT_PTR NotecolEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_NOTIFY: {
		int editid = 0;
		switch (((NMHDR*)lParam)->idFrom) {
		case IDC_SPIN2: editid = IDC_EDIT1; break;
		case IDC_SPIN3: editid = IDC_EDIT2; break;
		case IDC_SPIN4: editid = IDC_EDIT3; break;
		}
		if (editid) {
			if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
				NMUPDOWN *nmud = (NMUPDOWN*)lParam;
				char cbuf[256];
				double col;
				GetWindowText (GetDlgItem (hDlg, editid), cbuf, 256);
				if (sscanf (cbuf, "%lf", &col)) {
					col -= nmud->iDelta*0.1;
					col = min (max (col, 0.0), 1.0);
					sprintf (cbuf, "%f", col);
					SetWindowText (GetDlgItem (hDlg, editid), cbuf);
				}
			}
		}
		} break;
	}
	return PlaybackEvent::MsgProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK NotecolEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NotecolEvent *event = (uMsg == WM_INITDIALOG ?
		(NotecolEvent*)lParam : (NotecolEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================

NotesizeEvent::NotesizeEvent (PlaybackEditor *_editor, double _t0, double _size): PlaybackEvent (_editor, _t0)
{
	SetSize (_size);
}

void NotesizeEvent::SetSize (double _size)
{
	size = _size;
}

void NotesizeEvent::TagStr (char *str)
{
	strcpy (str, "NOTESIZE");
}

void NotesizeEvent::DescStr (char *str)
{
	sprintf (str, "%g", size);
}

void NotesizeEvent::Write (ofstream &ofs)
{
	char cbuf[256];
	sprintf (cbuf, "%g", size);
	WriteEvent (ofs, "NOTESIZE", cbuf);
}

void NotesizeEvent::EditEvent (PlaybackEditor *editor)
{
	char cbuf[256];
	editor->OpenEditTab (this, IDD_PBEDITOR_NOTESIZE, EditProc);
	PlaybackEvent::EditEvent (editor);
	HWND hEdit = editor->EditTab();
	sprintf (cbuf, "%g", size); SetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf);
}

void NotesizeEvent::CommitEdit ()
{
	PlaybackEvent::CommitEdit();
	char cbuf[256];
	HWND hEdit = editor->EditTab();
	double _size;
	GetWindowText (GetDlgItem (hEdit, IDC_EDIT1), cbuf, 256); sscanf (cbuf, "%lf", &_size);
	SetSize (_size);
}

INT_PTR NotesizeEvent::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			double _size;
			GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
			if (sscanf (cbuf, "%lf", &_size)) {
				_size -= nmud->iDelta*0.1;
				_size = max (_size, 0.0);
				sprintf (cbuf, "%f", _size);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
			}
		}
		break;
	}
	return PlaybackEvent::MsgProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR CALLBACK NotesizeEvent::EditProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NotesizeEvent *event = (uMsg == WM_INITDIALOG ?
		(NotesizeEvent*)lParam : (NotesizeEvent*)GetWindowLongPtr (hDlg, GWLP_USERDATA));
	return (event ? event->MsgProc (hDlg, uMsg, wParam, lParam) : FALSE);
}

// =========================================================
// =========================================================

PlaybackEditor::PlaybackEditor (Orbiter *ob, const char *ScnName)
{
	int i;
	char fname[1024];

	orbiter = ob;
	Efirst = Elast = 0;
	timer = 0;

	for (i = strlen(ScnName)-1; i > 0; i--)
		if (ScnName[i-1] == '\\') break;
	sprintf (fname, "Flights\\%s\\system.dat", ScnName+i);

	sysfname = new char[strlen(fname)+1]; TRACENEW
	strcpy (sysfname, fname);

	hDlg = OpenDialog();
	hEdit = NULL;

	ScanEventFile ();
	RefreshEventList ();
	InsertTMarker ();
}

PlaybackEditor::~PlaybackEditor ()
{
	if (hEdit) {
		CommitEdit();
		DestroyWindow (hEdit);
	}
	CloseDialog();
	if (timer) KillTimer (hDlg, timer);
}

HWND PlaybackEditor::OpenDialog ()
{
	return orbiter->OpenDialogEx (IDD_RECPLAY_ANNOTATE, RecPlayAnn_DlgProc,  DLG_CAPTIONCLOSE | DLG_CAPTIONHELP, this);
}

void PlaybackEditor::CloseDialog ()
{
	if (hDlg) {
		orbiter->CloseDialog (hDlg);
		hDlg = 0;
	}
}

HWND PlaybackEditor::OpenEditTab (PlaybackEvent *event, int resid, DLGPROC tabproc)
{
	if (hEdit) {
		RegisterEdit (hEdit);  // save current edits
		DestroyWindow (hEdit); // remove current edit tab
	}
	if (resid) {
		hEdit = CreateDialogParam (orbiter->GetInstance(), MAKEINTRESOURCE(resid), hDlg, tabproc, (LPARAM)event);
		SetWindowLongPtr (hEdit, GWLP_USERDATA, (LONG_PTR)event);
	} else {
		hEdit = NULL;
	}
	return hEdit;
}

void PlaybackEditor::RegisterEdit (HWND hEdit)
{
	PlaybackEvent *e = (PlaybackEvent*)GetWindowLongPtr (hEdit, GWLP_USERDATA);
	if (e) e->CommitEdit();
	int idx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETCURSEL, 0, 0);
	if (idx != LB_ERR)
		e = (PlaybackEvent*)SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETITEMDATA, idx, 0);
	else e = 0;
	RefreshEventList (e);
	InsertTMarker();
}

INT_PTR PlaybackEditor::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG: {
		INT tabs[2] = {40, 80};
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETTABSTOPS, 2, (LPARAM)tabs);
		CreateInsertEventList (hDlg);
		timer = SetTimer (hDlg, 1, 100, NULL);
		} return 0;
	case WM_TIMER:
		RefreshTMarker();
		break;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDHELP: {
			extern HELPCONTEXT DefHelpContext;
			DefHelpContext.topic = (char*)"/playbackedit.htm";
			orbiter->OpenHelp (&DefHelpContext);
			} return TRUE;
		case IDCANCEL:
			orbiter->CloseDialog (hDlg);
			orbiter->FRecorder_ToggleEditor();
			return FALSE;
		case IDC_BUTTON1: // delete event
			DeleteEvent();
			break;
		case IDC_BUTTON2: // insert event
			InsertEvent();
			break;
		case IDC_BUTTON3: // commit edits
			SaveEventFile();
			break;
		case IDC_LIST1:
			switch (HIWORD (wParam)) {
			case LBN_SELCHANGE: {
				int idx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETCURSEL, 0, 0);
				if (idx != LB_ERR && idx != tmarkidx) {
					PlaybackEvent *e = (PlaybackEvent*)SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETITEMDATA, idx, 0);
					if (e) e->EditEvent (this);
				}
				} break;
			}
			break;
		}
		break;
	}
	return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}

void PlaybackEditor::CreateInsertEventList (HWND hDlg)
{
	const int nevent = 7;
	static const char *events[nevent] = {
		"NOTE (annotation text)",
		"NOTEPOS (bounding box)",
		"NOTECOL (text colour)",
		"NOTESIZE (text size)",
		"TACC (time acceleration)",
		"CAMERA (view position)",
		"<Other>"
	};
	SendDlgItemMessage (hDlg, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < nevent; i++)
		SendDlgItemMessage (hDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)events[i]);
	SendDlgItemMessage (hDlg, IDC_COMBO1, CB_SETCURSEL, 0, 0);
}

void PlaybackEditor::ScanEventFile ()
{
	char line[2048];
	ifstream ifs (sysfname);
	while (ifs.getline (line, 2048)) {
		PlaybackEvent *pe = PlaybackEvent::Create (this, line);
		if (pe) {
			Event *event = new Event; TRACENEW
			event->e = pe;
			event->next = NULL;
			if (Elast) Elast->next = event;
			else Efirst = event;
			Elast = event;
		}
	}
}

void PlaybackEditor::SaveEventFile ()
{
	CommitEdit(); // commit last edits
	orbiter->FRecorder_SuspendPlayback();
	ofstream ofs (sysfname);
	Event *ev;
	for (ev = Efirst; ev; ev = ev->next) {
		ev->e->Write (ofs);
	}
	ofs.close();
	orbiter->FRecorder_RescanPlayback();
}

void PlaybackEditor::CommitEdit ()
{
	PlaybackEvent *e = NULL;
	if (hEdit)
		e = (PlaybackEvent*)GetWindowLongPtr (hEdit, GWLP_USERDATA);
	if (e) {
		e->CommitEdit();
		RefreshEventList (e);
		InsertTMarker();
	}
}

void PlaybackEditor::RefreshEventList (PlaybackEvent *emark)
{
	SendDlgItemMessage (hDlg, IDC_LIST1, LB_RESETCONTENT, 0, 0);
	Event *event;
	char cbuf[1024], tstr[64], tagstr[64], descstr[1024];
	int idx;
	for (event = Efirst; event; event = event->next) {
		event->e->TimeStr (tstr);
		event->e->TagStr (tagstr);
		event->e->DescStr (descstr);
		sprintf (cbuf, "%s\t%s\t%s", tstr, tagstr, descstr);
		idx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)cbuf);
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETITEMDATA, idx, (LPARAM)event->e);
		if (event->e == emark)
			SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETCURSEL, idx, 0);
	}
}

int PlaybackEditor::InsertTMarker ()
{
	int i;
	char cbuf[1024];
	char tstr[1024];
	sprintf (tstr, "%0.1f\t< < < < < < < < < < < < < < < < < < <", td.SimT0);
	double t;
	for (i = 0;; i++) {
		if (SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETTEXT, i, (LPARAM)cbuf) == LB_ERR)
			break;
		if (sscanf (cbuf, "%lf", &t) != 1) continue;
		if (t >= td.SimT0) {
			SendDlgItemMessage (hDlg, IDC_LIST1, LB_INSERTSTRING, i, (LPARAM)tstr);
			tmarkidx = i;
			return i;
		}
	}
	return -1;
}

void PlaybackEditor::RefreshTMarker ()
{
	char cbuf[1024];
	char tstr[1024];
	double t, dummy;
	if (orbiter->IsRunning()) {
		bool blink = (modf (td.SysT0, &dummy) > 0.5);
		if (blink) sprintf (tstr, "%0.0f", td.SimT0);
		else sprintf (tstr, "%0.0f\t< < < < < < < < < < < < < < < < < < <", td.SimT0);
	} else {
		sprintf (tstr, "%0.2f\t< < < < < < < < < < < < < < < < < < < [Paused]", td.SimT0);
	}
	int i, topidx;
	for (i = tmarkidx+1;; i++) {
		if (SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETTEXT, i, (LPARAM)cbuf) == LB_ERR)
			break;
		if (sscanf (cbuf, "%lf", &t) != 1) continue;
		if (t >= td.SimT0) break;
	}
	bool replace = (tmarkidx != i-1);
	if (!replace) {
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETTEXT, tmarkidx, (LPARAM)cbuf);
		replace = (strcmp (cbuf, tstr) != 0);
	}
	if (replace) {
		topidx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETTOPINDEX, 0, 0);
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_DELETESTRING, tmarkidx, 0);
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_INSERTSTRING, tmarkidx = i-1, (LPARAM)tstr);
		SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETTOPINDEX, topidx, 0);
	}
	if (orbiter->IsRunning()) {
		topidx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETTOPINDEX, 0, 0);
		if (topidx < max (tmarkidx-14, 0)) {
			SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETTOPINDEX, max (0, tmarkidx-14), 0);
		} else if (topidx > tmarkidx-4) {
			SendDlgItemMessage (hDlg, IDC_LIST1, LB_SETTOPINDEX, tmarkidx-4, 0);
		}
	}
}

void PlaybackEditor::InsertEvent ()
{
	PlaybackEvent *e = 0;
	const double eps = 1e-3;
	double newtime = td.SimT0-eps; // make sure the event is read when scanning the event list to current time
	char cbuf[1024], eventstr[1024];
	GetWindowText (GetDlgItem (hDlg, IDC_COMBO1), cbuf, 1024);
	sscanf (cbuf, "%s", eventstr);
	if (!_stricmp (eventstr, "NOTE")) {
		e = new NoteEvent (this, newtime, ""); TRACENEW
	} else if (!_stricmp (eventstr, "NOTEPOS")) {
		e = new NoteposEvent (this, newtime, 0, 0, 1, 1); TRACENEW
	} else if (!_stricmp (eventstr, "NOTECOL")) {
		e = new NotecolEvent (this, newtime, 1, 1, 1); TRACENEW
	} else if (!_stricmp (eventstr, "NOTESIZE")) {
		e = new NotesizeEvent (this, newtime, 1); TRACENEW
	} else if (!_stricmp (eventstr, "TACC")) {
		e = new TaccEvent (this, newtime, 1, 0); TRACENEW
	} else if (!_stricmp (eventstr, "CAMERA")) {
		e = new CameraEvent (this, newtime); TRACENEW
	} else if (!_stricmp (eventstr, "<Other>")) {
		e = new GenericEvent (this, newtime, "<TAG>", ""); TRACENEW
	}
	if (e) {
		Event *ev, *pev = 0, *event = new Event; TRACENEW
		event->e = e;
		event->next = NULL;
		if (!Efirst) {
			Efirst = event;
		} else {
			for (ev = Efirst; ev; ev = ev->next) {
				if (ev->e->T0() > event->e->T0()) {
					if (pev) {
						pev->next = event;
					} else {
						Efirst = event;
					}
					event->next = ev;
					break;
				}
				pev = ev;
			}
			if (!ev) { // add event to end of list
				event->next = NULL;
				if (pev) pev->next = event;
				Elast->next = event;
			}
		}
		if (!event->next) Elast = event;
	}
	RefreshEventList (e);
	InsertTMarker ();
	if (e) e->EditEvent (this);
}

void PlaybackEditor::DeleteEvent ()
{
	LRESULT idx = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETCURSEL, 0, 0);
	if (idx == LB_ERR || idx == tmarkidx) return;

	LRESULT res = SendDlgItemMessage (hDlg, IDC_LIST1, LB_GETITEMDATA, idx, 0);
	if (res == LB_ERR || res == 0) return;

	PlaybackEvent *e = (PlaybackEvent*)res;
	Event *ev, *pev = 0;
	for (ev = Efirst; ev; ev = ev->next) {
		if (ev->e == e) {
			if (hEdit) {
				DestroyWindow (hEdit);
				hEdit = NULL;
			}
			if (pev) pev->next = ev->next;
			if (Efirst == ev) Efirst = ev->next;
			if (Elast == ev) Elast = pev;
			delete ev->e;
			delete ev;
			RefreshEventList ();
			InsertTMarker ();
			break;
		}
		pev = ev;
	}
}

void PlaybackEditor::SortEvent (PlaybackEvent *e)
{
	Event *event, *ev, *pev = 0;
	bool needsort = false;
	for (ev = Efirst; ev; ev = ev->next) {
		if (ev->e == e) {
			if (pev && pev->e->T0() > e->T0()) { needsort = true; break; }
			if (ev->next && ev->next->e->T0() < e->T0()) { needsort = true; break; }
		}
		pev = ev;
	}
	if (needsort) {
		event = ev;
		// prise the event out of the list
		if (pev) pev->next = ev->next;
		if (Efirst == ev) Efirst = ev->next;
		if (Elast == ev) Elast = pev;
		// and re-insert in new place
		event->next = NULL;
		for (ev = Efirst, pev = 0; ev; ev = ev->next) {
			if (ev->e->T0() > e->T0()) {
				if (pev) pev->next = event;
				else     Efirst = event;
				event->next = ev;
				break;
			}
			pev = ev;
		}
		if (!event->next) Elast = event;
	}
}

// ======================================================================

INT_PTR CALLBACK RecPlayAnn_DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PlaybackEditor *pe = g_pOrbiter->FReditor;
	if (uMsg == WM_INITDIALOG) {
		pe = (PlaybackEditor*)lParam;
	}
	if (pe) return pe->DlgProc (hDlg, uMsg, wParam, lParam);
	else return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}

