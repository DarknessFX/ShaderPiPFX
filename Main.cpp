// FX - Adapted
//
//

#include "pch.h"
#include "FXApp.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow)
{
    FXApp fxApp(1280, 720, L"ShaderPiPFX");
	Wnd fxWnd;
	return fxWnd.Run(&fxApp, hInstance, lpCmdLine, nCmdShow);
}
