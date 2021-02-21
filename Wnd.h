// FX - Adapted
// 
// Original source code from:
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).

#pragma once

#include "FXApp.h"

class FXApp;

class Wnd
{
public:
    int Run(FXApp* pfxApp, HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow);

    static HWND GetHwnd()			{ return m_hwnd; }
    static HINSTANCE GetIntance()	{ return m_hInstance; }
	static void OnFileLoad(std::wstring inFileName);

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	bool FXInit(LPWSTR lpCmdLine);
	bool FXCreateWindow(FXApp* pfxApp, HINSTANCE hInstance, int nCmdShow);
	static void OnDPIChanged(int newDpi);
	static void OnDropFile(WPARAM wParam);
	static void PaintWndRgn(HDC hdc, RECT rc);
	bool VerifyCPU();
	static HWND m_hwnd;
	static HINSTANCE m_hInstance;
};
