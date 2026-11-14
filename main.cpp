#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

static ID3D11Device* g_device=nullptr;
static ID3D11DeviceContext* g_context=nullptr;
static IDXGISwapChain* g_swapChain=nullptr;
static ID3D11RenderTargetView* g_mainRTV=nullptr;
static HWND g_hwnd=nullptr;

struct AppItem { std::string name; std::string version="v1.0"; std::wstring path; bool favorite=false; };
static std::vector<AppItem> g_apps;
static int g_page=0, g_filter=0;
static char g_search[128]={}, g_newName[96]={};
static std::wstring g_newPath;
static bool g_autoClose=false, g_addPopup=false;
static const ImVec4 BLUE(0.30f,0.48f,1.0f,1.0f);

static void CreateRenderTarget(){ID3D11Texture2D* b=nullptr;g_swapChain->GetBuffer(0,IID_PPV_ARGS(&b));if(b){g_device->CreateRenderTargetView(b,nullptr,&g_mainRTV);b->Release();}}
static void CleanupRenderTarget(){if(g_mainRTV){g_mainRTV->Release();g_mainRTV=nullptr;}}
static bool CreateDeviceD3D(HWND h){
 DXGI_SWAP_CHAIN_DESC d{};d.BufferCount=2;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=h;d.SampleDesc.Count=1;d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 D3D_FEATURE_LEVEL l{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
 if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&d,&g_swapChain,&g_device,&l,&g_context)))return false;CreateRenderTarget();return true;
}
static void CleanupDeviceD3D(){CleanupRenderTarget();if(g_swapChain){g_swapChain->Release();g_swapChain=nullptr;}if(g_context){g_context->Release();g_context=nullptr;}if(g_device){g_device->Release();g_device=nullptr;}}
static std::wstring PickExe(){
 wchar_t file[MAX_PATH]=L"";OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=g_hwnd;o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.lpstrFilter=L"Applications (*.exe)\0*.exe\0\0";o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 return GetOpenFileNameW(&o)?std::wstring(file):std::wstring();
}
static void Launch(const AppItem& a){if(a.path.empty()||!std::filesystem::exists(a.path))return;auto dir=std::filesystem::path(a.path).parent_path().wstring();ShellExecuteW(g_hwnd,L"open",a.path.c_str(),nullptr,dir.c_str(),SW_SHOWNORMAL);if(g_autoClose)PostMessageW(g_hwnd,WM_CLOSE,0,0);}
static void Style(){
 ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();s.WindowRounding=0;s.ChildRounding=12;s.FrameRounding=9;s.PopupRounding=10;s.ScrollbarRounding=8;s.WindowPadding=ImVec2(18,16);s.FramePadding=ImVec2(12,9);s.ItemSpacing=ImVec2(10,10);s.ChildBorderSize=1;
 auto*c=s.Colors;c[ImGuiCol_WindowBg]=ImVec4(.025f,.032f,.048f,1);c[ImGuiCol_ChildBg]=ImVec4(.055f,.066f,.088f,.91f);c[ImGuiCol_Border]=ImVec4(.18f,.20f,.25f,.65f);c[ImGuiCol_FrameBg]=ImVec4(.10f,.12f,.16f,.92f);c[ImGuiCol_FrameBgHovered]=ImVec4(.15f,.18f,.24f,1);c[ImGuiCol_Button]=ImVec4(.12f,.15f,.21f,.9f);c[ImGuiCol_ButtonHovered]=ImVec4(.20f,.26f,.39f,1);c[ImGuiCol_ButtonActive]=ImVec4(.25f,.34f,.55f,1);c[ImGuiCol_CheckMark]=BLUE;c[ImGuiCol_Header]=ImVec4(.18f,.25f,.42f,.8f);
}
static void DrawBackdrop(){
 ImDrawList*d=ImGui::GetBackgroundDrawList();ImVec2 s=ImGui::GetIO().DisplaySize;
 d->AddRectFilledMultiColor(ImVec2(0,0),s,IM_COL32(9,12,19,255),IM_COL32(13,24,55,255),IM_COL32(7,12,19,255),IM_COL32(18,10,31,255));
 d->AddCircleFilled(ImVec2(s.x*.72f,-s.y*.05f),s.y*.82f,IM_COL32(34,69,166,75),80);
 d->AddCircleFilled(ImVec2(s.x*.08f,s.y*.98f),s.y*.52f,IM_COL32(105,44,151,55),80);
 ImVec2 c(s.x*.81f,s.y*.72f);float r=s.y*.47f;
 d->AddCircle(c,r,IM_COL32(113,101,40,24),100,18);d->AddCircle(c,r*.82f,IM_COL32(38,61,123,24),100,12);
 d->AddText(ImGui::GetFont(),s.y*.33f,ImVec2(c.x-r*.73f,c.y-r*.25f),"CS2",IM_COL32(125,111,45,19));
}
static bool NavButton(const char*label,bool active,ImVec2 size){if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.15f,.22f,.42f,.95f));bool hit=ImGui::Button(label,size);if(active)ImGui::PopStyleColor();return hit;}
static void AddCard(int idx,float width){
 auto&a=g_apps[idx];ImGui::PushID(idx);ImGui::BeginChild("card",ImVec2(width,145),true);
 ImGui::TextColored(ImVec4(.92f,.95f,1,1),"%s",a.name.c_str());
 ImGui::Dummy(ImVec2(0,9));ImGui::TextDisabled("%s",a.version.c_str());ImGui::SameLine();ImGui::TextColored(BLUE,a.path.empty()?"ДОБАВИТЬ EXE":"УСТАНОВЛЕНО");
 ImGui::SetCursorPosY(ImGui::GetWindowHeight()-40);
 if(ImGui::SmallButton(a.favorite?"★":"☆"))a.favorite=!a.favorite;
 ImGui::SameLine(ImGui::GetContentRegionAvail().x-64);if(ImGui::SmallButton("ОТКРЫТЬ"))Launch(a);
 ImGui::EndChild();ImGui::PopID();
}
static void DrawAddPopup(){
 if(g_addPopup){ImGui::OpenPopup("Добавить свой проект");g_addPopup=false;}
 if(ImGui::BeginPopupModal("Добавить свой проект",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::Text("Добавь собственную вкладку в THE WALL");ImGui::SetNextItemWidth(300);ImGui::InputTextWithHint("##name","Название проекта",g_newName,sizeof(g_newName));
  ImGui::Dummy(ImVec2(0,5));if(ImGui::Button("Выбрать EXE",ImVec2(130,34))){g_newPath=PickExe();}
  ImGui::SameLine();ImGui::TextDisabled("%s",g_newPath.empty()?"Файл не выбран":std::filesystem::path(g_newPath).filename().string().c_str());
  ImGui::Dummy(ImVec2(0,8));
  if(ImGui::Button("Добавить",ImVec2(130,36))&&g_newName[0]&& !g_newPath.empty()){
   AppItem a;a.name=g_newName;a.path=g_newPath;g_apps.push_back(a);g_newName[0]=0;g_newPath.clear();ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();if(ImGui::Button("Отмена",ImVec2(100,36))){g_newName[0]=0;g_newPath.clear();ImGui::CloseCurrentPopup();}
  ImGui::EndPopup();
 }
}
static void DrawUI(){
 ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
 ImGui::Begin("THE WALL##root",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBackground);
 ImGui::BeginChild("topbar",ImVec2(0,52),false);
 ImGui::SetNextItemWidth(285);ImGui::InputTextWithHint("##search","Поиск проектов",g_search,sizeof(g_search));
 ImGui::SameLine(ImGui::GetWindowWidth()-360);ImGui::TextDisabled("⚙  Настройки");ImGui::SameLine();ImGui::TextColored(ImVec4(.75f,.55f,1,1),"●");ImGui::SameLine();ImGui::Text("THE WALL");ImGui::SameLine();if(ImGui::SmallButton("_"))ShowWindow(g_hwnd,SW_MINIMIZE);ImGui::SameLine();if(ImGui::SmallButton("×"))PostMessageW(g_hwnd,WM_CLOSE,0,0);
 ImGui::EndChild();
 ImGui::Dummy(ImVec2(0,6));
 ImGui::Text("Мои проекты");ImGui::SameLine();ImGui::TextColored(BLUE,"%d",(int)g_apps.size());ImGui::SameLine();ImGui::TextDisabled("   добавляй свои приложения в библиотеку");
 ImGui::SameLine(ImGui::GetWindowWidth()-145);if(ImGui::Button("+ Добавить",ImVec2(125,34)))g_addPopup=true;
 ImGui::Dummy(ImVec2(0,8));
 const char* filters[]={"Все проекты","Избранное","Добавленные"};
 for(int i=0;i<3;i++){if(i)ImGui::SameLine();if(NavButton(filters[i],g_filter==i,ImVec2(118,34)))g_filter=i;}
 ImGui::Dummy(ImVec2(0,12));
 float avail=ImGui::GetContentRegionAvail().x;float gap=12;float cardW=(avail-gap*3)/4.0f;if(cardW<150)cardW=(avail-gap)/2.0f;
 int shown=0;
 for(int i=0;i<(int)g_apps.size();i++){
  auto&a=g_apps[i];if(g_filter==1&&!a.favorite)continue;if(g_filter==2&&a.path.empty())continue;
  if(g_search[0]&&a.name.find(g_search)==std::string::npos)continue;
  if(shown%4)ImGui::SameLine(0,gap);AddCard(i,cardW);shown++;
 }
 if(shown==0){ImGui::BeginChild("empty",ImVec2(0,190),true);ImGui::Dummy(ImVec2(0,30));ImGui::SetCursorPosX(20);ImGui::Text("Здесь пока пусто");ImGui::SetCursorPosX(20);ImGui::TextDisabled("Нажми «+ Добавить», чтобы создать свою вкладку и выбрать EXE.");ImGui::EndChild();}
 ImGui::SetCursorPosY(ImGui::GetWindowHeight()-66);
 ImGui::BeginChild("bottomnav",ImVec2(0,54),true);
 float navW=130;float start=(ImGui::GetWindowWidth()-navW*3)/2;
 ImGui::SetCursorPosX(start);if(NavButton("⌂  Проекты",g_page==0,ImVec2(navW,38)))g_page=0;ImGui::SameLine();if(NavButton("＋  Добавить",g_page==1,ImVec2(navW,38))){g_page=1;g_addPopup=true;}ImGui::SameLine();if(NavButton("☆  Избранное",g_page==2,ImVec2(navW,38))){g_page=2;g_filter=1;}
 ImGui::EndChild();DrawAddPopup();ImGui::End();
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static LRESULT WINAPI WndProc(HWND h,UINT m,WPARAM w,LPARAM l){if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;switch(m){case WM_SIZE:if(g_device&&w!=SIZE_MINIMIZED){CleanupRenderTarget();g_swapChain->ResizeBuffers(0,(UINT)LOWORD(l),(UINT)HIWORD(l),DXGI_FORMAT_UNKNOWN,0);CreateRenderTarget();}return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int){
 WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,WndProc,0,0,inst,nullptr,nullptr,nullptr,nullptr,L"TheWallWnd",nullptr};RegisterClassExW(&wc);
 g_hwnd=CreateWindowW(wc.lpszClassName,L"THE WALL",WS_OVERLAPPEDWINDOW,100,100,1180,780,nullptr,nullptr,inst,nullptr);
 if(!CreateDeviceD3D(g_hwnd)){CleanupDeviceD3D();UnregisterClassW(wc.lpszClassName,inst);return 1;}
 ShowWindow(g_hwnd,SW_SHOWDEFAULT);UpdateWindow(g_hwnd);IMGUI_CHECKVERSION();ImGui::CreateContext();ImGuiIO&io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",18,nullptr,io.Fonts->GetGlyphRangesCyrillic());
 Style();ImGui_ImplWin32_Init(g_hwnd);ImGui_ImplDX11_Init(g_device,g_context);
 MSG msg{};bool done=false;while(!done){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);if(msg.message==WM_QUIT)done=true;}if(done)break;
 ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();DrawBackdrop();DrawUI();ImGui::Render();const float clear[4]={.025f,.032f,.048f,1};g_context->OMSetRenderTargets(1,&g_mainRTV,nullptr);g_context->ClearRenderTargetView(g_mainRTV,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());g_swapChain->Present(1,0);}
 ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();CleanupDeviceD3D();DestroyWindow(g_hwnd);UnregisterClassW(wc.lpszClassName,inst);return 0;
}