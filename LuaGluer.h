#ifndef __lua_gluer_h__
#define __lua_gluer_h__

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#define DECLARE_LUAMETHOD(methodname) int lua_##methodname(lua_State *L)
#define IMPLEMENT_LUAMETHOD(typename, methodname) int typename::lua_##methodname(lua_State *L)
#define IMPLEMENT_LUAFUNC(methodname) int lua_##methodname(lua_State *L)
#define RESPONSE_LUAMETHOD(methodname) {#methodname, &lua_##methodname}
#define RESPONSE_LUAFUNC(namespace, methodname) {#methodname, &namespace::lua_##methodname}
#define DECLARE_LUACLASS(T) T(lua_State* L); \
	static const char className[]; \
	static CLuaGluer<T>::RegType methods[];

template <typename T> class CLuaGluer
{
public:
	typedef int (T::*mfp)(lua_State* L);
	typedef struct {
		T* object;
		bool bAutodelete;
	} userdataType;
	typedef struct {const char* name; mfp mp;} RegType;

	static void Register(lua_State* L) {
		lua_newtable(L);
		int methods = lua_gettop(L);

		luaL_newmetatable(L, T::className);
		int metatable = lua_gettop(L);

		// store method table in globals so that
		// scripts can add functions written in Lua.
		lua_pushstring(L, T::className);
		lua_pushvalue(L, methods);
		lua_settable(L, LUA_GLOBALSINDEX);

		lua_pushliteral(L, "__metatable");
		lua_pushvalue(L, methods);
		lua_settable(L, metatable); // hide metatable from Lua getmetatable()

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, methods);
		lua_settable(L, metatable);

		lua_pushliteral(L, "__tostring");
		lua_pushcfunction(L, tostring_T);
		lua_settable(L, metatable);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gc_T);
		lua_settable(L, metatable);

		lua_newtable(L); // mt for method table
		int mt = lua_gettop(L);
		lua_pushliteral(L, "__call");
		lua_pushcfunction(L, new_T);
		lua_pushliteral(L, "new");
		lua_pushvalue(L, -2); // dup new_T function
		lua_settable(L, methods); // add new_T to method table
		lua_settable(L, mt); // mt.__call = new_T
		lua_setmetatable(L, methods);

		// fill method table with methods from class T
		for (RegType* l = T::methods; l->name; l++) {
			lua_pushstring(L, l->name);
			lua_pushlightuserdata(L, (void*)l);
			lua_pushcclosure(L, thunk, 1);
			lua_settable(L, methods);
		}

		lua_pop(L, 2); // drop metatable and method table
	}

	static void pushobject(lua_State* L, T* obj, bool autodelete = true) {
		userdataType* userData = static_cast<userdataType*>(lua_newuserdata(L, sizeof(userdataType)));
		userData->object = obj;
		userData->bAutodelete = autodelete;
		__if_exists(T::JAddRef) {
			if (autodelete)
				userData->object->JAddRef();
		}
		__if_exists(T::AddRef) {
			__if_not_exists(T::JAddRef) {
				if (autodelete)
					userData->object->AddRef();
			}
		}
		luaL_getmetatable(L, T::className); // lookup metatable in Lua registry
		lua_setmetatable(L, -2);
	}

	static void setglobal(lua_State* L, T* obj, const char* name, bool autodelete = true) {
		pushobject(L, obj, autodelete);
		lua_setglobal(L, name);
	}

private:
	CLuaGluer() {} // hide default constructor

	// member function dispatcher
	static int thunk(lua_State* L) {
		// stack has userdata, followed by method args
		userdataType* userData = static_cast<userdataType*>(luaL_checkudata(L, 1, T::className));
		T* obj = userData->object; // get 'self', or if you prefer, 'this'
		lua_remove(L, 1); // remove self so member function args start at index 1
		// get member function from upvalue
		RegType* l = static_cast<RegType*>(lua_touserdata(L, lua_upvalueindex(1)));
		return (obj->*(l->mp))(L); // call member function
	}

	// create a new T object and
	// push onto the Lua stack a userdata containing a pointer to T object
	static int new_T(lua_State* L) {
		lua_remove(L, 1); // use classname:new(), instead of classname.new()
		pushobject(L, new T(L), true);
		return 1; // userdata containing pointer to T object
	}

	static int gc_T(lua_State* L) {
		userdataType* userData = static_cast<userdataType*>(lua_touserdata(L, 1));
		if (userData->bAutodelete) {
			__if_exists(T::JRelease) {
				userData->object->JRelease();
			}
			__if_exists(T::Release) {
				__if_not_exists(T::JRelease) {
					userData->object->Release();
				}
			}
			__if_not_exists(T::JRelease) {
				__if_not_exists(T::Release) {
					delete userData->object; // call destructor for T object
				}
			}
		}
		return 0;
	}

	static int tostring_T (lua_State *L) {
		userdataType *ud = static_cast<userdataType*>(lua_touserdata(L, 1));
		lua_pushfstring(L, "%s (%p)", T::className, ud->object);
		return 1;
	}
};

template<class JT>
void lua_getmethod(lua_State* L, JT* obj, const char* method)
{
	CLuaGluer<JT>::pushobject(L, obj);
	lua_getfield(L, -1, method);
}

inline
void lua_getmethod(lua_State* L, const char* name, const char* method)
{
	lua_getglobal(L, name);
	lua_getfield(L, -1, method);
}

#endif // __lua_gluer_h__