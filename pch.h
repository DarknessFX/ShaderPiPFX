// FX
// 
//

#pragma once
#pragma warning( disable : 4267 4244 )

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include <d3d12.h>
#include <dxgi1_6.h>
//#include <D3Dcompiler.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <tchar.h>
#include <sstream>
#include <wrl.h>
#include <shellapi.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include "dwmapi.h"
#pragma comment(lib, "dwmapi.lib") 
#include <Uxtheme.h>
#pragma comment(lib, "UxTheme.lib")

#include "DXHelper.h"

#include "fileapi.h"
#include <shobjidl.h> 

#include <ctime>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
