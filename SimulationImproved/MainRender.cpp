#include "MainRender.h"

using namespace DirectX;
using namespace std;


MainRender::MainRender() :
	mWidth(0),
	mHeight(0),
	Resetclicked(false)
	
{

}


MainRender::~MainRender()
{
}




void MainRender::SetWindow(const HWND& Wnd)
{
	mWnd = Wnd;
}


/// Compiles shader from files

HRESULT MainRender::CompileShaderFromFile(const WCHAR* const szFileName, const LPCSTR szEntryPoint, const LPCSTR szShaderModel, ID3DBlob** const ppBlobOut) const
{
	HRESULT hr = static_cast<HRESULT>(0L);

	const DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;


	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, const_cast<ID3DBlob**>(ppBlobOut), &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return static_cast<HRESULT>(0L);
}


HRESULT MainRender::InitDXDevice()
{
	HRESULT hr = static_cast<HRESULT>(0L);

	RECT rc;
	GetClientRect(mWnd, &rc);
	mWidth = rc.right - rc.left;
	mHeight = rc.bottom - rc.top;

	const UINT createDeviceFlags = 0;

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		mDriverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, mDriverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &mD3dDevice, &mFeatureLevel, &mImmediateContext);

		if (hr == static_cast<HRESULT>(0x80070057L))
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, mDriverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &mD3dDevice, &mFeatureLevel, &mImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = mD3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	// DirectX 11.1 or later
	hr = mD3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&mD3dDevice1));
	if (SUCCEEDED(hr))
	{
		static_cast<void>(mImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&mImmediateContext1)));
	}

	DXGI_SWAP_CHAIN_DESC1 sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Width = mWidth;
	sd.Height = mHeight;
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;



	hr = dxgiFactory2->CreateSwapChainForHwnd(mD3dDevice, mWnd, &sd, nullptr, nullptr, &mSwapChain1);
	if (SUCCEEDED(hr))
	{
		hr = mSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&mSwapChain));
	}

	dxgiFactory2->Release();
	
	

	//Disable full screen
	dxgiFactory->MakeWindowAssociation(mWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = mD3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = mWidth;
	descDepth.Height = mHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = mD3dDevice->CreateTexture2D(&descDepth, nullptr, &mDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = mD3dDevice->CreateDepthStencilView(mDepthStencil, &descDSV, &mDepthStencilView);
	if (FAILED(hr))
		return hr;

	mImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<float>(mWidth);
	vp.Height = static_cast<float>(mHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	mImmediateContext->RSSetViewports(1, &vp);

	// Set primitive topology
	mImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer
	mBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	mBufferDesc.ByteWidth = sizeof(ConstantBuffer);
	mBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	mBufferDesc.CPUAccessFlags = 0;
	hr = mD3dDevice->CreateBuffer(&mBufferDesc, nullptr, &mConstantBuffer);
	if (FAILED(hr))
		return hr;

	//Set constant buffers
	mImmediateContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
	mImmediateContext->PSSetConstantBuffers(0, 1, &mConstantBuffer);

	//Create sampler state
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	hr = mD3dDevice->CreateSamplerState(&sampDesc, &mSampler);
	if (FAILED(hr))
		return hr;

	//Set sampler state
	mImmediateContext->PSSetSamplers(0, 1, &mSampler);

	//Create blend state for alpha blending
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0f;

	hr = mD3dDevice->CreateBlendState(&blendDesc, &mBlendStateAlpha);
	if (FAILED(hr))
		return hr;

	//Create default blend state for disabling blend
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	hr = mD3dDevice->CreateBlendState(&blendDesc, &mBlendStateDisable);
	if (FAILED(hr))
		return hr;


	//initialize imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(mWnd);
	ImGui_ImplDX11_Init(mD3dDevice, mImmediateContext);
	ImGui::StyleColorsDark();
	// imgui 

	return static_cast<HRESULT>(0L);
}

//Listen for Messages to register inputs
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND  hwnd, UINT  umsg, WPARAM wparam, LPARAM  lparam);
LRESULT CALLBACK WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;


	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;


	case WM_ACTIVATEAPP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

HRESULT MainRender::Render(const vector<GameObject>& entities, const Camera* const camera, const float& time)
{
	HRESULT hr = static_cast<HRESULT>(0L);

	cb1.numOfLights = 0;
	cb1.time = time;

	// Clear the back buffer
	mImmediateContext->ClearRenderTargetView(mRenderTargetView, Colors::LightSkyBlue);

	// Clear the depth buffer to 1.0 (max depth)
	mImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	
	SetCamera(camera);
	
	for (const auto& entity : entities)
	{
		for (const auto& light : entity.Lights())
		{
		
			SetLights(light);
		}
	}
	
	for (const auto& entity : entities)
	{
		for (const auto& shape : entity.Shapes())
		{
			
			hr = Shader(shape);
			if (FAILED(hr))
			{
				return hr;
			}
			
			hr = Texture(shape);
			if (FAILED(hr))
			{
				return hr;
			}
			
			hr = VertexIndex(shape);
			if (FAILED(hr))
			{
				return hr;
			}
			
			XMStoreFloat4x4(&cb1.mWorld, XMMatrixTranspose(XMLoadFloat4x4(&shape.Transform())));
			XMStoreFloat4x4(&cb1.mWorldInverse, XMMatrixInverse(NULL, XMLoadFloat4x4(&cb1.mWorld)));
			mImmediateContext->UpdateSubresource(mConstantBuffer, 0, nullptr, &cb1, 0, 0);

		
			if (shape.ParticleSystem())
			{
				
				float blendFactor[] = { 0.5f, 0.0f, 0.0f, 0.0f };
				mImmediateContext->OMSetBlendState(mBlendStateAlpha, blendFactor, 0xffffffff);
				
				mImmediateContext->OMSetDepthStencilState(mDepthStencilLessThanEqual, 0);
				
				mImmediateContext->RSSetState(mRasteriserStateNoCull);
			}
			else
			{				
				mImmediateContext->OMSetBlendState(mBlendStateDisable, nullptr, 0xffffffff);
				
				mImmediateContext->OMSetDepthStencilState(NULL, 0);
				
				mImmediateContext->RSSetState(mRasteriserStateBackCull);
			}

			
			if (shape.Instances().size() > 0)
			{
				mImmediateContext->DrawIndexedInstanced(shape.Indices().size(), shape.Instances().size(), 0, 0, 0);
			}
			
			else
			{
				mImmediateContext->DrawIndexed(shape.Indices().size(), 0, 0);
			}
		}
	}


	
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();	
	ImGui::Begin("Missle Somulation");
	
	//Make keys and then assign them the values for the fucntions
	ImGui::Text(" Simulation Controls ");
	if (ImGui::Button("Reset Scene")) {
		Resetclicked = true;
	}
	//ImGui::Text(" Time scale : " + to_string(timescale).c_str());
	if (ImGui::Button("cam1")) {
	//	cam = 1;
	}
	if (ImGui::Button("cam2")) {
	//	cam = 2;
	}
	if (ImGui::Button("cam3")) {
		//activeCam = &cameras[2];
	}
	if (ImGui::Button("cam4")) {
		//activeCam = &cameras[3];
	}

	ImGui::Text("Timescale") ;

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());	
	
	//Everything should be called before this to render them 

	mSwapChain->Present(1, 0);


	return hr;
}


void MainRender::SetLights(const Lighting& light)
{
	cb1.vLightPos[static_cast<int>(cb1.numOfLights)] = light.Position();
	cb1.vLightColour[static_cast<int>(cb1.numOfLights)] = light.Colour();
	cb1.numOfLights = cb1.numOfLights + 1;
}


void MainRender::SetCamera(const Camera* const camera)
{
	XMStoreFloat4x4(&cb1.mView, XMMatrixTranspose(XMLoadFloat4x4(&camera->View())));
	XMStoreFloat4x4(&cb1.mProjection, XMMatrixTranspose(XMLoadFloat4x4(&camera->Projection())));
	cb1.vCameraPos = XMFLOAT4(camera->Position().x, camera->Position().y, camera->Position().z, 0.0f);
}

HRESULT MainRender::VertexIndex(const VerticeShapes& shape)
{
	HRESULT hr = static_cast<HRESULT>(0L);
	if (mVertexBuffers.count(shape.Geometry()) == 0)
	{
		
		ZeroMemory(&mBufferDesc, sizeof(mBufferDesc));
		mBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		mBufferDesc.ByteWidth = sizeof(SimpleVertex) * shape.Vertices().size();
		mBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		mBufferDesc.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = &(shape.Vertices()[0]);
		ID3D11Buffer* vertexBuffer = nullptr;
		hr = mD3dDevice->CreateBuffer(&mBufferDesc, &InitData, &vertexBuffer);
		if (FAILED(hr))
		{
			return hr;
		}
	
		mVertexBuffers.insert(pair<MeshType, ID3D11Buffer*>(shape.Geometry(), vertexBuffer));

		const UINT stride = sizeof(SimpleVertex);
		const UINT offset = 0;
		mImmediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	}
	else
	{
		
		const UINT stride = sizeof(SimpleVertex);
		const UINT offset = 0;
		mImmediateContext->IASetVertexBuffers(0, 1, &mVertexBuffers[shape.Geometry()], &stride, &offset);
	}

	if (mIndexBuffers.count(shape.Geometry()) == 0)
	{
		
		mBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		mBufferDesc.ByteWidth = sizeof(WORD) * shape.Indices().size();
		mBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		mBufferDesc.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = &(shape.Indices()[0]);
		ID3D11Buffer* indexBuffer = nullptr;
		hr = mD3dDevice->CreateBuffer(&mBufferDesc, &InitData, &indexBuffer);
		if (FAILED(hr))
		{
			return hr;
		}		
		mIndexBuffers.insert(pair<MeshType, ID3D11Buffer*>(shape.Geometry(), indexBuffer));		
		mImmediateContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	}
	else
	{		
		mImmediateContext->IASetIndexBuffer(mIndexBuffers[shape.Geometry()], DXGI_FORMAT_R16_UINT, 0);
	}


	if (shape.Instances().size() > 0)
	{	
		if (mInstanceBuffers.count(shape.Name()) == 0)
		{
			
			ZeroMemory(&mBufferDesc, sizeof(mBufferDesc));
			mBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			mBufferDesc.ByteWidth = sizeof(InstanceData) * shape.Instances().size();
			mBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			mBufferDesc.CPUAccessFlags = 0;
			D3D11_SUBRESOURCE_DATA InitData;
			ZeroMemory(&InitData, sizeof(InitData));
			InitData.pSysMem = &(shape.Instances()[0]);
			ID3D11Buffer* instanceBuffer = nullptr;
			hr = mD3dDevice->CreateBuffer(&mBufferDesc, &InitData, &instanceBuffer);
			if (FAILED(hr))
			{
				return hr;
			}		
			mInstanceBuffers.insert(pair<string, ID3D11Buffer*>(shape.Name(), instanceBuffer));			
			const UINT stride = sizeof(InstanceData);
			const UINT offset = 0;
			mImmediateContext->IASetVertexBuffers(1, 1, &instanceBuffer, &stride, &offset);
		}
		else
		{			
			const UINT stride = sizeof(InstanceData);
			const UINT offset = 0;
			mImmediateContext->IASetVertexBuffers(1, 1, &mInstanceBuffers[shape.Name()], &stride, &offset);
		    mImmediateContext->UpdateSubresource(mInstanceBuffers[shape.Name()], 0, nullptr, &(shape.Instances()[0]), 0, 0);
		}
	}

	return hr;
}

HRESULT MainRender::Texture(const VerticeShapes& shape)
{
	HRESULT hr = static_cast<HRESULT>(0L);
	ID3D11ShaderResourceView* textureRV = nullptr;


	if (!shape.DiffuseTexture().empty())
	{	
		if (mTextures.count(shape.DiffuseTexture()) == 0)
		{
			
			hr = CreateDDSTextureFromFile(mD3dDevice, shape.DiffuseTexture().c_str(), nullptr, &textureRV);
			if (FAILED(hr))
				return hr;
		
			mTextures.insert(pair<wstring, ID3D11ShaderResourceView*>(shape.DiffuseTexture(), textureRV));		
			mImmediateContext->PSSetShaderResources(0, 1, &textureRV);
		}
		else
		{			
			mImmediateContext->PSSetShaderResources(0, 1, &mTextures[shape.DiffuseTexture()]);
		}
	}

	
	if (!shape.NormalMap().empty())
	{
		
		if (mTextures.count(shape.NormalMap()) == 0)
		{
			
			hr = CreateDDSTextureFromFile(mD3dDevice, shape.NormalMap().c_str(), nullptr, &textureRV);
			if (FAILED(hr))
				return hr;
		
			mTextures.insert(pair<wstring, ID3D11ShaderResourceView*>(shape.NormalMap(), textureRV));

		
			mImmediateContext->PSSetShaderResources(1, 1, &textureRV);
		}
		else
		{			
			mImmediateContext->PSSetShaderResources(1, 1, &mTextures[shape.NormalMap()]);
		}
	}

	
	if (!shape.DisplacementMap().empty())
	{		
		if (mTextures.count(shape.DisplacementMap()) == 0)
		{
			hr = CreateDDSTextureFromFile(mD3dDevice, shape.DisplacementMap().c_str(), nullptr, &textureRV);
			if (FAILED(hr))
				return hr;		
			mTextures.insert(pair<wstring, ID3D11ShaderResourceView*>(shape.DisplacementMap(), textureRV));			
			mImmediateContext->PSSetShaderResources(2, 1, &textureRV);
		}
		else
		{		
			mImmediateContext->PSSetShaderResources(2, 1, &mTextures[shape.DisplacementMap()]);
		}
	}



	return static_cast<HRESULT>(0L);
}
// Initializes windows and its parameters
HRESULT InitWindow(const HINSTANCE hInstance, const int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, reinterpret_cast<LPCTSTR>(IDI_ICON1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"Missle Simulation";
	wcex.hIconSm = LoadIcon(wcex.hInstance, reinterpret_cast<LPCTSTR>(IDI_ICON1));
	if (!RegisterClassEx(&wcex))
		return static_cast<HRESULT>(0x80004005L);

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 1600, 900 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"Missle Simulation", L"Missle Simulation",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return static_cast<HRESULT>(0x80004005L);

	ShowWindow(g_hWnd, nCmdShow);

	return static_cast<HRESULT>(0L);
}

HRESULT MainRender::Shader(const VerticeShapes& shape)
{
	HRESULT hr = static_cast<HRESULT>(0L);

	
	if (mVertexShaders.count(shape.Shader()) == 0)
	{
		
		ID3DBlob* pVSBlob = nullptr;
		hr = CompileShaderFromFile(shape.Shader().c_str(), "VS", "vs_4_0", &pVSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}		
		ID3D11VertexShader* vertexShader = nullptr;
		hr = mD3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &vertexShader);
		if (FAILED(hr))
		{
			pVSBlob->Release();
			return hr;
		}		
		mVertexShaders.insert(pair<wstring, ID3D11VertexShader*>(shape.Shader(), vertexShader));	
		mImmediateContext->VSSetShader(vertexShader, nullptr, 0);		
		if (mLayouts.count(shape.Shader()) == 0)
		{			
			D3D11_INPUT_ELEMENT_DESC layout[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "INSTANCEPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			};
			UINT numElements = ARRAYSIZE(layout);

			
			ID3D11InputLayout* vertexLayout = nullptr;
			hr = mD3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
				pVSBlob->GetBufferSize(), &vertexLayout);
			pVSBlob->Release();
			if (FAILED(hr))
				return hr;
			
			mLayouts.insert(pair<wstring, ID3D11InputLayout*>(shape.Shader(), vertexLayout));			
			mImmediateContext->IASetInputLayout(vertexLayout);
		}
		else
		{		
			mImmediateContext->IASetInputLayout(mLayouts[shape.Shader()]);
		}
	}
	else
	{		
		mImmediateContext->VSSetShader(mVertexShaders[shape.Shader()], nullptr, 0);
	}

	
	if (mPixelShaders.count(shape.Shader()) == 0)
	{
	
		ID3DBlob* pPSBlob = nullptr;
		hr = CompileShaderFromFile(shape.Shader().c_str(), "PS", "ps_4_0", &pPSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		
		ID3D11PixelShader* pixelShader = nullptr;
		hr = mD3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pixelShader);
		pPSBlob->Release();
		if (FAILED(hr))
			return hr;

		
		mPixelShaders.insert(pair<wstring, ID3D11PixelShader*>(shape.Shader(), pixelShader));

	
		mImmediateContext->PSSetShader(pixelShader, nullptr, 0);
	}
	else
	{
	
		mImmediateContext->PSSetShader(mPixelShaders[shape.Shader()], nullptr, 0);
	}

	return static_cast<HRESULT>(0L);
}


void MainRender::CleanUpDevice()
{
	if (mImmediateContext) mImmediateContext->ClearState();
	
	for (const auto& buffer : mVertexBuffers)
	{
		buffer.second->Release();
	}
	
	for (const auto& buffer : mIndexBuffers)
	{
		buffer.second->Release();
	}
	
	for (const auto& buffer : mInstanceBuffers)
	{
		buffer.second->Release();
	}

	for (const auto& shader : mVertexShaders)
	{
		shader.second->Release();
	}

	for (const auto& shader : mPixelShaders)
	{
		shader.second->Release();
	}

	for (const auto& texture : mTextures)
	{
		texture.second->Release();
	}

	for (const auto& layout : mLayouts)
	{
		layout.second->Release();
	}

	if (mConstantBuffer) mConstantBuffer->Release();
	if (mDepthStencil) mDepthStencil->Release();
	if (mDepthStencilView) mDepthStencilView->Release();
	if (mRenderTargetView) mRenderTargetView->Release();
	if (mSwapChain1) mSwapChain1->Release();
	if (mSwapChain) mSwapChain->Release();
	if (mImmediateContext1) mImmediateContext1->Release();
	if (mImmediateContext) mImmediateContext->Release();
	if (mD3dDevice1) mD3dDevice1->Release();
	if (mD3dDevice) mD3dDevice->Release();
	if (mSampler) mSampler->Release();
	if (mRasteriserStateNoCull) mRasteriserStateNoCull->Release();
	if (mRasteriserStateBackCull) mRasteriserStateBackCull->Release();
	if (mDepthStencilLessThanEqual) mDepthStencilLessThanEqual->Release();
	if (mBlendStateAlpha) mBlendStateAlpha->Release();
	if (mBlendStateDisable) mBlendStateDisable->Release();

}


const int& MainRender::Width() const
{
	return mWidth;
}

const int& MainRender::Height() const
{
	return mHeight;
}

int WINAPI wWinMain(_In_ const HINSTANCE hInstance, _In_opt_ const HINSTANCE hPrevInstance, _In_ const LPWSTR lpCmdLine, _In_ const int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	
	MainRender renderer;
	renderer.SetWindow(g_hWnd);

	//Initisialises DirectX device, and cleans up device if there's a failure
	if (FAILED(renderer.InitDXDevice()))
	{
		renderer.CleanUpDevice();
		return 0;
	}

	//Initialises the scene with all required entities, cameras and lights
	MainRender gm(renderer.Width(), renderer.Height());
	gm.InitScene();

	//Main message loop
	MSG msg = { 0 };
	auto previousTime = chrono::high_resolution_clock::now();
	auto startTime = chrono::high_resolution_clock::now();
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//Calculates delta time
			auto currentTime = chrono::high_resolution_clock::now();
			const auto elapsedTimeTotal = currentTime - startTime;
			const auto elapsedTime = currentTime - previousTime;
			const float dt = (elapsedTime.count() / 1e+9);
			const float time = (elapsedTimeTotal.count() / 1e+9);

			//Update Render Loop
			gm.Update(dt);

			if (gm.ExitApplication())
			{
				renderer.CleanUpDevice();
				return 0;
			}
			if (FAILED(renderer.Render(gm.Entities(), gm.ActiveCamera(), time)))
			{
				renderer.CleanUpDevice();
				return 0;
			}
			previousTime = currentTime;
		}
		if (renderer.getClicked())
		{
			gm.ResetScene();
			renderer.setReset(false);
			
		}
		renderer.camsetting(renderer.cam);
	}

	//Cleanup device on exit
	renderer.CleanUpDevice();

	return static_cast<int>(msg.wParam);
}

void MainRender::camsetting(int ch)
{
	switch (ch) {
	case 1:
		activeCam = &cameras[0];
	
	case 2:
		activeCam = &cameras[1];
}

}

void MainRender::CheckInput(const float& dt)
{
	//Assign the state of keyboard to start 
	const auto kb = keyboard->GetState();


	if (kb.Escape)
	{
		isExit = true;
	}
	//Camera movement 
	if (kb.W && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(activeCam->Forward(), dt);
	}
	if (kb.A && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(XMFLOAT3(-activeCam->Right().x, -activeCam->Right().y, -activeCam->Right().z), dt);
	}
	if (kb.S && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(XMFLOAT3(-activeCam->Forward().x, -activeCam->Forward().y, -activeCam->Forward().z), dt);
	}
	if (kb.D && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(activeCam->Right(), dt);
	}
	if (kb.PageUp && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(activeCam->Up(), dt);
	}
	if (kb.PageDown && activeCam->AllowMovement() && kb.LeftControl)
	{
		activeCam->MoveCamera(XMFLOAT3(-activeCam->Up().x, -activeCam->Up().y, -activeCam->Up().z), dt);
	}

	//Camera rotation 
	if (kb.W && activeCam->AllowMovement() && !kb.LeftControl)
	{
		activeCam->RotateCamera(XMFLOAT3(-XMConvertToRadians(5), 0, 0), dt);
	}
	if (kb.A && activeCam->AllowMovement() && !kb.LeftControl)
	{
		activeCam->RotateCamera(XMFLOAT3(0, -XMConvertToRadians(5), 0), dt);
	}
	if (kb.S && activeCam->AllowMovement() && !kb.LeftControl)
	{
		activeCam->RotateCamera(XMFLOAT3(XMConvertToRadians(5), 0, 0), dt);
	}
	if (kb.D && activeCam->AllowMovement() && !kb.LeftControl)
	{
		activeCam->RotateCamera(XMFLOAT3(0, XMConvertToRadians(5), 0), dt);
	}


	//Cameras selection

	if (kb.F1)
	{
		activeCam = &cameras[0];
	}
	if (kb.F2)
	{
		activeCam = &cameras[1];
	}
	if (kb.F3)
	{
		activeCam = &cameras[2];
	}
	if (kb.F4)
	{
		activeCam = &cameras[3];
	}
	if (kb.F5)
	{
		activeCam = &cameras[4];
	}
	if (kb.F6)
	{
		//setwireframe 
	}
	if (kb.OemPeriod && kb.LeftShift && !isRocketLaunched)
	{
		rocket->RotateEntity(XMFLOAT3(0, 0, XMConvertToRadians(-5)), timescale, dt);
		launcher->RotateEntity(XMFLOAT3(0, 0, XMConvertToRadians(-5)), timescale, dt);
		rocketNoseCam->SetLookAt(rocket->Shapes()[1].WorldPos());
	}
	if (kb.OemComma && kb.LeftShift && !isRocketLaunched)
	{
		rocket->RotateEntity(XMFLOAT3(0, 0, XMConvertToRadians(5)), timescale, dt);
		launcher->RotateEntity(XMFLOAT3(0, 0, XMConvertToRadians(5)), timescale, dt);
		rocketNoseCam->SetLookAt(rocket->Shapes()[1].WorldPos());
	}
	if (kb.F11)
	{
		isRocketLaunched = true;
	}
	if (kb.R)
	{
		ResetScene();
	}
	if (kb.T)
	{
		timescale -= 0.05f;
		// to avoid goin in reverse 
		if (timescale <= 1) timescale = 1;
	}
	if (kb.LeftShift && kb.T)
	{
		timescale += 0.5f;
	}
}


void MainRender::CreateCameras()
{

	const float fov = XMConvertToRadians(60);
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

	//camera 1 Position
	Camera newCamera(true, XMFLOAT3(-2, 8, 0), XMFLOAT3(0, 0, 0), fov, aspectRatio, 0.1f, 100.0f, 1.0f, 10.0f);
	cameras.push_back(newCamera);

	//camera 2 Position
	newCamera = Camera(true, XMFLOAT3(-2.0f, 12, 2.0f), XMFLOAT3(XMConvertToRadians(90), 0, 0), fov, aspectRatio, 0.1f, 100.0f, 1.0f, 10.0f);
	cameras.push_back(newCamera);

	//camera 3 Position
	newCamera = Camera(false, XMFLOAT3(0.5f, 8.2f, 1.0f), XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 2.5f, 2.4f), fov, aspectRatio, 0.1f, 100.0f, 1.0f, 10.0f);
	cameras.push_back(newCamera);

	//camera 4 Position
	newCamera = Camera(false, XMFLOAT3(0.5f, 8.2f, 1.0f), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), fov, aspectRatio, 0.1f, 100.0f, 1.0f, 10.0f);
	cameras.push_back(newCamera);

	//camera 5 Position
	newCamera = Camera(false, XMFLOAT3(0.5f, 8.2f, 2.0f), XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 2.4f, 2.4f), fov, aspectRatio, 0.1f, 100.0f, 1.0f, 10.0f);
	cameras.push_back(newCamera);

	//Assign the camera pointers to cameras;
	activeCam = &cameras[0];
	rocketCam = &cameras[2];
	rocketNoseCam = &cameras[3];
	rocketBodyCam = &cameras[4];
}




void MainRender::CreateTerrain()
{

	vector<InstanceData> instances;
	for (int y = 0; y < 40; y++)
	{
		for (int z = 0; z < 30; z++)
		{
			for (int x = 0; x < 30; x++)
			{
				instances.emplace_back(InstanceData{ XMFLOAT3(x * -2, y * 2, z * 2) });
			}
		}
	}
	vector<InstanceData>* const instancesPointer = &instances;

	GameObject terrainObject(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(0.1f, 0.1f, 0.1f), "Terrain", 0.0f, 0.0f);
	terrainObject.AddShape(MeshType::CUBE, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1.0f, 1.0f, 1.0f), L"base.dds", L"normal.dds", L"displ.dds", L"InstancedShader.fx", "TerrainCube", &instances, false);
	entities.push_back(terrainObject);
	terrain = &terrainObject;
}



void MainRender::RocketCollision()
{

	const auto conePosF = rocket->Shapes()[1].WorldPos();
	const auto conePos = XMLoadFloat3(&conePosF);
	const auto terrainTransformF4 = terrain->Transform();
	const auto terrainTransform = XMLoadFloat4x4(&terrainTransformF4);
	vector<InstanceData> terrainCubes = terrain->Shapes()[0].Instances();
	vector<InstanceData> cubesTodestroy;
	auto collided = false;

	//Checks for collision
	for (const auto& cube : terrainCubes)
	{
		const XMVECTOR cubePos = XMVector3Transform(XMLoadFloat3(&cube.Pos), terrainTransform);
		XMFLOAT3 distance;
		XMStoreFloat3(&distance, XMVector3Length(cubePos - conePos));
		if (distance.x < collisionRadius)
		{
			collided = true;
			for (const auto& cube : terrainCubes)
			{
				const auto cubePos = XMVector3Transform(XMLoadFloat3(&cube.Pos), terrainTransform);
				XMFLOAT3 distance;
				XMStoreFloat3(&distance, XMVector3Length(cubePos - conePos));
				if (distance.x < explosionRadius)
				{
					cubesTodestroy.push_back(cube);

				}
			}
		}
	}
	InitiateExplosions(cubesTodestroy, conePosF);
}


void MainRender::SpawnExplosion(const XMFLOAT3& position)
{

	vector<InstanceData> smokeInstances;

	for (int i = 0; i < 600; i++)
	{
		smokeInstances.emplace_back(InstanceData{ XMFLOAT3(0, 0, i) });
	}
	const vector<InstanceData>* const smokeInstancesPointer = &smokeInstances;


	vector<InstanceData> explosionInstances;

	for (int i = 0; i < 300; i++)
	{
		explosionInstances.emplace_back(InstanceData{ XMFLOAT3(0, 0, i) });
	}
	vector<InstanceData>* const explosionInstancesPointer = &explosionInstances;

	GameObject smokeObject(XMFLOAT3(position.x, position.y - 1, position.z), XMFLOAT3(0, 0, 0), XMFLOAT3(.3f, .3f, .3f), "Smoke", 0.0f, 0.0f);
	smokeObject.AddShape(MeshType::CYLINDER, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1), L"smoke-effect.dds", L"", L"", L"SmokeParticle.fx", "SmokeParticle", smokeInstancesPointer, true);
	entities.push_back(smokeObject);
	smoke = &smokeObject;

	GameObject explosionObject(XMFLOAT3(position.x, position.y - 1, position.z), XMFLOAT3(0, 0, 0), XMFLOAT3(.5f, .5f, .5f), "Explosion", 0.0f, 0.0f);
	explosionObject.AddShape(MeshType::CYLINDER, XMFLOAT3(0, 2, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1.5f, 1.5f, 1.5f), L"Fire-texture.dds", L"", L"", L"ExplosionParticle.fx", "ExplosionParticle", explosionInstancesPointer, true);
	explosionObject.AddLight(XMFLOAT4(0.0f, 5.0f, 0.0f, 1.0f), XMFLOAT4(1.9f, 0.3f, 0.2f, 1.0f), "ExplosionLight");
	entities.push_back(explosionObject);
	explosion = &explosionObject;
	timeSinceExplosion = 0.0f;

	SetEntityPointers();
}

void MainRender::SetEntityPointers()
{

	for (auto& entity : entities)
	{
		if (entity.Name() == "Terrain")
		{
			terrain = &entity;
		}
		if (entity.Name() == "Rocket")
		{
			rocket = &entity;
		}
		if (entity.Name() == "Sun")
		{
			sun = &entity;
		}
		if (entity.Name() == "Moon")
		{
			moon = &entity;
		}
		if (entity.Name() == "Explosion")
		{
			explosion = &entity;
		}
		if (entity.Name() == "Launcher")
		{
			launcher = &entity;
		}
		if (entity.Name() == "Smoke")
		{
			smoke = &entity;
		}
	}
}

void MainRender::UpdateRocket(const float& dt)
{
	if (!isEngineOn)
	{

		vector<InstanceData> instances;
		for (int i = 0; i < 200; i++)
		{
			instances.emplace_back(InstanceData{ XMFLOAT3(0, 0, i) });
		}

		rocket->AddShape(MeshType::QUAD, XMFLOAT3(0, -4.0f, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1.0f, 1), L"Fire-texture.dds", L"", L"", L"EngineParticle.fx", "RocketEngineFlame", &instances, true);
		rocket->AddLight(XMFLOAT4(0, -4.5f, 0, 1.0f), XMFLOAT4(0.5f, 0.3f, 0.2f, 1.0f), "RocketEngineLight");
		isEngineOn = true;
	}

	rocket->MoveEntity(rocket->Up(), timescale, dt);

	if (rocket->Up().y >= -0.999f)
	{

		rocket->RotateEntity(XMFLOAT3(0, 0, XMConvertToRadians(15)), timescale * 1.5f, dt);
	}

	rocketCam->MoveCamera(rocket->Up(), timescale * rocket->Speed(), dt);
	rocketCam->SetLookAt(rocket->Position());
	rocketNoseCam->SetPosition(XMFLOAT3(rocket->Shapes()[1].WorldPos().x + 0.3f, rocket->Shapes()[1].WorldPos().y + 1.0f, rocket->Shapes()[1].WorldPos().z - 0.2f));
	rocketNoseCam->SetLookAt(rocket->Shapes()[1].WorldPos());
	rocketBodyCam->MoveCamera(rocket->Up(), timescale * rocket->Speed(), dt);
	rocketBodyCam->SetLookAt(rocket->Shapes()[0].WorldPos());
}

void MainRender::InitiateExplosions(const vector<InstanceData>& data, const XMFLOAT3& conePos)
{

	if (data.size() > 0)
	{
		entities.erase(remove(entities.begin(), entities.end(), *rocket), entities.end());
		rocket = nullptr;
		terrain->RemoveInstancesFromShape("TerrainCube", data);
		isRocketLaunched = false;
		isEngineOn = false;
		SpawnExplosion(conePos);
	}
}

void MainRender::ResetRocketCams()const
{
	rocketCam->SetPosition(XMFLOAT3(0.5f, 8.2f, 1.0f));
	rocketCam->SetLookAt(rocket->Position());
	rocketNoseCam->SetPosition(XMFLOAT3(rocket->Shapes()[1].WorldPos().x + 0.3f, rocket->Shapes()[1].WorldPos().y + 1.0f, rocket->Shapes()[1].WorldPos().z - 0.2f));
	rocketNoseCam->SetLookAt(rocket->Shapes()[1].WorldPos());
	rocketBodyCam->SetPosition(XMFLOAT3(0.5f, 8.2f, 2.5f));
	rocketBodyCam->SetLookAt(rocket->Shapes()[0].WorldPos());
}



MainRender::MainRender(const int& width, const int& height) :
	timescale(1.0f),
	width(width),
	height(height),
	isExit(false),
	isRocketLaunched(false),
	isEngineOn(false)
{
}


void MainRender::InitScene()
{
	if (!keyboard)
	{
		keyboard = make_unique<Keyboard>();
	}

	if (cameras.size() > 0)
	{
		cameras.clear();
	}
	if (entities.size() > 0)
	{
		entities.clear();
	}


	entities.reserve(8);

	// Function calls for Drawing 
	CreateCameras();
	CreateTerrain();
	Launcher();
	Sun();
	Moon();
	Rocket();
	SetEntityPointers();


}

void MainRender::Rocket()
{
	GameObject rocketObject;
	rocketObject = GameObject(XMFLOAT3(-0.5f, 8.2f, 3.0f), XMFLOAT3(0, 0, 0), XMFLOAT3(0.1f, 0.1f, 0.1f), "Rocket", 1.5f, 2.0f);
	rocketObject.AddShape(MeshType::CYLINDER, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 4.0f, 1), L"metal_diff.dds", L"metal_nor.dds", L"metal_disp.dds", L"BumpShader.fx", "Cylinder", nullptr, false);
	rocketObject.AddShape(MeshType::CONE, XMFLOAT3(0, 6.0f, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 2.0f, 1), L"skymap2.dds", L"", L"", L"ReflectiveShiny.fx", "Cone", nullptr, false);
	entities.push_back(rocketObject);
	rocket = &rocketObject;
	SetEntityPointers();
	ResetRocketCams();
}
void MainRender::Launcher()
{
	GameObject launcherobj;
	launcherobj = GameObject(XMFLOAT3(-0.65f, 8.2f, 3.0f), XMFLOAT3(0, 0, 0), XMFLOAT3(0.05, 0.1, 0.05), "Launcher", 1.0f, 2.0f);
	launcherobj.AddShape(MeshType::CYLINDER, XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(1, 4.0f, 1), L"M-diffuse.dds", L"", L"", L"BumpShader.fx", "Cylinder", nullptr, false);
	entities.push_back(launcherobj);
	launcher = &launcherobj;
}


void MainRender::Sun() {
	GameObject sunobj;
	sunobj = GameObject(XMFLOAT3(10, 3.2, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(10, 10, 10), "Sun", 1.0f, 2.0f);
	sunobj.AddShape(MeshType::CONE, XMFLOAT3(0, 2, 5.5f), XMFLOAT3(-30, 0, 0), XMFLOAT3(0.5, 0.5, 0.5), L"Sun.dds", L"", L"", L"EffectShader.fx", "Cone", nullptr, false);
	sunobj.AddLight(XMFLOAT4(0.5f, 0.5f, 5.0f, 1.0f), XMFLOAT4(3.5f, 2, 0.5f, 4.0f), "SunLight");
	entities.push_back(sunobj);
	sun = &sunobj;
}

void MainRender::Moon() {

	GameObject moonObject;
	moonObject = GameObject(XMFLOAT3(10, 3.2, 0), XMFLOAT3(0, 0, 0), XMFLOAT3(10, 10, 10), "Moon", 1.0f, 2.0f);
	moonObject.AddShape(MeshType::CONE, XMFLOAT3(0, 2, -5.5f), XMFLOAT3(30, 0, 0), XMFLOAT3(0.25, 0.25, 0.25), L"Moon.dds", L"", L"", L"EffectShader.fx", "Cone", nullptr, false);
	moonObject.AddLight(XMFLOAT4(-0.5f, -0.5f, -5.0f, -1.0f), XMFLOAT4(0.5f, 2.5f, 3.5f, 3.5f), "MoonLight");
	entities.push_back(moonObject);
	moon = &moonObject;
}

void MainRender::ResetScene()
{
	isReset = true;
	isRocketLaunched = false;
	isEngineOn = false;
	timescale = 1;
	InitScene();
}




void MainRender::Update(const float& dt)
{

	CheckInput(dt);




	if (sun)
	{
		sun->RotateAroundPoint(XMFLOAT3(10.0f, 3.2f, 2.0f), XMFLOAT3(0, XMConvertToRadians(5), 0), timescale, dt);
	}


	if (moon)
	{
		moon->RotateAroundPoint(XMFLOAT3(10.0f, 3.2f, 0.0f), XMFLOAT3(0, XMConvertToRadians(5), 0), timescale, dt);
	}


	if (isRocketLaunched && rocket)
	{
		UpdateRocket(dt);
	}


	if (rocket)
	{
		RocketCollision();
	}
	else
	{
		launcher->ResetRotation();
		Rocket();
	}

	if (smoke)
	{
		timeSinceExplosion += (dt * timescale);
		if (timeSinceExplosion >= 3.0f && explosion)
		{
			entities.erase(remove(entities.begin(), entities.end(), *explosion), entities.end());
			explosion = nullptr;
			SetEntityPointers();
		}
		if (timeSinceExplosion >= 6.0f)
		{
			entities.erase(remove(entities.begin(), entities.end(), *smoke), entities.end());
			smoke = nullptr;
			SetEntityPointers();
		}
	}
}

const Camera* const MainRender::ActiveCamera() const
{
	return activeCam;
}

const vector<GameObject>& MainRender::Entities() const
{
	return entities;
}

const bool& MainRender::ExitApplication() const
{
	return isExit;
}

