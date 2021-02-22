// FX 
//
//

#include "pch.h"
#include "Wnd.h"
#include "FileWatcher.h"

// Indicates to hybrid graphics systems to prefer the discrete part by default
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

const DWORD dwStyle = WS_POPUP | WS_SYSMENU | WS_THICKFRAME | WS_DLGFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;

FXApp* fxApp = nullptr;
HANDLE hMutex = NULL;
HWND Wnd::m_hwnd = nullptr;
HINSTANCE Wnd::m_hInstance = nullptr;
WNDCLASSEX m_wc;
bool bImGuiLoaded = false;
int m_currentDPI = 96;  // Windows default DPI

int Wnd::Run(FXApp* pfxApp, HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// FX
	if (!Wnd::FXInit(lpCmdLine))	{ return false;	}
	if (!Wnd::VerifyCPU()) { return true; }
	if (!Wnd::FXCreateWindow(pfxApp, hInstance, nCmdShow)) { return true; }
	bImGuiLoaded = true;

	// Parse the command line parameters
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	pfxApp->ParseCommandLineArgs(argv, argc);
	LocalFree(argv);

    MSG msg = {};
	ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		if (bIsFileWatcher && bIsFileChanged)
		{
			fxApp->SetShaderFile(m_fwfileName);
			bIsFileChanged = false;
		}
		fxApp->OnTick();
	}

	bImGuiLoaded = false;
	if (bIsFileWatcher) 
	{
		FileWatcherStop();
	}
    fxApp->OnDestroy();
	DestroyWindow(m_hwnd);
	UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
    return static_cast<char>(msg.wParam);
}

bool Wnd::FXInit(LPWSTR lpCmdLine)
{
	//GetModuleFileName(NULL, Wnd::m_ExePath, MAX_PATH);

	hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, L"ShaderPiPFX.0");
	if (!hMutex) {
		hMutex = CreateMutex(0, 0, L"ShaderPiPFX.0");
	}
	else
	{
		HWND hWnd = FindWindow(0, L"ShaderPiPFX");
		SetForegroundWindow(hWnd);
		if (wcslen(lpCmdLine) != 0) {
			COPYDATASTRUCT cds;
			cds.cbData = wcslen(lpCmdLine) * 2;
			cds.lpData = lpCmdLine;
			SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&cds);
		}
		return false;
	}
	return true;
}
bool Wnd::FXCreateWindow(FXApp* pfxApp, HINSTANCE hInstance, int nCmdShow)
{
	fxApp = pfxApp;

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	ImGui_ImplWin32_EnableDpiAwareness();

	WNDCLASSEX wc = {
		sizeof(WNDCLASSEX),
		CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		WindowProc,
		0L,
		0L,
		hInstance,
		LoadIconW(hInstance, L"SHADERPIPFX_ICON"),
		LoadCursorW(nullptr, IDC_ARROW),
		(HBRUSH)(COLOR_BACKGROUND + 1),
		NULL,
		L"ShaderPiPFX_WindowClass",
		NULL
	};
	m_wc = wc;
	RegisterClassEx(&wc);

	int offset = 32;
	int fullWidth = GetSystemMetrics(SM_CXFULLSCREEN);;
	int fullHeight = GetSystemMetrics(SM_CYFULLSCREEN);
	int contentWidth = fullWidth / 4;
	int contentHeight = fullHeight / 4;

	HWND hwnd = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_ACCEPTFILES,
		wc.lpszClassName,
		fxApp->GetTitle(),
		dwStyle,
		fullWidth - contentWidth - offset,
		fullHeight - contentHeight,
		contentWidth,
		contentHeight,
		NULL,
		NULL,
		wc.hInstance,
		pfxApp);
	if (!hwnd)
		return false;
	Wnd::m_hwnd = hwnd;

	int iDpi = GetDpiForWindow(hwnd);
	contentWidth = MulDiv(contentWidth, iDpi, m_currentDPI);
	contentHeight = MulDiv(contentHeight, iDpi, m_currentDPI);
	m_currentDPI = iDpi;
	SetWindowPos(hwnd, NULL, fullWidth - contentWidth - offset, fullHeight - contentHeight, contentWidth, contentHeight, SWP_NOZORDER | SWP_NOACTIVATE);

	fxApp->SetDPI(m_currentDPI);
	fxApp->SetWidth(contentWidth);
	fxApp->SetHeight(contentHeight);
	fxApp->OnInit();

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Wnd::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool s_activated = false;
	static bool s_in_sizemove = false;
	static bool s_in_suspend = false;
	static bool s_minimized = false;
	static bool s_fullscreen = false;
	static bool s_blockdrag = false;

	// Mouse controls and states
	static bool bIsDragWindow = false;			// Disable drag window when deactivate or just become active.
	static bool bIsShiftHold = false;		  	// Used to transfer MouseClicks to Shader
	static bool bIsLeftClicked = false;  		// Save state of Mouse Left Click button
	static bool bIsRightClicked = false;  		// Save state of Mouse Right Click button

	static POINT curpos;
	static RECT border_thickness;
	const int bordertop_offset = 6;

	if (bImGuiLoaded && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
		return true;
	}
	if (bImGuiLoaded && fxApp->UIWantCapture()) {
		return true;
	}

	switch (msg)
	{
		case WM_CREATE:
		{
			SetRectEmpty(&border_thickness);
			if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_THICKFRAME)
			{
				//AdjustWindowRectEx(&border_thickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
				AdjustWindowRectExForDpi(&border_thickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL, m_currentDPI);
				border_thickness.left *= -1;
				border_thickness.top *= -1;

				// Revert thickframe to ThinBorder
				border_thickness.left -= 2;
				border_thickness.right -= 2;
				border_thickness.top -= 2;
				border_thickness.bottom -= 2;
			}
			MARGINS margins = { 0 };
			DwmExtendFrameIntoClientArea(hWnd, &margins);
			SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			break;
		}

		case WM_PAINT:
		{
			if (!s_in_sizemove)
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);

				RECT rc = ps.rcPaint;
				BP_PAINTPARAMS params = { sizeof(params), BPPF_NOCLIP | BPPF_ERASE };
				HDC memdc;
				HPAINTBUFFER hbuffer = BeginBufferedPaint(hdc, &rc, BPBF_TOPDOWNDIB, &params, &memdc);

				Wnd::PaintWndRgn(memdc, rc);

				BufferedPaintSetAlpha(hbuffer, &rc, 255);
				EndBufferedPaint(hbuffer, TRUE);

				EndPaint(hWnd, &ps);
				if (!bImGuiLoaded) {
					bImGuiLoaded = true;
				}
			}
			return 0;
		}

		case WM_MOVE:
			if (fxApp)
			{
				fxApp->OnWindowMoved();
			}
			break;

		case WM_SIZE:
			if (wParam == SIZE_MINIMIZED)
			{
				if (!s_minimized)
				{
					s_minimized = true;
					if (!s_in_suspend && fxApp)
					{
						fxApp->OnSuspending();
					}
					s_in_suspend = true;
				}
			}
			else if (s_minimized)
			{
				s_minimized = false;
				if (s_in_suspend && fxApp)
				{
					fxApp->OnResuming();
				}
				s_in_suspend = false;
			}
			else if (!s_in_sizemove && fxApp && bImGuiLoaded)
			{
				fxApp->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
			}
			return 0;

		case WM_DESTROY:
			bImGuiLoaded = false;
			PostQuitMessage(0);
			return 0;

		case WM_GETMINMAXINFO:
			if (lParam)
			{
				auto info = reinterpret_cast<MINMAXINFO*>(lParam);
				info->ptMinTrackSize.x = 320;
				info->ptMinTrackSize.y = 200;
			}
			break;

		case WM_POWERBROADCAST:
			switch (wParam)
			{
				case PBT_APMQUERYSUSPEND:
					if (!s_in_suspend && fxApp)
						fxApp->OnSuspending();
					s_in_suspend = true;
					return TRUE;

				case PBT_APMRESUMESUSPEND:
					if (!s_minimized)
					{
						if (s_in_suspend && fxApp)
							fxApp->OnResuming();
						s_in_suspend = false;
					}
					return TRUE;
			}
			break;

		// FX
		case WM_MOUSEMOVE:
		{
			POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (!bIsDragWindow)
			{
				fxApp->OnMouseEvent(pos.x, pos.y, bIsLeftClicked, bIsRightClicked);
				curpos = pos;
			}

			if (bIsDragWindow && s_activated)
			{
				int windowWidth, windowHeight;
				RECT mainWindowRect;

				GetWindowRect(hWnd, &mainWindowRect);
				windowHeight = mainWindowRect.bottom - mainWindowRect.top;
				windowWidth = mainWindowRect.right - mainWindowRect.left;

				ClientToScreen(hWnd, &pos);
				MoveWindow(hWnd, pos.x - curpos.x, pos.y - curpos.y, windowWidth, windowHeight, TRUE);
			}
			break;
		}

		case WM_LBUTTONDOWN:
		{
			if (bIsShiftHold)
			{
				bIsLeftClicked = true;
				fxApp->OnMouseEvent(curpos.x, curpos.y, bIsLeftClicked, bIsRightClicked);
				break;
			}
			if (!s_fullscreen && s_activated && !s_blockdrag) {
				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				if (DragDetect(hWnd, pt))
				{
					curpos = pt;
					bIsDragWindow = true;
					SetCapture(hWnd);
				}
			}
			if (s_blockdrag) {
				s_blockdrag = false;
			}
			return 0;
		}

		case WM_LBUTTONUP:
			bIsLeftClicked = false;
			fxApp->OnMouseEvent(curpos.x, curpos.y, bIsLeftClicked, bIsRightClicked);

			if (bIsDragWindow) {
				ReleaseCapture();
				bIsDragWindow = false;
			}
			break;

		case WM_LBUTTONDBLCLK:
		{
			if (bIsShiftHold)
			{
				fxApp->OnMouseEvent(curpos.x, curpos.y, 2, bIsRightClicked);
				break;
			}

			WINDOWPLACEMENT wp;
			wp.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hWnd, &wp);
			if (wp.showCmd != SW_MAXIMIZE) {
				wp.showCmd = SW_MAXIMIZE;
				SetWindowPlacement(hWnd, &wp);
			}
			else
			{
				wp.showCmd = SW_SHOWNORMAL;
				SetWindowPlacement(hWnd, &wp);
			}
			s_fullscreen = !s_fullscreen;
			break;
		}

		case WM_RBUTTONDOWN:
		{
			if (bIsShiftHold)
			{
				bIsRightClicked = true;
				fxApp->OnMouseEvent(curpos.x, curpos.y, bIsLeftClicked, bIsRightClicked);
			}
			break;
		}

		case WM_RBUTTONUP:
		{
			bIsRightClicked = false;
			fxApp->OnMouseEvent(curpos.x, curpos.y, bIsLeftClicked, bIsRightClicked);
			if (bIsShiftHold)
			{
				break;
			}

			long dwExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
			if ((dwExStyle & WS_EX_TOPMOST))
			{
				dwExStyle = dwExStyle & ~WS_EX_TOPMOST;
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);

				border_thickness.left += 2;
				border_thickness.right += 2;
				border_thickness.top += 2;
				border_thickness.bottom += 2;
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
				return 0;
			}
			else
			{
				dwExStyle = dwExStyle | WS_EX_TOPMOST;
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);

				border_thickness.left -= 2;
				border_thickness.right -= 2;
				border_thickness.top -= 2;
				border_thickness.bottom -= 2;
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
				return 0;
			}
			break;
		}

		case WM_RBUTTONDBLCLK:
		{
			if (bIsShiftHold)
			{
				fxApp->OnMouseEvent(curpos.x, curpos.y, bIsLeftClicked, 2);
				break;
			}
		}

		case WM_SYSCOMMAND:
			// Disable ALT+SPACE window menu
			if ((wParam & 0xfff0) == SC_KEYMENU)
				return 0;
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			bool bIsWndKey = false;
			switch (wParam) {
				case VK_ESCAPE:
					//SHIFT+ESC = EXIT
					if (GetKeyState(VK_SHIFT)) {
						PostQuitMessage(0);
						bIsWndKey = true;
						return 0;
					}
					break;

				case VK_RETURN:
					// ALT+ENTER fullscreen toggle
					if (GetKeyState(VK_MENU))
					{
						WINDOWPLACEMENT wp;
						wp.length = sizeof(WINDOWPLACEMENT);
						GetWindowPlacement(hWnd, &wp);
						if (wp.showCmd != SW_MAXIMIZE)
						{
							wp.showCmd = SW_MAXIMIZE;
							SetWindowPlacement(hWnd, &wp);
						}
						else
						{
							wp.showCmd = SW_SHOWNORMAL;
							SetWindowPlacement(hWnd, &wp);
						}
						s_fullscreen = !s_fullscreen;
						bIsWndKey = true;
					}
					break;

				case VK_F4:
					// Catch ALT+F4 before fxApp.OnKeyDown
					if (GetKeyState(VK_MENU))
					{
						bIsWndKey = true;
						break;
					}

				case VK_SHIFT:
					if (!bIsDragWindow) 
					{
						bIsShiftHold = true;
						break;
					}
			}
			if (fxApp && !bIsWndKey)
			{
				fxApp->OnKeyDown(static_cast<UINT8>(wParam));
				return 0;
			}
			break;
		}

		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			switch (wParam) {
				case VK_SHIFT:
					bIsShiftHold = false;
					break;
			}
			if (fxApp)
			{
				fxApp->OnKeyUp(static_cast<UINT8>(wParam));
				return 0;
			}
			break;
		}
		case WM_DROPFILES:
			Wnd::OnDropFile(wParam);
			break;

		case WM_COPYDATA:
		{
			COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
			if (pcds->dwData != 0)
			{
				const LPWSTR lpCmdLine = (LPWSTR)(pcds->lpData);
				OutputDebugString(lpCmdLine);
			}
			break;
		}

		case WM_ACTIVATE:
			if (fxApp)
			{
				if (wParam)
				{
					fxApp->OnActivated();
					s_activated = true;
				}
				else
				{
					fxApp->OnDeactivated();
					s_activated = false;
					s_blockdrag = true;
				}
				return 0;
			}
			break;

		case WM_NCCALCSIZE:
			if (lParam)
			{
				NCCALCSIZE_PARAMS* sz = (NCCALCSIZE_PARAMS*)lParam;
				sz->rgrc[0].top -= bordertop_offset;
				sz->rgrc[0].left += border_thickness.left;
				sz->rgrc[0].right -= border_thickness.right;
				sz->rgrc[0].bottom -= border_thickness.bottom;
				return 0;
			}
			break;

		case WM_NCHITTEST:
		{
			//Allow resizing from top-border
			LRESULT result = DefWindowProc(hWnd, msg, wParam, lParam);
			if (result == HTCLIENT)
			{
				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				ScreenToClient(hWnd, &pt);
				if ((pt.y - bordertop_offset) <= border_thickness.top) return HTTOP;
			}
			return result;
		}

		case WM_DPICHANGED:
			Wnd::OnDPIChanged(LOWORD(wParam));
			return 0;

		case WM_MENUCHAR:
			return MAKELRESULT(0, MNC_CLOSE);

	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


void Wnd::OnDropFile(WPARAM wParam)
{
	TCHAR szName[MAX_PATH];
	HDROP hDrop = (HDROP)wParam;

	int count = DragQueryFile(hDrop, 0xFFFFFFFF, szName, MAX_PATH);
	for (int i = 0; i < count; i++)
	{
		DragQueryFile(hDrop, i, szName, MAX_PATH);
		std::wstring wsName = szName;
		std::wstring wsExt = wsName.substr(wsName.length() - 4, 4);
		std::wstring wsHLSLExt = L"hlsl";

		if (wsExt == wsHLSLExt)
		{
			fxApp->SetShaderFile(wsName);
		}
	}
	DragFinish(hDrop);
}

void Wnd::PaintWndRgn(HDC hdc, RECT rc)
{
	HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
	FillRect(hdc, &rc, brush);
	DeleteObject(brush);
}

bool Wnd::VerifyCPU()
{
// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	if (!DirectX::XMVerifyCPUSupport()) {
		::MessageBoxA(nullptr, "CPU don't support DirectX Math.\nXMVerifyCPUSupport failed.", "ShaderPiPFX", MB_OK | MB_ICONERROR);
		return false;
	}

	HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
	if (FAILED(hr)) {
		::MessageBoxA(nullptr, "CPU Multithread error.\nCoInitializeEx failed.", "ShaderPiPFX", MB_OK | MB_ICONERROR);
		return false;
	}
	CoUninitialize();
	return true;
}

void Wnd::OnDPIChanged(int newDpi) {
	m_currentDPI = newDpi;
}

void Wnd::OnFileLoad(std::wstring inFileName)
{
	if (bIsFileWatcher)
	{
		FileWatcherStop();
	}
	FileWatcherStart(inFileName);
}

