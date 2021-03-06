#ifndef NATIVE_H_INCLUDED
#define NATIVE_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_native(lua_State *L, AMX *amx);
		void amx_register_natives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);
		bool amx_get_param_addr(AMX *amx, cell amx_addr, cell **phys_addr);
		void amx_unregister_natives(AMX *amx);
		AMX_NATIVE find_native(AMX *amx, const char *native);
	}
}

#endif
