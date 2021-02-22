// FX
// 
// 

#pragma once

#include "Wnd.h"
#include "StepTimer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class FXApp
{
public:
	FXApp(UINT width, UINT height, std::wstring name);
	virtual ~FXApp();

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
	virtual void OnRenderUI();
	virtual void OnTick();
    virtual void OnDestroy();
	virtual void OnActivated();
	virtual void OnDeactivated();
	virtual void OnWindowMoved();
	virtual void OnWindowSizeChanged(int width, int height);
	virtual void OnSuspending();
	virtual void OnResuming();
	virtual void OnMouseEvent(int x, int y, int lClick, int rClick);

    virtual void OnKeyDown(UINT8 /*key*/)   {}
    virtual void OnKeyUp(UINT8 /*key*/)     {}

	virtual bool UIWantCapture();

	void SetShaderFile(std::wstring fileName);
	UINT GetWidth() const           { return m_width; }
    UINT GetHeight() const          { return m_height; }
    void SetWidth(int& width)       { m_width = width; 	 m_aspectRatio = float(width) / float(m_height); }
    void SetHeight(int& height)     { m_height = height; m_aspectRatio = float(m_width) / float(height) ;}
    void SetDPI(int& dpi)			{ m_dpi = dpi; }
    const WCHAR* GetTitle() const   { return m_title.c_str(); }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
	std::wstring GetAssetFullPath(LPCWSTR assetName);

	void GetHardwareAdapter(
		_In_ IDXGIFactory1* pFactory,
		_Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter = false);

	void SetCustomWindowText(LPCWSTR text);

	// Viewport dimensions.
	UINT m_width;
	UINT m_height;
	UINT m_dpi;
	float m_aspectRatio;

	// Adapter info.
	bool m_useWarpDevice;

private:
	enum EShaderType
	{
		PixelShader,
		VertexShader
	};

	// Structs
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT2 texuv0;
		XMFLOAT2 texuv1;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT3 bitangent;
	};

	struct ConstantValueBuffer
	{
		XMFLOAT4 resolution;
		float time;
		float timeDelta;
		INT frame;
		INT ticks;
		XMINT4 date;
		XMINT4 clock;
		XMINT4 mouse;
		INT random;
		INT perfHigh;
		INT perfLow;
	};

	struct Mouse
	{
		int x;
		int y;
		int lClick;
		int rClick;
	};


	// Forward declaration for internal functions
	void LoadPipeline();
	void LoadAssets();
	void LoadShaders();
	void LoadVertexBuffer();
	void LoadCommandList();
	void LoadUI();
	void ReloadAssets();
	void WaitForGpu();
	void WaitForNextFrame();
	void ResizeSwapChain(int width, int height);
	void CreateRenderTargets();
	void CleanupRenderTargets();
	void CleanupDeviceD3D();
	void MakeWindowAssociationWithLocatedFactory(ComPtr<IDXGISwapChain3> const& swapChain, HWND hWnd, UINT flags);
	void OpenFileDialog();
	bool CompileShader(ComPtr<IDxcBlobEncoding> pSource, EShaderType shaderType);

	// Internal vars
	UINT64 m_tickCounter				= 0;
	UINT64 m_frameCounter				= 0;
	bool bIsNextFrameReady				= true;
	bool bIsExiting						= false;
	bool bIsShaderFile					= false;
	bool bIsShaderError					= false;
	std::string							m_ShaderErrorMsg;

	time_t								m_tmNow;
	struct tm							m_tmLocalTime;
	UINT								m_tmLastSec;

	Mouse								m_mouse;

	// Root assets path.
	std::wstring m_assetsPath;

	// Window title.
	std::wstring m_title;

	// Shader File to monitor and work
	std::wstring m_shaderFile			= L"";
	std::string  m_shaderFileA			= "<- Click here to select HLSL file.";

    // Pipeline objects.
	static const UINT					FRAME_BUFFER = 3;
	UINT								m_frameIndex = 0;
	ComPtr<ID3D12Device8>				m_device;
	ComPtr<IDXGIAdapter1>				m_adapter;
	ComPtr<IDXGIFactory6>				m_factory;
	ComPtr<ID3D12CommandQueue>			m_commandQueue;
	ComPtr<ID3D12CommandAllocator>		m_commandAllocators[FRAME_BUFFER];
	ComPtr<ID3D12GraphicsCommandList5>	m_commandList;
	ComPtr<IDXGISwapChain3>				m_swapChain;
	HANDLE								m_swapChainWaitableObject = NULL;
	ComPtr<ID3D12Resource>				m_renderTargets[FRAME_BUFFER];
	D3D12_CPU_DESCRIPTOR_HANDLE			m_renderTargetsDescriptor[FRAME_BUFFER] = {};
	ComPtr<ID3D12RootSignature>			m_rootSignature;
	ComPtr<ID3DBlob>					m_signature;
	ComPtr<ID3DBlob>					m_error;
	ComPtr<ID3D12PipelineState>			m_pipelineState;
	ComPtr<ID3D12Fence1>				m_fence;
	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
	UINT								m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap>		m_srvHeap;
	UINT								m_srvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap>		m_dsvHeap;
	UINT								m_dsvDescriptorSize;
	ComPtr<ID3D12PipelineState>			m_depthState;
	ComPtr<ID3D12Resource>				m_depthStencil;
	HANDLE								m_fenceEvent = NULL;
	UINT64								m_fenceLastSignaledValue = 0;
	UINT64								m_fenceValues[FRAME_BUFFER];
	ComPtr<IDxcBlob>					m_pixelShader;
	ComPtr<IDxcBlob>					m_vertexShader;
	UINT								m_vertexCount;
	ComPtr<ID3D12Resource>				m_vertexBuffer;
	ComPtr<ID3D12Resource>				m_vertexBufferUploadHeap;
	D3D12_VERTEX_BUFFER_VIEW			m_vertexBufferView;
	ComPtr<ID3D12Resource>				m_indexBuffer;
	ComPtr<ID3D12Resource>				m_indexBufferUploadHeap;
	D3D12_INDEX_BUFFER_VIEW				m_indexBufferView;
	ComPtr<ID3D12Resource>				m_cbvBuffer;
	ComPtr<ID3D12DescriptorHeap>		m_cbvHeap;
	UINT								m_cbvDescriptorSize;
	ComPtr<ID3D12Resource>				m_cbvUploadHeap;
	ConstantValueBuffer					m_cbvData;
	UINT8*								m_cbvDataBegin;

	// TODO: Learn about other Resources and Samplers
	ComPtr<ID3D12DescriptorHeap>		m_samplerHeap;
	ComPtr<ID3D12Resource>				m_texture;
	ComPtr<ID3D12Resource>				m_textureUploadHeap;
	ComPtr<ID3D12PipelineState>			m_queryState;
	ComPtr<ID3D12DescriptorHeap>		m_queryHeap;
	ComPtr<ID3D12Resource>				m_queryResult;

	StepTimer							m_timer;
	CD3DX12_VIEWPORT					m_viewport;
	CD3DX12_RECT						m_scissorRect;

#if defined(_DEBUG)
	// [DEBUG] Enable debug interface
	ComPtr<ID3D12Debug>					m_debugController;
	ComPtr<IDXGIDebug1>					m_dxgiDebug;
#endif
};
