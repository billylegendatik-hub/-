#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#pragma comment(lib, "d3d11.lib")

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swapChain = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static HWND g_hwnd = nullptr;
static std::wstring g_exePath;
static int g_page = 0;
static bool g_autoClose = false;
static char g_search[128] = {};

static void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) { g_device->CreateRenderTargetView(backBuffer, nullptr, &g_mainRTV); backBuffer->Release(); }
}
static void CleanupRenderTarget() { if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; } }
static bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2,
        D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &level, &g_context);
    if (FAILED(hr)) return false;
    CreateRenderTarget(); return true;
}
static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}
static std::wstring PickExecutable() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Applications (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) return file;
    return L"";
}
static void LaunchSelected() {
    if (g_exePath.empty() || !std::filesystem::exists(g_exePath)) return;
    ShellExecuteW(g_hwnd, L"open", g_exePath.c_str(), nullptr,
        std::filesystem::path(g_exePath).parent_path().c_str(), SW_SHOWNORMAL);
    if (g_autoClose) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
}
static void ApplyStyle() {
    ImGui::StyleColorsDark(); ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10; s.ChildRounding = 8; s.FrameRounding = 6; s.PopupRounding = 6;
    s.ScrollbarRounding = 8; s.GrabRounding = 6; s.WindowPadding = ImVec2(18, 18);
    s.FramePadding = ImVec2(12, 8); s.ItemSpacing = ImVec2(10, 10);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(.055f,.063f,.082f,1);
    c[ImGuiCol_ChildBg] = ImVec4(.075f,.086f,.110f,1);
    c[ImGuiCol_Border] = ImVec4(.15f,.17f,.21f,1);
    c[ImGuiCol_FrameBg] = ImVec4(.11f,.13f,.17f,1);
    c[ImGuiCol_Button] = ImVec4(.22f,.25f,.32f,1);
    c[ImGuiCol_ButtonHovered] = ImVec4(.30f,.35f,.45f,1);
    c[ImGuiCol_ButtonActive] = ImVec4(.38f,.44f,.58f,1);
    c[ImGuiCol_CheckMark] = ImVec4(.48f,.67f,1,1);
    c[ImGuiCol_Header] = ImVec4(.19f,.25f,.36f,1);
}
static void DrawUI() {
    ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("THE WALL", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::BeginChild("sidebar", ImVec2(190, 0), true);
    ImGui::Dummy(ImVec2(0,8));
    ImGui::TextColored(ImVec4(.55f,.70f,1,1), "THE WALL"); ImGui::SameLine(); ImGui::TextDisabled("/ LOADER");
    ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));
    const char* nav[] = { "  Home", "  Library", "  Settings" };
    for (int i=0;i<3;i++) { if (g_page==i) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.19f,.25f,.36f,1));
        if (ImGui::Button(nav[i], ImVec2(-1,40))) g_page=i;
        if (g_page==i) ImGui::PopStyleColor(); }
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-48); ImGui::TextDisabled("build 1.0.0  |  DX11");
    ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("content", ImVec2(0,0), false);
    ImGui::TextDisabled("WORKSPACE  /  %s", g_page==0 ? "HOME" : g_page==1 ? "LIBRARY" : "SETTINGS");
    ImGui::Separator(); ImGui::Dummy(ImVec2(0,8));
    if (g_page == 0) {
        ImGui::Text("Welcome back."); ImGui::TextDisabled("Your desktop launcher, in one place."); ImGui::Dummy(ImVec2(0,14));
        ImGui::BeginChild("featured", ImVec2(0,190), true);
        ImGui::TextColored(ImVec4(.55f,.70f,1,1), "FEATURED"); ImGui::Dummy(ImVec2(0,6));
        ImGui::Text("Local application"); ImGui::TextDisabled("Choose an executable and launch it from this workspace.");
        ImGui::Dummy(ImVec2(0,16));
        if (ImGui::Button("Select .exe", ImVec2(130,38))) { auto p=PickExecutable(); if(!p.empty()) g_exePath=p; }
        ImGui::SameLine(); ImGui::BeginDisabled(g_exePath.empty());
        if (ImGui::Button("Launch", ImVec2(110,38))) LaunchSelected(); ImGui::EndDisabled();
        ImGui::EndChild(); ImGui::Dummy(ImVec2(0,12));
        ImGui::Text("CURRENT SELECTION");
        ImGui::BeginChild("selection", ImVec2(0,75), true);
        ImGui::TextWrapped("%s", g_exePath.empty() ? "No application selected" : std::filesystem::path(g_exePath).filename().string().c_str());
        ImGui::TextDisabled("%s", g_exePath.empty() ? "Select a local .exe to get started" : "Local executable");
        ImGui::EndChild();
    } else if (g_page == 1) {
        ImGui::Text("Library"); ImGui::TextDisabled("Local apps you choose to launch."); ImGui::Dummy(ImVec2(0,10));
        ImGui::SetNextItemWidth(260); ImGui::InputTextWithHint("##search", "Search library...", g_search, sizeof(g_search));
        ImGui::Dummy(ImVec2(0,10));
        ImGui::BeginChild("appcard", ImVec2(0,105), true);
        ImGui::Text("Custom application"); ImGui::TextDisabled("Add an executable from your PC"); ImGui::SameLine(ImGui::GetContentRegionAvail().x-110);
        if (ImGui::Button("Browse .exe", ImVec2(110,32))) { auto p=PickExecutable(); if(!p.empty()) g_exePath=p; }
        ImGui::EndChild();
        if (!g_exePath.empty()) { ImGui::TextWrapped("Selected: %s", std::filesystem::path(g_exePath).filename().string().c_str()); }
    } else {
        ImGui::Text("Preferences"); ImGui::TextDisabled("Launcher behavior"); ImGui::Dummy(ImVec2(0,12));
        ImGui::Checkbox("Close launcher after starting an app", &g_autoClose);
        ImGui::Dummy(ImVec2(0,12)); ImGui::TextDisabled("This build launches a user-selected local executable. It does not inject into or modify other processes.");
    }
    ImGui::EndChild(); ImGui::End();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd,msg,wParam,lParam)) return 1;
    switch(msg) {
    case WM_SIZE: if(g_device && wParam!=SIZE_MINIMIZED) { CleanupRenderTarget(); g_swapChain->ResizeBuffers(0,(UINT)LOWORD(lParam),(UINT)HIWORD(lParam),DXGI_FORMAT_UNKNOWN,0); CreateRenderTarget(); } return 0;
    case WM_SYSCOMMAND: if((wParam&0xfff0)==SC_KEYMENU) return 0; break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd,msg,wParam,lParam);
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,WndProc,0,0,hInstance,nullptr,nullptr,nullptr,nullptr,L"TheWallWnd",nullptr};
    RegisterClassExW(&wc);
    g_hwnd=CreateWindowW(wc.lpszClassName,L"THE WALL",WS_OVERLAPPEDWINDOW,100,100,1000,650,nullptr,nullptr,wc.hInstance,nullptr);
    if(!CreateDeviceD3D(g_hwnd)) { CleanupDeviceD3D(); UnregisterClassW(wc.lpszClassName,wc.hInstance); return 1; }
    ShowWindow(g_hwnd,SW_SHOWDEFAULT); UpdateWindow(g_hwnd);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io=ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyStyle(); ImGui_ImplWin32_Init(g_hwnd); ImGui_ImplDX11_Init(g_device,g_context);
    bool done=false; MSG msg{};
    while(!done) {
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if(msg.message==WM_QUIT) done=true; }
        if(done) break;
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); DrawUI(); ImGui::Render();
        const float clear[4]={.055f,.063f,.082f,1}; g_context->OMSetRenderTargets(1,&g_mainRTV,nullptr); g_context->ClearRenderTargetView(g_mainRTV,clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); g_swapChain->Present(1,0);
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); CleanupDeviceD3D(); DestroyWindow(g_hwnd); UnregisterClassW(wc.lpszClassName,wc.hInstance); return 0;
}
