
//-----------------------------------------------------------------------------

//
// Includes
//

#pragma region Includes

//#include "stdafx.h"

// STL
#include <string>
#include <set>
#include <map>
#include <fstream>
#include <sstream>

// Windows
#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <mmsystem.h>

// Project
#include "stringutil.h"
#include "seismix.h"
#include "smeditor.h"
#include "luastrut.h"

#pragma endregion

#ifdef _DEBUG
#define _VERIFY(expr) _ASSERT(expr)
#else
#define _VERIFY(expr) (expr)
#endif

//-----------------------------------------------------------------------------

using namespace seismix;

//-----------------------------------------------------------------------------

// Timers
#define IDT_BALOONPOP          101
#define TIMER_BALOONPOP        (GetDoubleClickTime() * 10)

namespace seismix {
	DECLARE_LUAMETHOD(WindowDestroy);
	DECLARE_LUAMETHOD(BaloonShowCtrl);
	DECLARE_LUAMETHOD(BaloonShowPoint);
	DECLARE_LUAMETHOD(BaloonHide);
	DECLARE_LUAMETHOD(getFocus);
	DECLARE_LUAMETHOD(setFocus);
}; // namespace seismix

struct CKeyFile {
	std::string key;
	std::wstring path, file, ext;
};

void MakeKeyFile(const std::wstring& file, CKeyFile& kf);
MCIERROR PlaySound(const std::wstring& snd);
void PlayItem();

void BaloonShow(HWND hwndId, int idCtrl, const std::wstring& msg, const std::wstring& title, int icon, COLORREF cr);
void BaloonShow(HWND hwndId, const POINT& p, const std::wstring& msg, const std::wstring& title, HICON hicon, COLORREF cr);
void BaloonHide(HWND hwndId = 0);

int CALLBACK CompareKeys(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int CALLBACK CompareFile(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
static int getKeyindex(const char* str, size_t len);

//-----------------------------------------------------------------------------

typedef std::map<int, CKeyFile> MapKeyFile;
typedef std::map<std::string, int> MapStrInt;

static WCHAR mpath[_MAX_PATH];
static WCHAR drive[_MAX_DRIVE];
static WCHAR dir[_MAX_DIR];
static HINSTANCE g_hApp = 0;
static HWND g_hwndDlg = 0;
static HWND hwndFiles = 0, hwndPlaylist = 0, hwndBaloon = 0;
static HWND isBaloon = 0;
static BOOL bSkipErr = TRUE, bShowPath = TRUE, bShowExt = TRUE;
static MapKeyFile aFiles;
static MapStrInt  aKeys;
static std::set<std::wstring> sExt;
static std::wstring ExtPlaylist = L"txt";
static int g_index = 0;

static std::string strPlaylist;
static int nPlayitem = -1, nPlaylen = 0;
static std::set<std::string> sPlayset;
static size_t nMaxIdLength = 5;

// Lua virtual machine
static lua_State* g_luaVM = 0;
std::set<MCIDEVICEID> wDeviceID;
bool pause = false;

//-----------------------------------------------------------------------------

IMPLEMENT_LUAMETHOD(seismix, play) {
	auto file = luaL_checkstring(L, 1);
	auto from = (DWORD)luaL_checkinteger(L, 2);
	auto to = (DWORD)luaL_checkinteger(L, 3);

	MCIERROR err = 0;
	MCI_WAVE_OPEN_PARMSA mci1;
	mci1.dwCallback = 0;
	mci1.wDeviceID = 0;
	mci1.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_WAVEFORM_AUDIO;
	mci1.lpstrElementName = file;
	mci1.lpstrAlias = 0;
	mci1.dwBufferSeconds = 4;
	err = mciSendCommandA(0, MCI_OPEN,
		MCI_WAIT | MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_ELEMENT | MCI_WAVE_OPEN_BUFFER, (DWORD_PTR)&mci1);
	if (!err) {
		MCI_PLAY_PARMS mci2;
		mci2.dwCallback = 0;
		mci2.dwFrom = from;
		mci2.dwTo = to;
		wDeviceID.insert(mci1.wDeviceID);
		err = mciSendCommand(mci1.wDeviceID, MCI_PLAY, MCI_WAIT | (from > 0 ? MCI_FROM : 0) | (to > 0 ? MCI_TO : 0), (DWORD_PTR)&mci2);
		wDeviceID.erase(mci1.wDeviceID);
		MCI_GENERIC_PARMS mci3;
		mci3.dwCallback = 0;
		mciSendCommand(mci1.wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&mci3);
	}
	lua_pushnumber(L, err);
	return 1;
}

IMPLEMENT_LUAMETHOD(seismix, send) {
	auto str = luaL_checkstring(L, 1);

	MCIERROR err = mciSendStringA(str, 0, 0, 0);
	lua_pushnumber(L, err);
	return 1;
}

IMPLEMENT_LUAMETHOD(seismix, pause) {
	auto pause = (DWORD)luaL_checkinteger(L, 1);

	Sleep(pause);
	return 0;
}

//-----------------------------------------------------------------------------

IMPLEMENT_LUAMETHOD(seismix, WindowDestroy) {
	EndDialog(g_hwndDlg, 0);
	return 0;
}

IMPLEMENT_LUAMETHOD(seismix, BaloonShowCtrl) {
	auto idCtrl = (int)luaL_checkinteger(L, 1);
	auto msg = luaL_checkwstr(L, 2);
	auto title = luaL_checkwstr(L, 3);
	auto icon = (int)luaL_checkinteger(L, 4);
	auto cr = (COLORREF)luaL_checkinteger(L, 5);

	BaloonShow(g_hwndDlg, idCtrl, msg, title, icon, cr);
	return 0;
}

IMPLEMENT_LUAMETHOD(seismix, BaloonShowPoint) {
	auto x = (LONG)luaL_checkinteger(L, 1);
	auto y = (LONG)luaL_checkinteger(L, 2);
	auto msg = luaL_checkwstr(L, 3);
	auto title = luaL_checkwstr(L, 4);
	auto icon = (int)luaL_checkinteger(L, 5);
	auto cr = (COLORREF)luaL_checkinteger(L, 6);

	POINT p; p.x = x, p.y = y;
	BaloonShow(g_hwndDlg, p, msg, title, (HICON)(INT_PTR)icon, cr);
	return 0;
}

IMPLEMENT_LUAMETHOD(seismix, BaloonHide) {
	BaloonHide();
	return 0;
}

IMPLEMENT_LUAMETHOD(seismix, getFocus) {
	HWND hwnd = GetFocus();
	if (GetParent(hwnd) != g_hwndDlg) hwnd = 0;
	lua_pushinteger(L, hwnd ? (int)GetMenu(hwnd) : 0);
	return 1;
}

IMPLEMENT_LUAMETHOD(seismix, setFocus) {
	auto idCtrl = (int)luaL_checkinteger(L, 1);

	SetFocus(GetDlgItem(g_hwndDlg, idCtrl));
	return 0;
}

//-----------------------------------------------------------------------------

void lunareg_seismix(lua_State* L) {
	static luaL_Reg methods1[] = {
		RESPONSE_LUAFUNC(seismix, play),
		RESPONSE_LUAFUNC(seismix, send),
		RESPONSE_LUAFUNC(seismix, pause),
		{ nullptr, nullptr }
	};
	static luaL_Reg methods2[] = {
		RESPONSE_LUAMETHOD(WindowDestroy),
		RESPONSE_LUAMETHOD(BaloonShowCtrl),
		RESPONSE_LUAMETHOD(BaloonShowPoint),
		RESPONSE_LUAMETHOD(BaloonHide),
		RESPONSE_LUAMETHOD(getFocus),
		RESPONSE_LUAMETHOD(setFocus),
		{ nullptr, nullptr }
	};
	luaL_newlib(L, methods1);
	lua_setglobal(L, "mci");
	luaL_newlib(L, methods2);
	lua_setglobal(L, "wnd");
}

std::ostringstream oserr;
int lua_errtrace(lua_State* L) {
	oserr.clear();
	oserr << lua_tostring(L, -1) << std::endl;
	lua_pop(L, 1);

	oserr << "stack traceback:" << std::endl;
	lua_Debug ar;
	for (int level = 1; lua_getstack(L, level, &ar); level++)
	{
		lua_getinfo(L, "Sln", &ar);
		oserr << "\t" << ar.source << ":" << ar.linedefined << ": in ";
		if (ar.name) oserr << "function '" << ar.name << "'";
		else oserr << "main chunk";
		oserr << std::endl;
	}
	throw std::exception(oserr.str().c_str());
}

void lua_getmethod(lua_State *L, const char* name) {
	lua_getglobal(L, "JSeismix");
	lua_getfield(L, -1, name);
}

//-----------------------------------------------------------------------------

void MakeKeyFile(const std::wstring& file, CKeyFile& kf) {
	size_t pf;
	pf = file.rfind(L'\\');
	pf++;
	size_t pe;
	pe = file.rfind(L'.');
	pe++;
	kf.path = file.substr(0, pf);
	kf.file = file.substr(pf, pe-pf-1);
	kf.ext = file.substr(pe);
	kf.key = wchar_to_utf8(file.substr(pf, min(nMaxIdLength, pe-pf-1)));
	CharLowerW((LPWSTR)kf.ext.c_str());
}

MCIERROR PlaySound(const std::wstring& snd) {
	MCIERROR err;
	MCI_WAVE_OPEN_PARMSW mci1;
	mci1.dwCallback = MAKELONG(g_hwndDlg, 0);
	mci1.wDeviceID = 0;
	mci1.lpstrDeviceType = (LPCWSTR)MCI_DEVTYPE_WAVEFORM_AUDIO;
	mci1.lpstrElementName = snd.data();
	mci1.lpstrAlias = 0;
	mci1.dwBufferSeconds = 4;
	err = mciSendCommandW(0, MCI_OPEN, MCI_WAIT | MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_ELEMENT | MCI_WAVE_OPEN_BUFFER, (DWORD_PTR)&mci1);
	if (!err) {
		MCI_PLAY_PARMS mci2;
		mci2.dwCallback = MAKELONG(g_hwndDlg, 0);
		mci2.dwFrom = 0;
		mci2.dwTo = 0;
		err = mciSendCommand(mci1.wDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&mci2);
		if (!err) {
			_ASSERT(wDeviceID.find(mci1.wDeviceID) == wDeviceID.end()); // ID must be unical
			wDeviceID.insert(mci1.wDeviceID);
		} else {
			MCI_GENERIC_PARMS mci3;
			mci3.dwCallback = MAKELONG(g_hwndDlg, 0);
			mciSendCommand(mci1.wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&mci3);
		}
	}
	return err;
}

void PlayItem() {
	bool ok = false;
	nPlayitem += nPlaylen;
	while (nPlayitem < (int)strPlaylist.size()) {
		int index = getKeyindex(strPlaylist.c_str() + nPlayitem, min(nMaxIdLength, strPlaylist.size() - nPlayitem));
		if (index) {
			const CKeyFile& kf = aFiles[index];
			nPlaylen = kf.key.size();
			ok = !PlaySound(kf.path+kf.file+L"."+kf.ext);
			if (ok || !bSkipErr) break;
		} else {
			nPlaylen = 1;
			if (!bSkipErr) break;
		}
		nPlayitem += nPlaylen;
	}
	if (!ok && !bSkipErr && nPlayitem < (int)strPlaylist.size()) {
		std::wstring msg, title;

		lua_getglobal(g_luaVM, "MsgPlaylistBadkey");
		auto szmsg = lua_tostring(g_luaVM, -1);
		msg = utf8_to_wchar(format(szmsg, strPlaylist.substr(nPlayitem, nPlaylen).c_str(), nPlayitem));
		lua_pop(g_luaVM, 1);

		lua_getglobal(g_luaVM, "TitlePlaylist");
		title = luaL_towstr(g_luaVM, -1);
		lua_pop(g_luaVM, 1);

		BaloonShow(g_hwndDlg, IDC_PLAYLIST, msg, title, 3, RGB(0, 0, 0));
	}
	if ((!ok && !bSkipErr) || nPlayitem >= (int)strPlaylist.size()) {
		nPlayitem = -1, nPlaylen = 0;
		strPlaylist.clear();
		EnableWindow(GetDlgItem(g_hwndDlg, IDC_PLAYLIST), TRUE);
		EnableWindow(GetDlgItem(g_hwndDlg, IDC_DELETE), TRUE);
		EnableWindow(GetDlgItem(g_hwndDlg, IDC_STOP), FALSE);
	}
}

void BaloonShow(HWND hwndId, int idCtrl, const std::wstring& msg, const std::wstring& title, int icon, COLORREF cr) {
	RECT r;
	_VERIFY(GetWindowRect(GetDlgItem(hwndId, idCtrl), &r));
	POINT p;
	p.x = (r.left + r.right)/2, p.y = (r.top + r.bottom)/2;
	BaloonShow(hwndId, p, msg, title, (HICON)(INT_PTR)icon, cr);
}

void BaloonShow(HWND hwndId, const POINT& p, const std::wstring& msg, const std::wstring& title, HICON hicon, COLORREF cr) {
	static WCHAR buftitle[256];
	wcscpy_s(buftitle, _countof(buftitle), title.data());
	_VERIFY(SendMessage(hwndBaloon, TTM_SETTITLE, (WPARAM)hicon, (LPARAM)buftitle));

	static WCHAR bufmsg[1024]; // resorce string may be greater than 80 chars
	wcscpy_s(bufmsg, _countof(bufmsg), msg.data());
	TOOLINFO ti;
	ti.cbSize = sizeof(ti);
	ti.hwnd = hwndId;
	ti.uId = (UINT_PTR)hwndId;
	ti.hinst = g_hApp;
	ti.lpszText = (WCHAR*)bufmsg;
	SendMessage(hwndBaloon, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);

	SendMessage(hwndBaloon, TTM_SETTIPTEXTCOLOR, cr, 0);
	SendMessage(hwndBaloon, TTM_TRACKPOSITION, 0, MAKELPARAM(p.x, p.y));
	SendMessage(hwndBaloon, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);

	isBaloon = hwndId;
	SetTimer(hwndId, IDT_BALOONPOP, TIMER_BALOONPOP, 0);
}

void BaloonHide(HWND hwndId) {
	if (!hwndId) hwndId = isBaloon;
	if (hwndId) {
		TOOLINFO ti;
		ti.cbSize = sizeof(ti);
		ti.hwnd = hwndId;
		ti.uId = (UINT_PTR)hwndId;
		SendMessage(hwndBaloon, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
		if (hwndId == isBaloon) isBaloon = 0;
		KillTimer(hwndId, IDT_BALOONPOP);
	}
}

int CALLBACK CompareKeys(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	int cmp = StrCmpIA(aFiles[lParam1].key.c_str(), aFiles[lParam2].key.c_str());
	if (!cmp) cmp = -StrCmpA(aFiles[lParam1].key.c_str(), aFiles[lParam2].key.c_str());
	return lParamSort*cmp;
}

int CALLBACK CompareFile(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	int cmp = StrCmpIW(aFiles[lParam1].file.c_str(), aFiles[lParam2].file.c_str());
	if (!cmp) cmp = -StrCmpW(aFiles[lParam1].file.c_str(), aFiles[lParam2].file.c_str());
	return lParamSort*cmp;
}

static int getKeyindex(const char* str, size_t len) {
	MapStrInt::const_iterator iter;
	do {
		iter = aKeys.find(std::string(str, len));
		if (iter != aKeys.end())
			return iter->second;
		len--;
	} while (len);
	return 0;
}

static int InsertLine(const CKeyFile& kf) {
	if (sExt.find(kf.ext) == sExt.end()) return 0;
	for each (MapKeyFile::value_type const& v in aFiles) {
		if (v.second.key == kf.key) return 0;
	}
	g_index++;
	aFiles[g_index] = kf;
	aKeys[kf.key] = g_index;

	lua_getglobal(g_luaVM, "KeyFiles");
	auto kfs = wchar_to_utf8(kf.path + kf.file + L"." + kf.ext);
	lua_pushlstring(g_luaVM, kfs.data(), kfs.size());
	lua_setfield(g_luaVM, -2, kf.key.c_str());
	lua_pop(g_luaVM, 1);

	// Insert into listview
	LVITEM lvi;
	int index = INT_MAX;
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.pszText = LPSTR_TEXTCALLBACK;
	lvi.cchTextMax = -1;
	lvi.lParam = (LPARAM)g_index;
	index = SendMessage(hwndFiles, LVM_INSERTITEM, 0, (LPARAM)&lvi);
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	lvi.pszText = LPSTR_TEXTCALLBACK;
	lvi.cchTextMax = -1;
	SendMessage(hwndFiles, LVM_SETITEMTEXT, index, (LPARAM)&lvi);
	return g_index;
}

static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	INT_PTR retval = TRUE;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			g_hwndDlg = hwndDlg;

			// Set main window icons
			SNDMSG(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(g_hApp, MAKEINTRESOURCE(IDI_MAIN)));
			SNDMSG(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(g_hApp, MAKEINTRESOURCE(IDI_MAIN)));

			TOOLINFO ti;
			ti.cbSize = sizeof(ti);
			ti.uFlags = TTF_ABSOLUTE | TTF_IDISHWND | TTF_TRACK;
			ti.hwnd = hwndDlg;
			ti.uId = (UINT_PTR)hwndDlg;
			ti.hinst = g_hApp;
			ti.lpszText = 0;
			_VERIFY(SendMessage(hwndBaloon, TTM_ADDTOOL, 0, (LPARAM)&ti));

			hwndPlaylist = GetDlgItem(hwndDlg, IDC_PLAYLIST);
			hwndFiles = GetDlgItem(hwndDlg, IDC_FILES);

			ListView_SetExtendedListViewStyle(hwndFiles,
				LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES | LVS_EX_TRACKSELECT);

			LV_COLUMNW lvc;
			int i = 1;
			lua_getglobal(g_luaVM, "PlaylistColumns");
			std::wstring wstr;
			do {
				lua_pushinteger(g_luaVM, i);
				lua_gettable(g_luaVM, -2);
				if (lua_istable(g_luaVM, -1)) {
					lvc.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
					lvc.fmt = LVCFMT_LEFT;
					lua_pushinteger(g_luaVM, 1);
					lua_gettable(g_luaVM, -2);
					if (lua_isstring(g_luaVM, -1)) {
						wstr = luaL_towstr(g_luaVM, -1);
						lvc.pszText = (LPWSTR)wstr.data();
					} else break;
					lua_pop(g_luaVM, 1);
					lua_pushinteger(g_luaVM, 2);
					lua_gettable(g_luaVM, -2);
					if (lua_isnumber(g_luaVM, -1)) {
						lvc.cx = (int)lua_tointeger(g_luaVM, -1);
					}
					else break;
					lua_pop(g_luaVM, 1);
					SendMessageW(hwndFiles, LVM_INSERTCOLUMNW, i, (LPARAM)&lvc);
					i++;
				} else i = 0;
				lua_pop(g_luaVM, 1);
			} while (i);
			lua_pop(g_luaVM, 1);

			CKeyFile kf;
			std::wstring buffer;
			lua_getglobal(g_luaVM, "KeyFiles");
			lua_pushnil(g_luaVM);
			while (lua_next(g_luaVM, -2) != 0) {
				if (lua_isstring(g_luaVM, -2) && lua_isstring(g_luaVM, -1)) {
					buffer = luaL_towstr(g_luaVM, -1);
					MakeKeyFile(buffer, kf);
					kf.key = luaL_tombstr(g_luaVM, -2);
					InsertLine(kf);
				}
				lua_pop(g_luaVM, 1);
			}

			SendDlgItemMessage(hwndDlg, IDC_PLAY, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon(g_hApp, MAKEINTRESOURCE(IDI_PLAY)));
			SendDlgItemMessage(hwndDlg, IDC_STOP, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon(g_hApp, MAKEINTRESOURCE(IDI_STOP)));
			EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), FALSE);
			CheckDlgButton(hwndDlg, IDC_SKIPERR, bSkipErr ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SHOWPATH, bShowPath ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SHOWEXT, bShowExt ? BST_CHECKED : BST_UNCHECKED);
			break;
		}

	case WM_DESTROY:
		{
			// Lua response
			lua_State *L = g_luaVM;
			lua_pushcclosure(L, lua_errtrace, 0);
			lua_getmethod(L, "wmDestroy");
			if (lua_isfunction(L, -1)) {
				lua_insert(L, -2);
				lua_pcall(L, 1, 0, -1-2);
			} else lua_pop(L, 2);
			lua_pop(L, 1);

			TOOLINFO ti;
			ti.cbSize = sizeof(ti);
			ti.hwnd = hwndDlg;
			ti.uId = (UINT_PTR)hwndDlg;
			SendMessage(hwndBaloon, TTM_DELTOOL, 0, (LPARAM)&ti);

			// Close all opened waves
			MCI_GENERIC_PARMS mci;
			mci.dwCallback = MAKELONG(hwndDlg, 0);
			for each (MCIDEVICEID const& v in wDeviceID) {
				_VERIFY(!mciSendCommand(v, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&mci));
			}
			break;
		}

	case WM_CLOSE:
		{
			// Lua response
			lua_State *L = g_luaVM;
			lua_pushcclosure(L, lua_errtrace, 0);
			lua_getmethod(L, "wmClose");
			if (lua_isfunction(L, -1)) {
				lua_insert(L, -2);
				lua_pcall(L, 1, 0, -1-2);
			} else lua_pop(L, 2);
			lua_pop(L, 1);
			break;
		}

	case WM_ACTIVATEAPP:
		{
			// Lua response
			lua_State *L = g_luaVM;
			lua_pushcclosure(L, lua_errtrace, 0);
			lua_getmethod(L, "wmActivateApp");
			if (lua_isfunction(L, -1)) {
				lua_insert(L, -2);
				lua_pushboolean(L, wParam);
				lua_pushinteger(L, lParam);
				lua_pcall(L, 3, 0, -3-2);
			} else lua_pop(L, 2);
			lua_pop(L, 1);
			retval = 0;
			break;
		}

	case WM_COMMAND:
		{
			// Lua response
			lua_State *L = g_luaVM;
			bool res = false;
			lua_pushcclosure(L, lua_errtrace, 0);
			lua_getmethod(L, "wmCommand");
			if (lua_isfunction(L, -1)) {
				lua_insert(L, -2);
				lua_pushinteger(L, LOWORD(wParam));
				lua_pushinteger(L, HIWORD(wParam));
				lua_pcall(L, 3, 1, -3-2);
				_ASSERT(lua_isboolean(L, -1));
				res = lua_toboolean(L, -1) != 0;
				lua_pop(L, 1);
			} else lua_pop(L, 2);
			lua_pop(L, 1);
			if (res) break;

			switch (LOWORD(wParam))
			{
			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				break;

			case IDC_PLAY:
				{
					if (nPlayitem < 0) { // Play
						int len = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_PLAYLIST));
						strPlaylist.resize(len);
						GetDlgItemTextA(hwndDlg, IDC_PLAYLIST, (LPSTR)strPlaylist.c_str(), len + 1);

						if (strPlaylist.size()) {
							if (sPlayset.insert(strPlaylist).second) {
								SendDlgItemMessageA(hwndDlg, IDC_PLAYLIST, CB_ADDSTRING, 0, (LPARAM)strPlaylist.c_str());
							}
							EnableWindow(GetDlgItem(hwndDlg, IDC_PLAYLIST), FALSE);
							EnableWindow(GetDlgItem(hwndDlg, IDC_DELETE), FALSE);
							EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), TRUE);

							nPlayitem = 0, nPlaylen = 0;
							PlayItem();
						} else {
							std::wstring msg, title;

							lua_getglobal(g_luaVM, "MsgPlaylistEmpty");
							msg = luaL_towstr(g_luaVM, -1);
							lua_pop(g_luaVM, 1);

							lua_getglobal(g_luaVM, "TitlePlaylist");
							title = luaL_towstr(g_luaVM, -1);
							lua_pop(g_luaVM, 1);

							BaloonShow(hwndDlg, IDC_PLAYLIST, msg, title, 2, RGB(0, 0, 0));
						}
					} else { // Pause
						if (wDeviceID.size()) {
							if (pause) {
								MCI_GENERIC_PARMS mci;
								mci.dwCallback = MAKELONG(hwndDlg, 0);
								_VERIFY(!mciSendCommand(*wDeviceID.begin(), MCI_RESUME, MCI_WAIT, (DWORD_PTR)&mci));
								pause = false;
							} else {
								MCI_GENERIC_PARMS mci;
								mci.dwCallback = MAKELONG(hwndDlg, 0);
								_VERIFY(!mciSendCommand(*wDeviceID.begin(), MCI_PAUSE, MCI_WAIT, (DWORD_PTR)&mci));
								pause = true;
							}
						}
					}
					break;
				}

			case IDC_STOP:
				{
					if (nPlayitem >= 0) { // Playing
						if (wDeviceID.size()) {
							MCI_GENERIC_PARMS mci;
							mci.dwCallback = MAKELONG(hwndDlg, 0);
							_VERIFY(!mciSendCommand(*wDeviceID.begin(), MCI_STOP, MCI_WAIT, (DWORD_PTR)&mci));
							pause = false;

							nPlayitem = -1, nPlaylen = 0;
							strPlaylist.clear();
							EnableWindow(GetDlgItem(hwndDlg, IDC_PLAYLIST), TRUE);
							EnableWindow(GetDlgItem(hwndDlg, IDC_DELETE), TRUE);
							EnableWindow(GetDlgItem(hwndDlg, IDC_STOP), FALSE);
						}
					}
					break;
				}

			case IDC_SKIPERR:
				{
					bSkipErr = IsDlgButtonChecked(hwndDlg, IDC_SKIPERR) == BST_CHECKED;
					break;
				}

			case IDC_SHOWPATH:
				{
					bShowPath = IsDlgButtonChecked(hwndDlg, IDC_SHOWPATH) == BST_CHECKED;
					InvalidateRect(hwndFiles, 0, TRUE);
					break;
				}

			case IDC_SHOWEXT:
				{
					bShowExt = IsDlgButtonChecked(hwndDlg, IDC_SHOWEXT) == BST_CHECKED;
					InvalidateRect(hwndFiles, 0, TRUE);
					break;
				}

			case IDC_DELETE:
				{
					LVITEM lvi;
					int index;
					while ((index = ListView_GetNextItem(hwndFiles, -1, LVNI_SELECTED)) != -1) {
						lvi.mask = LVIF_PARAM;
						lvi.iItem = index;
						lvi.iSubItem = 0;
						ListView_GetItem(hwndFiles, &lvi);
						std::string key = aFiles[lvi.lParam].key;
						aFiles.erase(lvi.lParam);
						aKeys.erase(key);
						ListView_DeleteItem(hwndFiles, index);
						lua_getglobal(g_luaVM, "KeyFiles");
						lua_pushnil(g_luaVM);
						lua_setfield(g_luaVM, -2, key.c_str());
						lua_pop(g_luaVM, 1);
					}
					break;
				}

			case IDC_OPEN:
				{
					static WCHAR szAcceptFile[4096], szTitleBuffer[MAX_PATH];
					static WCHAR szCustomFilter[256] = TEXT("");
					static std::wstring zero(L"\000", 1);

					std::wstring ExtFilter;
					int i = 1;
					lua_getglobal(g_luaVM, "ExtFilter");
					std::wstring str1, str2;
					do {
						lua_pushinteger(g_luaVM, i);
						lua_gettable(g_luaVM, -2);
						if (lua_istable(g_luaVM, -1)) {
							lua_pushinteger(g_luaVM, 1);
							lua_gettable(g_luaVM, -2);
							if (lua_isstring(g_luaVM, -1)) str1 = luaL_towstr(g_luaVM, -1);
							else break;
							lua_pop(g_luaVM, 1);
							lua_pushinteger(g_luaVM, 2);
							lua_gettable(g_luaVM, -2);
							if (lua_isstring(g_luaVM, -1)) str2 = luaL_towstr(g_luaVM, -1);
							else break;
							lua_pop(g_luaVM, 1);
							ExtFilter += str1 + zero + str2 + zero;
							i++;
						} else i = 0;
						lua_pop(g_luaVM, 1);
					} while (i);
					lua_pop(g_luaVM, 1);

					szAcceptFile[0] = 0;
					auto initdir = std::wstring(drive) + dir;
					OPENFILENAME ofn = {
						sizeof(OPENFILENAME), // lStructSize
						hwndDlg, // hwndOwner
						g_hApp, // hInstance
						ExtFilter.c_str(), // lpstrFilter
						szCustomFilter, // lpstrCustomFilter
						_countof(szCustomFilter), // nMaxCustFilter
						1, // nFilterIndex
						szAcceptFile, // lpstrFile
						_countof(szAcceptFile), // nMaxFile
						szTitleBuffer, // lpstrFileTitle
						_countof(szTitleBuffer), // nMaxFileTitle
						initdir.c_str(), // lpstrInitialDir
						nullptr, // lpstrTitle
						OFN_ALLOWMULTISELECT | OFN_EXPLORER |
						OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, // Flags
						0, // nFileOffset
						0, // nFileExtension
						TEXT("mp3"), // lpstrDefExt
						0, // lCustData
						nullptr, // lpfnHook
						nullptr // lpTemplateName
					};
					if (GetOpenFileName(&ofn)) {
						CKeyFile kf;
						if (ofn.nFileExtension) { // one file selected
							MakeKeyFile(ofn.lpstrFile, kf);
							InsertLine(kf);
						} else { // multiply files selected
							int pos = ofn.nFileOffset;
							do {
								MakeKeyFile(szAcceptFile + std::wstring(L"\\") + std::wstring(szAcceptFile + pos), kf);
								InsertLine(kf);
								pos += lstrlenW(szAcceptFile + pos) + 1;
							} while (szAcceptFile[pos]);
						}
					}
					break;
				}

			case IDC_SAVE:
				{
					break;
				}

			default: retval = FALSE;
			}
			break;
		}

	case WM_DROPFILES:
		{
			int count = DragQueryFile((HDROP)wParam, -1, 0, 0);
			CKeyFile kf;
			std::wstring buffer;
			for (int i = 0; i < count; i++) {
				int len = DragQueryFile((HDROP)wParam, i, 0, 0);
				buffer.resize(len, 0);
				DragQueryFile((HDROP)wParam, i, (WCHAR*)buffer.c_str(), len+1);
				if (!StrCmpIW(buffer.c_str() + buffer.size() - ExtPlaylist.size(), ExtPlaylist.c_str())) {
					std::ifstream is(buffer.c_str());
					std::string playlist;
					static char line[1024];
					while (!is.eof()) {
						is.getline(line, _countof(line));
						playlist += line;
					}
					if (sPlayset.insert(playlist).second) {
						SendDlgItemMessageA(hwndDlg, IDC_PLAYLIST, CB_ADDSTRING, 0, (LPARAM)playlist.c_str());
					}
				} else {
					MakeKeyFile(buffer, kf);
					InsertLine(kf);
				}
			}
			break;
		}

	case WM_NOTIFY:
		{
			NMHDR* pnmh = (NMHDR*)lParam;
			switch (pnmh->code)
			{
			case LVN_GETDISPINFO:
				{
					static WCHAR buffer[_MAX_PATH];
					LV_DISPINFO* pnmv = (LV_DISPINFO*)lParam;
					if (pnmh->idFrom == IDC_FILES)
					{
						if (pnmv->item.mask & LVIF_TEXT)
						{
							switch (pnmv->item.iSubItem)
							{
							case 0:
								{
									wcscpy_s(buffer, _countof(buffer), utf8_to_wchar(aFiles[pnmv->item.lParam].key).c_str());
									pnmv->item.pszText = buffer;
									pnmv->item.cchTextMax = lstrlen(buffer);
									break;
								}

							case 1:
								{
									std::wstring str;
									if (bShowPath) str += aFiles[pnmv->item.lParam].path;
									str += aFiles[pnmv->item.lParam].file;
									if (bShowExt) str += L"." + aFiles[pnmv->item.lParam].ext;
									wcscpy_s(buffer, _countof(buffer), str.c_str());
									pnmv->item.pszText = buffer;
									pnmv->item.cchTextMax = lstrlen(buffer);
									break;
								}
							}
						}
					}
					break;
				}

			case LVN_BEGINLABELEDIT:
				{
					NMLVDISPINFO* pdi = (NMLVDISPINFO*)lParam;
					if (pnmh->idFrom == IDC_FILES) {
						SendMessage(ListView_GetEditControl(hwndFiles), EM_LIMITTEXT, nMaxIdLength, 0);
						retval = FALSE;
					}
					break;
				}

			case LVN_ENDLABELEDIT:
				{
					NMLVDISPINFO* pdi = (NMLVDISPINFO*)lParam;
					if (pnmh->idFrom == IDC_FILES && pdi->item.pszText) {
						CKeyFile& kf = aFiles[pdi->item.lParam];
						std::string val(wchar_to_utf8(pdi->item.pszText), 0, min(nMaxIdLength, (size_t)lstrlen(pdi->item.pszText)));

						bool has = false;
						for each (MapKeyFile::value_type const& v in aFiles) {
							if (v.second.key == val) {
								has = true;
								break;
							}
						}

						if (!has && val != kf.key) {
							if (nPlayitem < 0) {
								lua_getglobal(g_luaVM, "KeyFiles");
								lua_pushnil(g_luaVM);
								lua_setfield(g_luaVM, -2, kf.key.c_str());
								auto kfs = wchar_to_utf8(kf.path + kf.file + L"." + kf.ext);
								lua_pushlstring(g_luaVM, kfs.c_str(), kfs.size());
								lua_setfield(g_luaVM, -2, val.c_str());
								lua_pop(g_luaVM, 1);

								aKeys.erase(aFiles[pdi->item.lParam].key);
								aFiles[pdi->item.lParam].key = val;
								aKeys[val] = pdi->item.lParam;
								retval = TRUE;
							} else {
								std::wstring msg, title;

								lua_getglobal(g_luaVM, "MsgFilesPlay");
								msg = luaL_towstr(g_luaVM, -1);
								lua_pop(g_luaVM, 1);

								lua_getglobal(g_luaVM, "TitleFiles");
								title = luaL_towstr(g_luaVM, -1);
								lua_pop(g_luaVM, 1);

								BaloonShow(hwndDlg, IDC_FILES, msg, title, 2, RGB(0, 0, 0));
							}
						}
					}
					retval = FALSE;
					break;
				}

			case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
					switch (pnmv->iSubItem)
					{
					case 0:
						{
							static int sign = -1;
							sign = -sign;
							ListView_SortItems(hwndFiles, CompareKeys, sign);
							break;
						}

					case 1:
						{
							static int sign = -1;
							sign = -sign;
							ListView_SortItems(hwndFiles, CompareFile, sign);
							break;
						}
					}
					break;
				}
			}
			break;
		}

	case WM_TIMER:
		{
			switch (wParam)
			{
			case IDT_BALOONPOP:
				{
					BaloonHide();
					break;
				}
			}
			break;
		}

	case WM_HELP:
		{
			HELPINFO* lphi = (HELPINFO*)lParam;
			// Lua response
			lua_State *L = g_luaVM;
			lua_pushcclosure(L, lua_errtrace, 0);
			lua_getmethod(L, "wmHelp");
			if (lua_isfunction(L, -1)) {
				lua_insert(L, -2);
				lua_pushinteger(L, lphi->iCtrlId);
				lua_pushinteger(L, lphi->MousePos.x);
				lua_pushinteger(L, lphi->MousePos.y);
				lua_pcall(L, 4, 0, -4-2);
			} else lua_pop(L, 2);
			lua_pop(L, 1);
			break;
		}

	case MM_MCINOTIFY:
		{
			if (wDeviceID.find((MCIDEVICEID)lParam) != wDeviceID.end()) {
				MCI_GENERIC_PARMS mci;
				mci.dwCallback = MAKELONG(hwndDlg, 0);
				_VERIFY(!mciSendCommand((MCIDEVICEID)lParam, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&mci));
				wDeviceID.erase((MCIDEVICEID)lParam);

				PlayItem();
			} else {
				_ASSERT(false); // no way to here
			}
			break;
		}

	default: retval = FALSE;
	}
	return retval;
}

//-----------------------------------------------------------------------------

int CALLBACK WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpszCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpszCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	INITCOMMONCONTROLSEX InitCtrls = {
		sizeof(INITCOMMONCONTROLSEX),
		ICC_WIN95_CLASSES |
		ICC_STANDARD_CLASSES |
		ICC_COOL_CLASSES |
		ICC_LINK_CLASS
	};
	_VERIFY(InitCommonControlsEx(&InitCtrls));

	g_hApp = hInstance;
	sExt.insert(L"wav");
	sExt.insert(L"mp3");
	sExt.insert(L"wma");
	sExt.insert(L"aif");
	sExt.insert(L"aiff");

	hwndBaloon = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, 0,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, g_hApp, 0);
	_ASSERT(hwndBaloon);
	SendMessage(hwndBaloon, TTM_SETMAXTIPWIDTH, 0, 300);

	int retval;
	try {
		// Set current directory to executable location
		GetModuleFileName(NULL, mpath, _countof(mpath));
		_wsplitpath_s(mpath, drive, _countof(drive), dir, _countof(dir), NULL, 0, NULL, 0);
		SetCurrentDirectory((std::wstring(drive)+dir).c_str());


		// Inits Lua virtual machine
		auto L = luaL_newstate();
		_ASSERT(L);
		luaL_openlibs(L);

		// insert "DevMode" indicator
		lua_pushboolean(L,
#ifdef _DEBUG
			true);
#else
			false);
#endif
		lua_setglobal(L, "devmode");
		// insert "Platform" string
		lua_pushstring(L,
#if defined(_M_IX86)
			"x86");
#elif defined(_M_AMD64)
			"amd64");
#elif defined(_M_IA64)
			"ia64");
#elif defined(_M_ARM)
			"arm");
#else
			"*");
#endif
		lua_setglobal(L, "platform");
		// insert "PtrSize" value
		lua_pushinteger(L, sizeof(size_t) * 8);
		lua_setglobal(L, "ptrsize");

		// register Lua data
		g_luaVM = L;
		lunareg_seismix(L);

		// insert itself to script
		if (luaL_dofile(L, "smeditor.lua")) {
			if (lua_isstring(L, -1)) {
				throw std::exception(lua_tostring(L, -1));
			}
			retval = EXIT_FAILURE;
		}
		else {
			int i = 1;
			lua_getglobal(L, "ExtAllowed");
			do {
				lua_pushinteger(L, i);
				lua_gettable(L, -2);
				if (lua_isstring(L, -1)) {
					sExt.insert(luaL_towstr(L, -1));
					i++;
				}
				else i = 0;
				lua_pop(L, 1);
			} while (i);
			lua_pop(L, 1);

			lua_getglobal(L, "ExtPlaylist");
			ExtPlaylist = luaL_towstr(L, -1);
			lua_pop(L, 1);

			lua_getglobal(L, "MaxIdLength");
			nMaxIdLength = (size_t)lua_tointeger(L, -1);
			lua_pop(L, 1);

			DialogBox(hInstance, MAKEINTRESOURCE(IDD_SMGUI), 0, DialogProc);
		}

		// Close Lua virtual machine
		lua_close(L);
		L = nullptr;

		retval = 0;
	}
	catch (std::exception& e) {
		MessageBoxA(0, format("%s\r\n%s", typeid(e).name(), e.what()).c_str(), "Unhandled Exception!", MB_OK | MB_ICONSTOP);
		retval = -1;
	}

	DestroyWindow(hwndBaloon);
	hwndBaloon = 0;

	return retval;
}

//-----------------------------------------------------------------------------

// The End.