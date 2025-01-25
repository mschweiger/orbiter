// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __SELECT_H
#define __SELECT_H

#include "OrbiterAPI.h"
#include <list>

#define ITEM_SUBMENU 0x01
#define ITEM_NOHILIGHT 0x02
#define ITEM_SEPARATOR 0x04

struct SelectEntry {
    int m_Flags;
    std::string m_Text;
    std::list<SelectEntry> m_SubEntries;
};

class Select : public ImGuiDialog {
public:
    typedef bool (*Callbk)(Select*, int, char*, void*);
    Select();
    void Display() override {
		OnDraw();
		if (!active) OnClose();
	}

	void OnDraw() override;
    bool opened;
    std::string title;
    std::list<SelectEntry> rootmenu;
    std::list<SelectEntry> *currententry;
    Callbk cbSubmenu;
    Callbk cbEnter;
    void *userdata;

    void Open(const char *_title = 0, Callbk submenu_cbk = 0, Callbk enter_cbk = 0, void *_userdata = 0, int cntx = -1, int cnty = -1);
    void Append(const char *str, int flags = 0);
    void AppendSeparator();
    void DrawMenu(std::list<SelectEntry>& entries);
};

class InputBox : public ImGuiDialog {
public:
    typedef bool (*Callbk)(InputBox*, char*, void*);
    InputBox();
    void Display() override {
		OnDraw();
		if (!active) OnClose();
	}
	void OnDraw() override;
    bool opened;
    std::string title;
    Callbk cbEnter;
    Callbk cbCancel;
    void *userdata;
    char inputbuf[128];

    void Open(const char *_title = 0, char *_buf = 0, int _vislen = 20,
        Callbk cbk = 0, void *_userdata = 0, int cntx = -1, int cnty = -1);

    bool OpenEx(const char *_title = 0, char *_buf = 0, int _vislen = 20,
        Callbk enter_cbk = 0, Callbk cancel_cbk = 0, void *_userdata = 0,
        int flags = 0, int cntx = -1, int cnty = -1);
};

#endif
