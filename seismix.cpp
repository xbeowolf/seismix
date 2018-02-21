
//-----------------------------------------------------------------------------

//
// Includes
//

#pragma region Includes

// C/C++ RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// STL
#include <iostream>

// Windows
#include <windows.h>
#include <mmsystem.h>

// Common
#include "AssertMsg.h"

// Project
#include "seismix.h"

#pragma endregion

//-----------------------------------------------------------------------------

using namespace seismix;

//-----------------------------------------------------------------------------

MCIDEVICEID wDeviceID = 0;

IMPLEMENT_LUAMETHOD(seismix, play) {
	ASSERT(lua_isstring(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1));
	auto file = lua_tostring(L, -3);
	auto from = (DWORD)lua_tointeger(L, -2);
	auto to = (DWORD)lua_tointeger(L, -1);
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
		err = mciSendCommand(mci1.wDeviceID, MCI_PLAY, MCI_WAIT | (from > 0 ? MCI_FROM : 0) | (to > 0 ? MCI_TO : 0), (DWORD_PTR)&mci2);
		if (!err) {
			wDeviceID = mci1.wDeviceID;
		}
		MCI_GENERIC_PARMS mci3;
		mci3.dwCallback = 0;
		mciSendCommand(mci1.wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&mci3);
	}
	lua_pushnumber(L, err);
	return 1;
}

IMPLEMENT_LUAMETHOD(seismix, send) {
	ASSERT(lua_isstring(L, -1));
	auto str = lua_tostring(L, -1);
	MCIERROR err = mciSendStringA(str, 0, 0, 0);
	lua_pushnumber(L, err);
	return 1;
}

IMPLEMENT_LUAMETHOD(seismix, pause) {
	ASSERT(lua_isnumber(L, -1));
	auto pause = (DWORD)lua_tointeger(L, -1);
	Sleep(pause);
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
	luaL_newlib(L, methods1);
	lua_setglobal(L, "mci");
}

//-----------------------------------------------------------------------------

int __cdecl main(int argc, char* argv[]) {
	int retval = EXIT_SUCCESS;
	// Set current directory to executable location
#ifndef _DEBUG
	{
		TCHAR mpath[_MAX_PATH];
		TCHAR drive[_MAX_DRIVE];
		TCHAR dir[_MAX_DIR];
		GetModuleFileName(nullptr, mpath, _countof(mpath));
		_tsplitpath_s(mpath, drive, _countof(drive), dir, _countof(dir), nullptr, 0, nullptr, 0);
		SetCurrentDirectory((std::tstring(drive) + dir).c_str());
	}
#endif

	std::string playlist;
	for (int i = 1; i < argc; i++) {
		if (playlist.size()) playlist += " ";
		playlist += argv[i];
	}

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
	lunareg_seismix(L);

	// insert itself to script
	if (luaL_dofile(L, "seismix.lua")) {
		if (lua_isstring(L, -1)) {
			std::cout << lua_tostring(L, -1) << std::endl;
		}
		retval = EXIT_FAILURE;
	}
	else {
		ASSERT(lua_gettop(L) == 0); // Lua stack must be empty

		// Call Lua event
		lua_getglobal(L, "main");
		if (lua_isfunction(L, -1)) {
			lua_pushlstring(L, playlist.c_str(), playlist.size());
			if (lua_pcall(L, 1, 0, 0)) {
				std::cout << lua_tostring(L, -1) << std::endl;
				retval = EXIT_FAILURE;
			}
		}
		else lua_pop(L, 1);
	}

	// Close Lua virtual machine
	lua_close(L);
	L = nullptr;

	return retval;
}

//-----------------------------------------------------------------------------

// The End.