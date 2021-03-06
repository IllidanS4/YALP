#include "tags.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>
#include <cstring>

static std::unordered_map<AMX*, std::weak_ptr<struct amx_tag_info>> amx_map;

struct amx_tag_info
{
	AMX *amx;

	lua_State *L;
	int self;
	int taglist;

	amx_tag_info(lua_State *L, AMX *amx) : L(L), amx(amx)
	{

	}

	~amx_tag_info()
	{
		if(amx)
		{
			amx_map.erase(amx);
			amx = nullptr;
		}
	}
};

static bool getudatatag(lua_State *L, int idx, const char *&tagname)
{
	if(lua_getmetatable(L, idx))
	{
		if(lua_getfield(L, -1, "__tag") == LUA_TSTRING)
		{
			tagname = lua_tostring(L, -1);
			lua_pop(L, 2);
			return true;
		}
		lua_pop(L, 2);
	}
	return false;
}

static int tagof(lua_State *L)
{
	int num = lua_gettop(L);

	for(int i = 1; i <= num; i++)
	{
		const char *tagname;
		if(lua::isstring(L, i))
		{
			tagname = lua_tostring(L, i);
		}else if(lua_isinteger(L, i))
		{
			tagname = "";
		}else if(lua_isboolean(L, i))
		{
			tagname = "bool";
		}else if(lua::isnumber(L, i))
		{
			tagname = "Float";
		}else if(lua_istable(L, i))
		{
			lua_len(L, i);
			int isnum;
			int len = (int)lua_tointegerx(L, -1, &isnum);
			lua_pop(L, 1);
			if(!isnum || len < 0)
			{
				return lua::argerror(L, i, "invalid length");
			}
			if(len == 0)
			{
				return lua::argerror(L, i, "cannot obtain tag of table (zero length)");
			}
			tagname = nullptr;
			for(int j = 1; j <= len; j++)
			{
				const char *elemtagname;
				lua_pushinteger(L, j);
				switch(lua_gettable(L, i))
				{
					case LUA_TNUMBER:
						elemtagname = lua_isinteger(L, -1) ? "" : "Float";
						break;
					case LUA_TBOOLEAN:
						elemtagname = "bool";
						break;
					default:
						if(!getudatatag(L, -1, tagname))
						{
							return lua::argerror(L, i, "table index %d: cannot obtain tag of %s", j, luaL_typename(L, -1));
						}
						break;
				}
				if(tagname == nullptr)
				{
					tagname = elemtagname;
				}else if(std::strcmp(tagname, elemtagname) != 0)
				{
					return lua::argerror(L, i, "table index %d: tag of %s is not %s", j, luaL_typename(L, -1), tagname);
				}
				lua_pop(L, 1);
			}
		}else if(!getudatatag(L, i, tagname))
		{
			return lua::argerror(L, i, "cannot obtain tag of %s", luaL_typename(L, i));
		}

		if(!tagname[0])
		{
			lua_pushlightuserdata(L, reinterpret_cast<void*>(0x80000000));
			lua_replace(L, i);
			continue;
		}
		size_t maxlen = (size_t)lua_tointeger(L, lua_upvalueindex(2));
		if(std::strlen(tagname) > maxlen)
		{
			return lua::argerror(L, i, "tag name exceeds %d characters", maxlen);
		}
		if(lua_getfield(L, lua_upvalueindex(1), tagname) == LUA_TLIGHTUSERDATA)
		{
			lua_replace(L, i);
			continue;
		}
		lua_pop(L, 1);
		lua_pushstring(L, tagname);
		lua_pushvalue(L, -1);
		int index = luaL_ref(L, lua_upvalueindex(1));
		index |= 0x80000000;
		if(tagname[0] >= 'A' && tagname[0] <= 'Z')
		{
			index |= 0x40000000;
		}
		lua_pushlightuserdata(L, reinterpret_cast<void*>(index));
		lua_settable(L, lua_upvalueindex(1));
		lua_pushlightuserdata(L, reinterpret_cast<void*>(index));
		lua_replace(L, i);
	}
	return num;
}

static int tagname(lua_State *L)
{
	int num = lua_gettop(L);

	for(int i = 1; i <= num; i++)
	{
		cell tag_id = reinterpret_cast<cell>(lua::checklightudata(L, i));
		if(tag_id == 0x80000000 || tag_id == 0)
		{
			lua::pushliteral(L, "");
			lua_replace(L, i);
			continue;
		}
		if(tag_id & 0x80000000)
		{
			int index = tag_id & 0x3FFFFFFF;
			if(lua_rawgeti(L, lua_upvalueindex(1), index) == LUA_TSTRING)
			{
				auto str = lua_tostring(L, -1);
				if((str[0] >= 'A' && str[0] <= 'Z') == ((tag_id & 0x40000000) != 0))
				{
					lua_replace(L, i);
					continue;
				}
			}
			lua_pop(L, 1);
		}
		lua_pushnil(L);
		lua_replace(L, i);
	}
	return num;
}

void lua::interop::init_tags(lua_State *L, AMX *amx, const std::unordered_map<cell, std::string> &init)
{
	int table = lua_absindex(L, -1);

	auto info = std::make_shared<amx_tag_info>(lua::mainthread(L), amx);
	amx_map[amx] = info;
	lua::pushuserdata(L, info);

	luaL_checkstack(L, init.size() + 4, nullptr);
	lua_createtable(L, init.size(), 0);
	for(const auto &pair : init)
	{
		int index = pair.first & 0x3FFFFFFF;
		lua_pushlstring(L, pair.second.c_str(), pair.second.size());
		lua_pushvalue(L, -1);
		lua_rawseti(L, -3, index);
		lua_pushlightuserdata(L, reinterpret_cast<void*>(pair.first | 0x80000000));
		lua_settable(L, -3);
	}

	lua_pushvalue(L, -1);
	int maxlen;
	amx_NameLength(amx, &maxlen);
	lua_pushinteger(L, maxlen);
	lua_pushcclosure(L, tagof, 2);
	lua_setfield(L, table, "tagof");

	lua_pushvalue(L, -1);
	lua_pushcclosure(L, tagname, 1);
	lua_setfield(L, table, "tagname");


	info->taglist = luaL_ref(L, LUA_REGISTRYINDEX);

	info->self = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool gettaglist(lua_State *L, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::interop::amx_find_tag_id(AMX *amx, cell tag_id, char *tagname)
{
	if(tagname && (tag_id & 0x80000000) == 0 && tag_id != 0)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(gettaglist(L, info->taglist))
				{
					int index = tag_id & 0x3FFFFFFF;
					if(lua_rawgeti(L, -1, index) == LUA_TSTRING)
					{
						auto str = lua_tostring(L, -1);
						if((str[0] >= 'A' && str[0] <= 'Z') == ((tag_id & 0x40000000) != 0))
						{
							std::strcpy(tagname, str);
							lua_pop(L, 2);
							return true;
						}
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_get_tag(AMX *amx, int index, char *tagname, cell *tag_id)
{
	if(tagname || tag_id)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(gettaglist(L, info->taglist))
				{
					index += 1;
					if(lua_rawgeti(L, -1, index) == LUA_TSTRING)
					{
						auto str = lua_tostring(L, -1);
						if(tagname)
						{
							std::strcpy(tagname, str);
						}
						if(tag_id)
						{
							*tag_id = index | (str[0] >= 'A' && str[0] <= 'Z' ? 0x40000000 : 0);
						}
						lua_pop(L, 2);
						return true;
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_num_tags(AMX *amx, int *number)
{
	if(number)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(gettaglist(L, info->taglist))
				{
					*number = lua_rawlen(L, -1);
					lua_pop(L, 1);
					return true;
				}
			}
		}
	}
	return false;
}
