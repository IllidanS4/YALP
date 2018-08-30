#include "natives.h"
#include "lua_api.h"
#include "lua_utils.h"
//#include <malloc.h>

namespace Natives
{
	// native Lua:lua_newstate(lua_lib:load=lua_baselibs, lua_lib:preload=lua_newlibs);
	static cell AMX_NATIVE_CALL lua_newstate(AMX *amx, cell *params)
	{
		auto L = luaL_newstate();
		if(L)
		{
			lua::init(L, optparam(1, 0xCF), optparam(2, 0x1C00));
		}
		return reinterpret_cast<cell>(L);
	}

	// native lua_dostring(Lua:L, str[]);
	static cell AMX_NATIVE_CALL lua_dostring(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);

		char *str;
		amx_StrParam(amx, params[2], str);
		
		return luaL_dostring(L, str);
	}

	// native bool:lua_close(Lua:L);
	static cell AMX_NATIVE_CALL lua_close(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		lua_close(L);
		return 1;
	}

	// native lua_pcall(Lua:L, nargs, nresults, errfunc=0);
	static cell AMX_NATIVE_CALL _lua_pcall(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);

		return lua_pcall(L, params[2], params[3], optparam(4, 0));
	}

	// native lua_call(Lua:L, nargs, nresults);
	static cell AMX_NATIVE_CALL _lua_call(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);

		switch(lua_pcall(L, params[2], params[3], 0))
		{
			case LUA_OK:
				break;
			case LUA_ERRMEM:
				logprintf("%s", lua_tostring(L, -1));
				lua_pop(L, 1);
				amx_RaiseError(amx, AMX_ERR_MEMORY);
				break;
			default:
				logprintf("%s", lua_tostring(L, -1));
				lua_pop(L, 1);
				amx_RaiseError(amx, AMX_ERR_GENERAL);
				break;
		}
		return 0;
	}

	// native lua_load(Lua:L, const reader[], data, bufsize, chunkname[]="");
	static cell AMX_NATIVE_CALL lua_load(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);

		const char *reader;
		amx_StrParam(amx, params[2], reader);

		cell data = params[3];
		cell cellsize = params[4];
		if(cellsize < 0) cellsize = 128;

		const char *chunkname;
		amx_OptStrParam(amx, 5, chunkname, nullptr);

		int error;

		cell buffer, *addr;
		error = amx_Allot(amx, cellsize, &buffer, &addr);
		if(error != AMX_ERR_NONE)
		{
			amx_RaiseError(amx, error);
			return 0;
		}
		
		int index;
		error = amx_FindPublic(amx, reader, &index);
		if(error != AMX_ERR_NONE)
		{
			amx_RaiseError(amx, error);
			return 0;
		}
		
		bool last = false;
		int result = lua::load(L, [&](lua_State *L, size_t *size)
		{
			if(last)
			{
				*size = 0;
			}else{
				amx_Push(amx, cellsize);
				amx_Push(amx, data);
				amx_Push(amx, buffer);
				amx_Push(amx, reinterpret_cast<cell>(L));
				cell rsize;
				if(amx_Exec(amx, &rsize, index) == AMX_ERR_NONE)
				{
					if(rsize < 0)
					{
						*size = -rsize;
						last = true;
					}else{
						*size = rsize;
					}
				}else{
					*size = 0;
				}
			}
			return reinterpret_cast<const char*>(addr);
		}, chunkname, nullptr);

		amx_Release(amx, buffer);

		return result;
	}

	// native lua_stackdump(Lua:L, depth=-1);
	static cell AMX_NATIVE_CALL lua_stackdump(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		int top = lua_gettop(L);
		int bottom = 1;
		cell depth = optparam(2, -1);
		if(depth >= 0 && depth <= top) bottom = top - depth + 1;
		bool tostring = lua_getglobal(L, "tostring") == LUA_TFUNCTION;
		if(!tostring) lua_pop(L, 1);
		for(int i = top; i >= bottom; i--)
		{
			if(!tostring)
			{
				if(auto str = lua_tostring(L, i))
				{
					logprintf("%s", str);
				}else{
					logprintf("%s", luaL_typename(L, i));
				}
			}else{
				lua_pushvalue(L, -1);
				lua_pushvalue(L, i);
				lua_pcall(L, 1, 1, 0);
				if(auto str = lua_tostring(L, -1))
				{
					logprintf("%s", str);
				}else{
					logprintf("%s", luaL_typename(L, i));
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
		return 0;
	}

	static cell AMX_NATIVE_CALL lua_loopback(AMX *amx, cell *params)
	{
		int index, error;
		error = amx_FindPublic(amx, "#lua", &index);
		if(error != AMX_ERR_NONE)
		{
			amx_RaiseError(amx, error);
			return 0;
		}
		size_t numargs = params[0] / sizeof(cell);
		for(size_t i = numargs; i >= 1; i--)
		{
			error = amx_Push(amx, params[i]);
			if(error != AMX_ERR_NONE)
			{
				amx_RaiseError(amx, error);
				return 0;
			}
		}
		cell retval;
		error = amx_Exec(amx, &retval, index);
		if(error != AMX_ERR_NONE)
		{
			amx_RaiseError(amx, error);
		}
		return retval;
	}

	static cell AMX_NATIVE_CALL lua_gettop(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		return ::lua_gettop(L);
	}

	static cell AMX_NATIVE_CALL _lua_tostring(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		int idx = lua_absindex(L, params[2]);
		size_t len;
		auto str = lua_tolstring(L, idx, &len);
		bool pop = false;
		if(!str)
		{
			if(lua_getglobal(L, "tostring") == LUA_TFUNCTION)
			{
				lua_pushvalue(L, idx);
				lua_pcall(L, 1, 1, 0);
				str = lua_tolstring(L, -1, &len);
				pop = true;
			}else{
				str = luaL_typename(L, idx);
				len = std::strlen(str);
			}
		}
		
		cell *addr;
		amx_GetAddr(amx, params[3], &addr);

		amx_SetString(addr, str, true, false, params[4]);

		if(pop)
		{
			lua_pop(L, 1);
		}

		return len;
	}

	static cell AMX_NATIVE_CALL lua_settop(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		::lua_settop(L, params[2]);
		return 0;
	}
}

static AMX_NATIVE_INFO native_list[] =
{
	AMX_DECLARE_NATIVE(lua_newstate),
	AMX_DECLARE_NATIVE(lua_dostring),
	AMX_DECLARE_NATIVE(lua_close),
	AMX_DECLARE_NATIVE(lua_load),
	{"lua_pcall", Natives::_lua_pcall},
	{"lua_call", Natives::_lua_call},
	AMX_DECLARE_NATIVE(lua_stackdump),
	AMX_DECLARE_NATIVE(lua_loopback),
	AMX_DECLARE_NATIVE(lua_gettop),
	{"lua_tostring", Natives::_lua_tostring},
	AMX_DECLARE_NATIVE(lua_settop),
};

int RegisterNatives(AMX *amx)
{
	return amx_Register(amx, native_list, sizeof(native_list) / sizeof(*native_list));
}
