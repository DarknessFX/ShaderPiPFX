// FX 
// 
// 

#include "pch.h"
#include "FXApp.h"

using namespace Microsoft::WRL;

#pragma region Init
FXApp::~FXApp() { }

FXApp::FXApp(UINT width, UINT height, std::wstring name) :
	m_width(width),
	m_height(height),
	m_title(name),
	m_useWarpDevice(false),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_fenceValues {},
	m_rtvDescriptorSize(0),
	m_srvDescriptorSize(0),
	m_dsvDescriptorSize(0)
{
	WCHAR assetsPath[512];
	GetAssetsPath(assetsPath, _countof(assetsPath));
	m_assetsPath = assetsPath;

	m_aspectRatio = float(width) / float(height);

	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.f / 60.f);

	m_tmNow = time(0);
	localtime_s(&m_tmLocalTime, &m_tmNow);
	m_tmLastSec = m_tmNow;

	ThrowIfFailed(DXGIDeclareAdapterRemovalSupport());
}

void FXApp::OnInit()
{
	LoadPipeline();
	LoadAssets();
	LoadUI();
}
#pragma endregion

#pragma region Tick
void FXApp::OnTick()
{
	if (bIsExiting) { return; }

	m_tickCounter++;

	// One per frame
	if (bIsNextFrameReady)
	{
		bIsNextFrameReady = false;
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

		FXApp::OnUpdate();
		FXApp::OnRender();
		FXApp::OnRenderUI();

		// Present the frame.
		ThrowIfFailed(m_swapChain->Present(1, 0));
		WaitForNextFrame();
		bIsNextFrameReady = true;
	}
}

// Update frame-based values.
void FXApp::OnUpdate() 
{
	m_timer.Tick(NULL);
	m_frameCounter++;

	m_tmNow = time(0);
	if (m_tmLastSec != m_tmNow)
	{
		m_tmLastSec = m_tmNow;
		localtime_s(&m_tmLocalTime, &m_tmNow);
		m_tmLocalTime.tm_year = 1900 + m_tmLocalTime.tm_year;
		m_tmLocalTime.tm_mon = 1 + m_tmLocalTime.tm_mon;
	}

	{
		// Update CBV data
		m_cbvData.resolution.x = float(m_width);
		m_cbvData.resolution.y = float(m_height);
		m_cbvData.resolution.z = float(m_dpi);
		m_cbvData.resolution.w = m_aspectRatio;
		m_cbvData.time = float(m_timer.GetTotalSeconds());
		m_cbvData.timeDelta = float(m_timer.GetElapsedSeconds());
		m_cbvData.frame = m_frameCounter;
		m_cbvData.ticks = m_tickCounter;
		m_cbvData.date.x = m_tmLocalTime.tm_year;
		m_cbvData.date.y = m_tmLocalTime.tm_mon;
		m_cbvData.date.z = m_tmLocalTime.tm_mday;
		m_cbvData.date.w = m_tmLocalTime.tm_yday;
		m_cbvData.clock.x = m_tmLocalTime.tm_hour;
		m_cbvData.clock.y = m_tmLocalTime.tm_min;
		m_cbvData.clock.z = m_tmLocalTime.tm_sec;
		m_cbvData.clock.w = m_tmLocalTime.tm_isdst;
		m_cbvData.random = rand() * RAND_MAX;
		m_cbvData.perfHigh = m_timer.GetPerfTimeHigh();
		m_cbvData.perfLow = m_timer.GetPerfTimeLow();
		memcpy(m_cbvDataBegin, &m_cbvData, sizeof(m_cbvData));
	}

	if (!(m_frameCounter % 300))
	{
		// Update window text with FPS value.
		wchar_t fps[64];
		swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());
		SetCustomWindowText(fps);
		//m_frameCounter = 0;
	}

	//m_timer.GetElapsedSeconds()
}

// Render the scene.
void FXApp::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	LoadCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}
#pragma endregion

#pragma region DX Loaders
// Load the rendering pipeline dependencies.
void FXApp::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// [DEBUG] Enable debug interface
	{
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController))))
        {
            m_debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_dxgiDebug))))
		{
			m_dxgiDebug->EnableLeakTrackingForThread();
		}
	}
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(m_factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

#if defined(_DEBUG)
	// [DEBUG] Setup debug interface to break on any warnings/errors
    if (m_debugController)
    {
        ID3D12InfoQueue* pInfoQueue = NULL;
        m_device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
    }
#endif

	{
		// Describe and create a render target view descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FRAME_BUFFER;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 1;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT n = 0; n < FRAME_BUFFER; n++)
		{
			m_renderTargetsDescriptor[n] = rtvHandle;
			rtvHandle.ptr += m_rtvDescriptorSize;
		}
		NAME_D3D12_OBJECT(m_rtvHeap);
    }

	{
		// Create SVR descriptor heap. - shader-resource, and unordered-access views
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvHeapDesc.NumDescriptors = 1;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
		m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		NAME_D3D12_OBJECT(m_srvHeap);
	}

	{
		// Create CBV descriptor heap. - constant buffer values
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NumDescriptors = 1;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
		m_cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		NAME_D3D12_OBJECT(m_cbvHeap);
	}

	{
		// Describe and create a depth stencil view (DSV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 1;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
		m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		NAME_D3D12_OBJECT(m_dsvHeap);
	}

	{
		// Describe and create a sampler descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		samplerHeapDesc.NumDescriptors = 1;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));
		NAME_D3D12_OBJECT(m_samplerHeap);
	}


	// Describe and create the command queue.
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;
		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
		NAME_D3D12_OBJECT(m_commandQueue);
	}

	// Create CommandAllocators, CommandList and Fence.
	{
		for (UINT n = 0; n < FRAME_BUFFER; n++)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
			NAME_D3D12_OBJECT(m_commandAllocators[n]);
		}
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), NULL, IID_PPV_ARGS(&m_commandList)));
		NAME_D3D12_OBJECT(m_commandList);
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		NAME_D3D12_OBJECT(m_fence);
		m_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	//	Create the swap chain.
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		{
			ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
			swapChainDesc.BufferCount = FRAME_BUFFER;
			swapChainDesc.Width = m_width;
			swapChainDesc.Height = m_height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.Stereo = FALSE;
		}

		ComPtr<IDXGISwapChain1> swapChain = NULL;
		ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
			m_commandQueue.Get(),
			Wnd::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));
		ThrowIfFailed(m_factory->MakeWindowAssociation(Wnd::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swapChain.As(&m_swapChain));
		m_swapChain->SetMaximumFrameLatency(FRAME_BUFFER);
		m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	}

	// Update Viewport and Scissors sizes
	m_viewport.Width = m_width;
	m_viewport.Height = m_height;
	m_scissorRect.right = m_width;
	m_scissorRect.bottom = m_height;
}

void FXApp::LoadAssets()
{
	// Create an empty root signature.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

		CD3DX12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 2;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, rootSignatureFlags);

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &m_signature, &m_error));
		ThrowIfFailed(m_device->CreateRootSignature(0, m_signature->GetBufferPointer(), m_signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		LoadShaders();

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS.BytecodeLength = m_vertexShader->GetBufferSize();
		psoDesc.VS.pShaderBytecode = m_vertexShader->GetBufferPointer();
		psoDesc.PS.BytecodeLength = m_pixelShader->GetBufferSize();
		psoDesc.PS.pShaderBytecode = m_pixelShader->GetBufferPointer();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		//psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
		NAME_D3D12_OBJECT(m_pipelineState);
	}

	// Create the vertex buffer.
	LoadVertexBuffer();

	{
		// Create the ConstantBufferValues
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_cbvUploadHeap)));
		NAME_D3D12_OBJECT(m_cbvUploadHeap);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_cbvUploadHeap->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(m_cbvData) + 255) & ~255;
		m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

		ZeroMemory(&m_cbvData, sizeof(m_cbvData));

		// Copy the square data to the vertex buffer.
		CD3DX12_RANGE readcbvRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_cbvUploadHeap->Map(0, &readcbvRange, reinterpret_cast<void**>(&m_cbvDataBegin)));
		memcpy(m_cbvDataBegin, &m_cbvData, sizeof(m_cbvData));
		m_cbvUploadHeap->Unmap(0, nullptr);

	}

	{
		// Create the DepthStencilView.
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
		));
		NAME_D3D12_OBJECT(m_depthStencil);
		m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	/* TODO Learn about other Resources and Samplers
	{
		// Create the texture and sampler.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = Textures[0].MipLevels;
		textureDesc.Format = Textures[0].Format;
		textureDesc.Width = Textures[0].Width;
		textureDesc.Height = Textures[0].Height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture)));

		NAME_D3D12_OBJECT(m_texture);

		const UINT subresourceCount = textureDesc.DepthOrArraySize * textureDesc.MipLevels;
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, subresourceCount);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_textureUploadHeap)));

		// Copy data to the intermediate upload heap and then schedule a copy
		// from the upload heap to the Texture2D.
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = pVertexDataBegin + Textures[0].Data[0].Offset;
		textureData.RowPitch = Textures[0].Data[0].Pitch;
		textureData.SlicePitch = Textures[0].Data[0].Size;

		UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, subresourceCount, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	{
		// Create a sampler.
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		m_device->CreateSampler(&samplerDesc, m_samplerHeap->GetCPUDescriptorHandleForHeapStart());

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = Textures->Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	{
		// Create a heap for occlusion queries.
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Count = 1;
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		//ThrowIfFailed(m_device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_queryHeap)));
		//NAME_D3D12_OBJECT(m_queryHeap);
	}
		*/

	// Create synchronization objects.
	{
		ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_frameIndex]++;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	CreateRenderTargets();

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);

	// Command list created, so close it now.
	ThrowIfFailed(m_commandList->Close());

	WaitForGpu();

}

void FXApp::LoadShaders()
{
	// Create Utils.
	// Open source file.
	// Send to compiler.
	ComPtr<IDxcUtils> pUtils;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
	ComPtr<IDxcBlobEncoding> pSource = nullptr;

	// TODO: Separate HLSL files for Vertex and Pixel Shaders.
	bIsShaderError = false;
	if (bIsShaderFile)
	{
		pUtils->LoadFile(m_shaderFile.c_str(), nullptr, &pSource);
		CompileShader(pSource, PixelShader);
		CompileShader(pSource, VertexShader);
	}
	else
	{
		// Find shaders.hlsl on disk or use basic HLSL from internal resouce
		m_shaderFile = GetAssetFullPath(L"Shader.hlsl");
		if (m_shaderFile != L"") {
			pUtils->LoadFile(m_shaderFile.c_str(), nullptr, &pSource);
			CompileShader(pSource, PixelShader);
			CompileShader(pSource, VertexShader);
			if (!bIsShaderError) 
			{
				std::string converted_str(m_shaderFile.begin(), m_shaderFile.end());
				m_shaderFileA = converted_str.c_str();
				bIsShaderFile = true;
				Wnd::OnFileLoad(m_shaderFile);
			}
			else 
			{
				// Local shader.hlsl file have errors, fallback to internal hlsl.
				bIsShaderError = false;
				goto internalres;
			}
		}
		else
		{
internalres:
			uint32_t codePage = CP_UTF8;
			HRSRC hRes = FindResource(Wnd::GetIntance(), L"SHADERSHLSL", L"TEXTFILE");
			HGLOBAL hMem = LoadResource(Wnd::GetIntance(), hRes);
			DWORD size = SizeofResource(Wnd::GetIntance(), hRes);
			char* resText = (char*)LockResource(hMem);
			char* text = (char*)malloc(size + 1);
			memcpy(text, resText, size);
			FreeResource(hMem);

			// Since we are using the internal HLSL, 
			// generate a default hlsl file so we can output all extra infos
			WCHAR lp_exePath[MAX_PATH];
			GetModuleFileName(NULL, lp_exePath, MAX_PATH);
			std::wstring ws_exePath = lp_exePath;
			ws_exePath = ws_exePath.substr(0, ws_exePath.rfind(L"\\") + 1);
			FILE* fp = NULL;
			m_shaderFile.append(ws_exePath);
			m_shaderFile.append(L"Shader.hlsl");
			_wfopen_s(&fp, m_shaderFile.c_str(), L"wb");
			fwrite(text, size, 1, fp);
			fclose(fp);
			free(text);

			//pUtils->CreateBlob(text, size, CP_UTF8, &pSource);
			pUtils->LoadFile(m_shaderFile.c_str(), nullptr, &pSource);
			CompileShader(pSource, PixelShader);
			CompileShader(pSource, VertexShader);

			std::string converted_str(m_shaderFile.begin(), m_shaderFile.end());
			m_shaderFileA = converted_str.c_str();
			bIsShaderFile = true;
			Wnd::OnFileLoad(m_shaderFile);
		}
	}
}

void FXApp::LoadVertexBuffer()
{
	if (bIsShaderFile)
	{
		m_vertexCount = 4;
		// Define the geometry for a quad
		Vertex bufferVertices[] =
		{
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
			{ {  1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
			{ {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(bufferVertices);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBufferUploadHeap)));

		NAME_D3D12_OBJECT(m_vertexBufferUploadHeap);

		// Copy the square data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBufferUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, bufferVertices, sizeof(bufferVertices));
		m_vertexBufferUploadHeap->Unmap(0, nullptr);

		m_commandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, m_vertexBufferUploadHeap.Get(), 0, vertexBufferSize);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;

		// a quad (2 triangles)
		DWORD iList[] = {
			0, 1, 2, // first triangle
			0, 3, 2, // second triangle
		};

		int iBufferSize = sizeof(iList);

		// create default heap to hold index buffer
		m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
			nullptr, // optimized clear value must be null for this type of resource
			IID_PPV_ARGS(&m_indexBuffer));
		NAME_D3D12_OBJECT(m_indexBuffer);

		// create upload heap to upload index buffer
		m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&m_indexBufferUploadHeap));
		NAME_D3D12_OBJECT(m_indexBufferUploadHeap);

		// Copy the square data to the vertex buffer.
		UINT8* pindexBuffer;
		CD3DX12_RANGE indexreadRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_indexBufferUploadHeap->Map(0, &indexreadRange, reinterpret_cast<void**>(&pindexBuffer)));
		memcpy(pindexBuffer, iList, sizeof(iList));
		m_indexBufferUploadHeap->Unmap(0, nullptr);

		m_commandList->CopyBufferRegion(m_indexBuffer.Get(), 0, m_indexBufferUploadHeap.Get(), 0, iBufferSize);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
		m_indexBufferView.SizeInBytes = iBufferSize;

	}
	else
	{
		m_vertexCount = 3;
		// Define the geometry for a triangle.
		Vertex bufferVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.1f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.9f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(bufferVertices);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));
		NAME_D3D12_OBJECT(m_vertexBuffer);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBufferUploadHeap)));
		NAME_D3D12_OBJECT(m_vertexBufferUploadHeap);

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBufferUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, bufferVertices, sizeof(bufferVertices));
		m_vertexBufferUploadHeap->Unmap(0, nullptr);

		m_commandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, m_vertexBufferUploadHeap.Get(), 0, vertexBufferSize);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Close the resource creation command list and execute it to begin the vertex buffer copy into
	// the default heap.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

}

void FXApp::LoadCommandList()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Set necessary state.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);  //&dsvHandle

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	m_commandList->SetComputeRootSignature(m_rootSignature.Get());
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// Record commands.
	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(1, ppHeaps[0]->GetGPUDescriptorHandleForHeapStart());

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); //D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	m_commandList->DrawInstanced(m_vertexCount, 1, 0, 0);
	/*
	* For now, ignore IndexBuffer
	if (m_indexBuffer)
	{
		m_commandList->IASetIndexBuffer(&m_indexBufferView);
		m_commandList->DrawIndexedInstanced(m_vertexCount, 1, 0, 0, 0);
	}
	else
	{
		m_commandList->DrawInstanced(m_vertexCount, 1, 0, 0);
	}
	*/

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	ThrowIfFailed(m_commandList->Close());

	/*
	//ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get(), m_samplerHeap.Get() };
	//m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//m_commandList->SetGraphicsRootDescriptorTable(0, ppHeaps[0]->GetGPUDescriptorHandleForHeapStart());
	//m_commandList->SetGraphicsRootDescriptorTable(1, ppHeaps[1]->GetGPUDescriptorHandleForHeapStart());

	// Move depth bounds so we can see they move. Depth bound test will test against DEST depth
	// that we primed previously
	const FLOAT f = 0.125f + sinf((m_frameCounter & 0x7F) / 127.f) * 0.125f;      // [0.. 0.25]
	m_commandList->OMSetDepthBounds(0.0f + f, 1.0f - f);

	// Render the triangle with depth bounds
	m_commandList->SetPipelineState(m_pipelineState.Get());
	m_commandList->DrawInstanced(m_vertexCount, 1, 0, 0);

	// Disable depth bounds on Direct3D 12 by resetting back to the default range
	m_commandList->OMSetDepthBounds(0.0f, 1.0f);
	*/
}

void FXApp::LoadUI()
{
	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Wnd::GetHwnd());
	ImGui_ImplDX12_Init(m_device.Get(), FRAME_BUFFER,
						DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(),
						m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
						m_srvHeap->GetGPUDescriptorHandleForHeapStart());
}

void FXApp::ReloadAssets()
{
	WaitForNextFrame();

	// Shutdown main pipelines before start again with new ShaderFile.
	m_pipelineState.ReleaseAndGetAddressOf();
	m_depthState.ReleaseAndGetAddressOf();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// Start new command list
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	LoadAssets();
	LoadUI();
}
#pragma endregion

#pragma region DX_GPU
void FXApp::WaitForGpu()
{
// Wait for pending GPU work to complete.
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// Wait until the fence has been processed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValues[m_frameIndex]++;
}

void FXApp::WaitForNextFrame()
{
// Prepare to render the next frame.
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Update the frame index.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void FXApp::ResizeSwapChain(int width, int height)
{
	// Force Release of buffer references 
	m_vertexBuffer.ReleaseAndGetAddressOf();
	for (UINT n = 0; n < FRAME_BUFFER; n++)
	{
		if (m_renderTargets[n]) {
			m_renderTargets[n].ReleaseAndGetAddressOf();
		}
	}

	CloseHandle(m_swapChainWaitableObject);

	// Resize BufferSize
	ThrowIfFailed(m_swapChain->ResizeBuffers(FRAME_BUFFER, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

	// Resize Viewport and Scissor
	m_viewport.Width = static_cast<float>(width);
	m_viewport.Height = static_cast<float>(height);
	m_scissorRect.right = static_cast<LONG>(width);
	m_scissorRect.bottom = static_cast<LONG>(height);

	// Create new buffer references
	m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
	assert(m_swapChainWaitableObject != NULL);

	// Start new command list
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	CreateRenderTargets();
	LoadVertexBuffer();

	WaitForNextFrame();
}

void FXApp::CreateRenderTargets()
{
	// Create a RenderTargetView for each frame.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FRAME_BUFFER; n++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
		NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);
	}
}

void FXApp::CleanupRenderTargets()
{
	for (UINT n = 0; n < FRAME_BUFFER; n++)
	{
		if (m_renderTargets[n]) {
			m_renderTargets[n]->Release();
		}
	}
	m_depthStencil->Release();
}

void FXApp::CleanupDeviceD3D()
{
	for (UINT n = 0; n < FRAME_BUFFER; n++)
	{
		if (m_renderTargets[n]) {
			m_renderTargets[n].ReleaseAndGetAddressOf();
		}
	}
	for (UINT n = 0; n < FRAME_BUFFER; n++)
	{
		if (m_commandAllocators[n]) {
			m_commandAllocators[n].ReleaseAndGetAddressOf();
		}
	}
	if (m_commandQueue) {
		m_commandQueue.ReleaseAndGetAddressOf();
	}
	if (m_commandList) {
		m_commandList.ReleaseAndGetAddressOf();
	}
	if (m_swapChain) {
		m_swapChain.ReleaseAndGetAddressOf();
	}
	if (m_swapChainWaitableObject != NULL) {
		CloseHandle(m_swapChainWaitableObject);
	}
	if (m_texture) {
		m_texture.ReleaseAndGetAddressOf();
	}
	if (m_textureUploadHeap) {
		m_textureUploadHeap.ReleaseAndGetAddressOf();
	}
	if (m_queryState) {
		m_queryState.ReleaseAndGetAddressOf();
	}
	if (m_queryHeap) {
		m_queryHeap.ReleaseAndGetAddressOf();
	}
	if (m_queryResult) {
		m_queryResult.ReleaseAndGetAddressOf();
	}
	if (m_vertexBuffer) {
		m_vertexBuffer.ReleaseAndGetAddressOf();
	}
	if (m_vertexBufferUploadHeap) {
		m_vertexBufferUploadHeap.ReleaseAndGetAddressOf();
	}
	if (m_indexBuffer) {
		m_indexBuffer.ReleaseAndGetAddressOf();
	}
	if (m_indexBufferUploadHeap) {
		m_indexBufferUploadHeap.ReleaseAndGetAddressOf();
	}
	if (m_depthState) {
		m_depthState.ReleaseAndGetAddressOf();
	}
	if (m_depthStencil) {
		m_depthStencil.ReleaseAndGetAddressOf();
	}
	if (m_samplerHeap) {
		m_samplerHeap.ReleaseAndGetAddressOf();
	}
	if (m_cbvHeap) {
		m_cbvHeap.ReleaseAndGetAddressOf();
	}
	if (m_cbvBuffer) {
		m_cbvBuffer.ReleaseAndGetAddressOf();
	}
	if (m_cbvUploadHeap) {
		m_cbvUploadHeap.ReleaseAndGetAddressOf();
	}
	if (m_rtvHeap) {
		m_rtvHeap.ReleaseAndGetAddressOf();
	}
	if (m_dsvHeap) {
		m_dsvHeap.ReleaseAndGetAddressOf();
	}
	if (m_srvHeap) {
		m_srvHeap.ReleaseAndGetAddressOf();
	}
	if (m_samplerHeap) {
		m_samplerHeap.ReleaseAndGetAddressOf();
	}
	if (m_queryState) {
		m_queryState.ReleaseAndGetAddressOf();
	}
	if (m_queryHeap) {
		m_queryHeap.ReleaseAndGetAddressOf();
	}
	if (m_fence) {
		m_fence.ReleaseAndGetAddressOf();
	}
	if (m_fenceEvent) {
		CloseHandle(m_fenceEvent);
	}
	if (m_pixelShader) {
		m_pixelShader.ReleaseAndGetAddressOf();
	}
	if (m_vertexShader) {
		m_vertexShader.ReleaseAndGetAddressOf();
	}
	if (m_signature) {
		m_signature.ReleaseAndGetAddressOf();
	}
	if (m_error) {
		m_error.ReleaseAndGetAddressOf();
	}
	if (m_rootSignature) {
		m_rootSignature.ReleaseAndGetAddressOf();
	}
	if (m_pipelineState) {
		m_pipelineState.ReleaseAndGetAddressOf();
	}
	if (m_adapter) {
		m_adapter.ReleaseAndGetAddressOf();
	}
	if (m_factory) {
		m_factory.ReleaseAndGetAddressOf();
	}
	if (m_device) {
		m_device.ReleaseAndGetAddressOf();
	}

#if defined(_DEBUG)
	OutputDebugString(L"===========================================:\n");
	OutputDebugString(L"DX Objects still open after CleanupDevice3D:\n");
	m_dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	m_dxgiDebug.ReleaseAndGetAddressOf();
	m_debugController.ReleaseAndGetAddressOf();
	OutputDebugString(L"   (if empty, you cleanup everything.)\n");
	OutputDebugString(L"===========================================:\n");
#endif
}
#pragma endregion

#pragma region ImGui
// ImGui
void FXApp::OnRenderUI()
{
	// TODO: Learn how to get the current line height from ImGui.
	const int lineheight = 36;

	WaitForGpu();

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(500, lineheight), ImGuiCond_FirstUseEver);
	ImGui::Begin("ShaderPiPFX Select File", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
	if (ImGui::Button("HLSL file :")) 
	{
		FXApp::OpenFileDialog();
		return;
	}
	ImGui::SameLine();
	ImGui::Text(m_shaderFileA.c_str());
	ImGui::End();

	// Error Log Window
	if (bIsShaderError) 
	{
		ImGui::Begin("Shader Compiler Log", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::TextUnformatted(m_ShaderErrorMsg._Unchecked_begin(), m_ShaderErrorMsg._Unchecked_end());
		ImGui::PopStyleVar();
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) 
		{
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::SetWindowFontScale(1.2f);
		ImGui::EndChild();
		ImGui::End();
		ImGui::SetWindowPos("Shader Compiler Log", ImVec2(0, m_height - (lineheight * 5)));
		ImGui::SetWindowSize("Shader Compiler Log", ImVec2(m_width, lineheight * 5));
		//ImGui::SetNextWindowPos(ImVec2(0, m_height - (lineheight * 5)), ImGuiCond_FirstUseEver);
		//ImGui::SetNextWindowSize(ImVec2(m_width, lineheight * 5), ImGuiCond_FirstUseEver);
	}

	// Rendering
	ImGui::Render();

	// Render Dear ImGui graphics
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), NULL);
	m_commandList->ResourceBarrier(1, &barrier);

	//m_commandList->ClearRenderTargetView(m_renderTargetsDescriptor[m_frameIndex], (float*)&clear_color, 0, NULL);
	m_commandList->OMSetRenderTargets(1, &m_renderTargetsDescriptor[m_frameIndex], FALSE, NULL);
	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	m_commandList->ResourceBarrier(1, &barrier);
	m_commandList->Close();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

bool FXApp::UIWantCapture()
{
	return (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput);
}
#pragma endregion

#pragma region Event Handlers
// Message handlers
void FXApp::OnActivated()
{
}

void FXApp::OnDeactivated()
{
}

void FXApp::OnResuming()
{
}


void FXApp::OnSuspending()
{
}

void FXApp::OnWindowMoved()
{
}

void FXApp::OnWindowSizeChanged(int width, int height)
{
	bIsNextFrameReady = false;
	m_width = width;
	m_height = height;
	m_aspectRatio = float(width) / float(height);
	WaitForNextFrame();
	ImGui_ImplDX12_InvalidateDeviceObjects();
	ResizeSwapChain(width, height);
	ImGui_ImplDX12_CreateDeviceObjects();
	bIsNextFrameReady = true;
}

void FXApp::OnDestroy()
{
	bIsExiting = false;
	WaitForNextFrame();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();

#if defined(_DEBUG)
	std::wstringstream cc;
	cc << m_tickCounter;
	OutputDebugString(L"TickCounter  : ");
	OutputDebugString(cc.str().c_str());
	OutputDebugString(L"\n");
	OutputDebugString(L"FrameCounter : ");
	cc = {};
	cc << m_frameCounter;
	OutputDebugString(cc.str().c_str());
	OutputDebugString(L"\n");
#endif
}
#pragma endregion

#pragma region Helpers
// Helper function for resolving the full path of assets.
std::wstring FXApp::GetAssetFullPath(LPCWSTR assetName)
{
	std::wstring m_assetFullPath = m_assetsPath + assetName;
	HANDLE hFile = CreateFile2(m_assetFullPath.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
	if (hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		return m_assetsPath + assetName;
	}
	CloseHandle(hFile);
	std::wstring filenotfound = L"";
	return filenotfound;
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
void FXApp::GetHardwareAdapter(
	IDXGIFactory1* pFactory,
	IDXGIAdapter1** ppAdapter,
	bool requestHighPerformanceAdapter)
{
	for (
		UINT adapterIndex = 0;
		DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapterByGpuPreference(
		adapterIndex,
		requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
		IID_PPV_ARGS(&m_adapter));
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		m_adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see whether the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}
}

// Helper function for setting the window's title text.
void FXApp::SetCustomWindowText(LPCWSTR text)
{
	std::wstring windowText = m_title + L" " + text;
	SetWindowText(Wnd::GetHwnd(), windowText.c_str());
}

// Helper function for parsing any supplied command line args.
void FXApp::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
		{
			m_useWarpDevice = true;
			m_title = m_title + L" (WARP)";
		}
	}
}

void FXApp::MakeWindowAssociationWithLocatedFactory(
	ComPtr<IDXGISwapChain3> const& swapChain,
	HWND hWnd,
	UINT flags)
{
	ComPtr<IDXGIFactory6> factory;
	swapChain->GetParent(IID_PPV_ARGS(&factory));
	factory->MakeWindowAssociation(hWnd, flags);
}

void FXApp::OpenFileDialog() 
{
	WaitForNextFrame();

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
								COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog* pFileOpen;
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
							  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
		if (SUCCEEDED(hr))
		{
			COMDLG_FILTERSPEC aFileTypes[1];
			aFileTypes[0].pszName = L"HLSL Shader";
			aFileTypes[0].pszSpec = L"*.hlsl";
			pFileOpen->SetFileTypes(1, aFileTypes);
			hr = pFileOpen->Show(NULL);
			if (SUCCEEDED(hr))
			{
				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					if (SUCCEEDED(hr))
					{
						CoTaskMemFree(pszFilePath);
						std::wstring wsName = pszFilePath;
						std::wstring wsExt = wsName.substr(wsName.length() - 4, 4);
						std::wstring wsHLSLExt = L"hlsl";
						if (wsExt == wsHLSLExt)
						{
							FXApp::SetShaderFile(pszFilePath);
						}
						//MessageBox(Wnd::GetHwnd(), m_shaderFile.c_str(), L"Current file received", MB_OK);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}
}

void FXApp::SetShaderFile(std::wstring fileName)
{
	m_shaderFile = fileName;

	std::string converted_str(fileName.begin(), fileName.end());
	m_shaderFileA = converted_str.c_str();

	bIsShaderFile = true;
	ReloadAssets();
	Wnd::OnFileLoad(m_shaderFile);
}

bool FXApp::CompileShader(ComPtr<IDxcBlobEncoding> pSource, FXApp::EShaderType shaderType)
{
	ComPtr<IDxcBlob> m_tempShader;

	// FX
	WCHAR lp_exePath[MAX_PATH];
	GetModuleFileName(NULL, lp_exePath, MAX_PATH);
	std::wstring ws_exePath = lp_exePath;
	ws_exePath = ws_exePath.substr(0, ws_exePath.rfind(L"\\") + 1);

	// 
	// Create compiler and utils.
	//
	ComPtr<IDxcUtils> pUtils;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
	ComPtr<IDxcCompiler3> pCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

	//
	// Create default include handler. (You can create your own...)
	//
	ComPtr<IDxcIncludeHandler> pIncludeHandler;
	pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

	//
	// Compiler results pointer.
	//
	ComPtr<IDxcResult> pResults;

	//
	// Prepare source buffer
	//
	DxcBuffer Source;
	Source.Ptr = pSource->GetBufferPointer();
	Source.Size = pSource->GetBufferSize();
	Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

	//
	// Compiler arguments.
	//
	//std::wstring m_FileName = m_shaderFile.substr(m_shaderFile.rfind(L"\\") + 1, m_shaderFile.length() - m_shaderFile.rfind(L"\\"));
	std::wstring m_FileName = m_shaderFile;
	std::wstring m_shaderFileBin = L"";
	std::wstring m_shaderFilePdb = L"";
	std::wstring m_shaderFileHea = L"";
	std::wstring m_shaderFileAsm = L"";
	std::wstring m_shaderFileRef = L"";
	std::wstring m_shaderFileRoo = L"";
	std::wstring m_shaderFileErr = L"";

	switch (shaderType)
	{
		case PixelShader:
		{
			m_shaderFileBin.append(m_FileName);
			m_shaderFileBin.append(L".PS.cso");
			m_shaderFilePdb.append(m_FileName);
			m_shaderFilePdb.append(L".PS.pdb");
			m_shaderFileHea.append(m_FileName);
			m_shaderFileHea.append(L".PS.h");
			m_shaderFileAsm.append(m_FileName);
			m_shaderFileAsm.append(L".PS.ll");
			m_shaderFileRef.append(m_FileName);
			m_shaderFileRef.append(L".PS.reflection");
			m_shaderFileRoo.append(m_FileName);
			m_shaderFileRoo.append(L".PS.rootsig");
			m_shaderFileErr.append(m_FileName);
			m_shaderFileErr.append(L".PS.err");

			LPCWSTR pszArgs[] =
			{
				m_shaderFile.c_str(),            // Optional shader source file name for error reporting and for PIX shader source view.  
				L"-E", L"PSMain",              // Entry point.
				L"-T", L"ps_6_4",            // Target.
				L"-Zi",                      // Enable debug information.
				L"-Fo", m_shaderFileBin.c_str(),     // CSO. Stored in the pdb. 
				L"-Fd", m_shaderFilePdb.c_str(),     // The file name of the pdb. This must either be supplied or the autogenerated file name must be used.
				//L"-Fh", m_shaderFileHea.c_str(),
				//L"-Fc", m_shaderFileAsm.c_str(),
				L"-Fre", m_shaderFileRef.c_str(),
				L"-Frs", m_shaderFileRoo.c_str(),
				L"-Fe", m_shaderFileErr.c_str(),
				L"-Dcheck_warning",
				L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
			};

			//
			// Compile and send to destination shader
			//
			pCompiler->Compile(
				&Source,                // Source buffer.
				pszArgs,                // Array of pointers to arguments.
				_countof(pszArgs),      // Number of arguments.
				pIncludeHandler.Get(),        // User-provided interface to handle #include directives (optional).
				IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
			);
			pResults->GetResult(&m_tempShader);

			break;
		}
		case VertexShader:
		{
			m_shaderFileBin.append(m_FileName);
			m_shaderFileBin.append(L".VS.cso");
			m_shaderFilePdb.append(m_FileName);
			m_shaderFilePdb.append(L".VS.pdb");
			m_shaderFileHea.append(m_FileName);
			m_shaderFileHea.append(L".VS.h");
			m_shaderFileAsm.append(m_FileName);
			m_shaderFileAsm.append(L".VS.ll");
			m_shaderFileRef.append(m_FileName);
			m_shaderFileRef.append(L".VS.reflection");
			m_shaderFileRoo.append(m_FileName);
			m_shaderFileRoo.append(L".VS.rootsig");
			m_shaderFileErr.append(m_FileName);
			m_shaderFileErr.append(L".VS.err");

			LPCWSTR pszArgs[] =
			{
				m_shaderFile.c_str(),            // Optional shader source file name for error reporting and for PIX shader source view.  
				L"-E", L"VSMain",              // Entry point.
				L"-T", L"vs_6_4",            // Target.
				L"-Zi",                      // Enable debug information.
				L"-Fo", m_shaderFileBin.c_str(),     // Optional. Stored in the pdb. 
				L"-Fd", m_shaderFilePdb.c_str(),     // The file name of the pdb. This must either be supplied or the autogenerated file name must be used.
				//L"-Fh", m_shaderFileHea.c_str(),
				//L"-Fc", m_shaderFileAsm.c_str(),
				L"-Fre", m_shaderFileRef.c_str(),
				L"-Frs", m_shaderFileRoo.c_str(),
				L"-Fe", m_shaderFileErr.c_str(),
				L"-Dcheck_warning",
				L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
			};

			//
			// Compile and send to destination shader
			//
			pCompiler->Compile(
				&Source,                // Source buffer.
				pszArgs,                // Array of pointers to arguments.
				_countof(pszArgs),      // Number of arguments.
				pIncludeHandler.Get(),        // User-provided interface to handle #include directives (optional).
				IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
			);
			pResults->GetResult(&m_tempShader);

			break;
		}
		default:
			break;
	}

	//
	// Print errors if present.
	//
	ComPtr<IDxcBlobUtf8> pErrors = nullptr;
	ComPtr<IDxcBlobUtf16> pErrorsName = nullptr;
	pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), &pErrorsName);
	// Note that d3dcompiler would return null if no errors or warnings are present.  
	// IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
	if (pErrors != nullptr && pErrors->GetStringLength() != 0)
	{
		/*
		* Disable save ERR for now.
		FILE* fp = NULL;
		_wfopen_s(&fp, m_shaderFileErr.c_str(), L"wb");
		fwrite(pErrors->GetBufferPointer(), pErrors->GetBufferSize(), 1, fp);
		fclose(fp);
		*/

		wprintf(L"DXC Warnings and Errors:\n%S\n", pErrors->GetStringPointer());
		wchar_t buffer[1024];
		swprintf(buffer, 1024, L"%S", pErrors->GetStringPointer());
		std::wstring wsErrorsMsg = buffer;
		OutputDebugString(wsErrorsMsg.c_str());
		bIsShaderError = true;
		std::string converted_str(wsErrorsMsg.begin(), wsErrorsMsg.end());
		m_ShaderErrorMsg = converted_str.c_str();
	}
	else
	{
		// Only send to our pipeline if shader compiled successfully
		switch (shaderType)
		{
			case PixelShader:
			{
				pResults->GetResult(&m_pixelShader);
				break;
			}
			case VertexShader:
			{
				pResults->GetResult(&m_vertexShader);
				break;
			}
			default:
			{
				break;
			}
		}
		
	}

	//
	// Quit if the compilation failed.
	//
	HRESULT hrStatus;
	pResults->GetStatus(&hrStatus);
	if (FAILED(hrStatus))
	{
		wprintf(L"DXC Compilation Failed\n");
		return false;
	}

	// TODO: Create options to save extra files (CSO, PDB, Hash, RootSignature, ...), escape for now
	return true;
	
	//
	// Save shader binary.
	//
	ComPtr<IDxcBlob> pShader = nullptr;
	ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
	pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pShaderName);
	if (pShader != nullptr)
	{
		FILE* fp = NULL;

		_wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
		fwrite(pShader->GetBufferPointer(), pShader->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	//
	// Save pdb.
	//
	ComPtr<IDxcBlob> pPDB = nullptr;
	ComPtr<IDxcBlobUtf16> pPDBName = nullptr;
	pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
	{
		FILE* fp = NULL;

		// Note that if you don't specify -Fd, a pdb name will be automatically generated. Use this file name to save the pdb so that PIX can find it quickly.
		_wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
		fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	//
	// Print hash.
	//
	ComPtr<IDxcBlob> pHash = nullptr;
	pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
	if (pHash != nullptr)
	{
		wprintf(L"Hash: ");
		DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
		for (int i = 0; i < _countof(pHashBuf->HashDigest); i++)
			wprintf(L"%x", pHashBuf->HashDigest[i]);
		wprintf(L"\n");
	}


	//
	// Print RootDig.
	//
	ComPtr<IDxcBlob> pRootSig = nullptr;
	ComPtr<IDxcBlobUtf16> pRootSigName = nullptr;
	pResults->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&pRootSig), &pRootSigName);
	if (pRootSig != nullptr)
	{
		FILE* fp = NULL;
		_wfopen_s(&fp, pRootSigName->GetStringPointer(), L"wb");
		fwrite(pRootSig->GetBufferPointer(), pRootSig->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	//
	// Print Disassembly.
	//
	ComPtr<IDxcBlob> pDisassembly = nullptr;
	ComPtr<IDxcBlobUtf16> pDisassemblyName = nullptr;
	pResults->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&pDisassembly), &pDisassemblyName);
	if (pDisassembly != nullptr)
	{
		FILE* fp = NULL;
		_wfopen_s(&fp, pDisassemblyName->GetStringPointer(), L"wb");
		fwrite(pDisassembly->GetBufferPointer(), pDisassembly->GetBufferSize(), 1, fp);
		fclose(fp);
	}

	//
	// Get separate reflection.
	//
	ComPtr<IDxcBlob> pReflectionData;
	pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
	if (pReflectionData != nullptr)
	{
		// Optionally, save reflection blob for later here.

		// Create reflection interface.
		DxcBuffer ReflectionData;
		ReflectionData.Encoding = DXC_CP_ACP;
		ReflectionData.Ptr = pReflectionData->GetBufferPointer();
		ReflectionData.Size = pReflectionData->GetBufferSize();

		ComPtr< ID3D12ShaderReflection > pReflection;
		pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

		// Use reflection interface here.

	}
	return true;
}

#pragma endregion
