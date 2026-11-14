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
 ImGui::StyleColorsDark();
 auto& s=ImGui::GetStyle();
 s.WindowRounding=0; s.ChildRounding=14; s.FrameRounding=9; s.PopupRounding=12;
 s.ScrollbarRounding=10; s.GrabRounding=8; s.WindowPadding=ImVec2(22,18);
 s.FramePadding=ImVec2(13,10); s.ItemSpacing=ImVec2(12,12); s.ItemInnerSpacing=ImVec2(8,8);
 s.ChildBorderSize=1; s.PopupBorderSize=1; s.FrameBorderSize=0;
 s.Colors[ImGuiCol_WindowBg]=ImVec4(.025f,.031f,.048f,1);
 s.Colors[ImGuiCol_ChildBg]=ImVec4(.055f,.067f,.095f,.86f);
 s.Colors[ImGuiCol_Border]=ImVec4(.22f,.27f,.38f,.48f);
 s.Colors[ImGuiCol_FrameBg]=ImVec4(.09f,.115f,.16f,.96f);
 s.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.13f,.17f,.24f,1);
 s.Colors[ImGuiCol_FrameBgActive]=ImVec4(.16f,.21f,.31f,1);
 s.Colors[ImGuiCol_Button]=ImVec4(.12f,.17f,.27f,.95f);
 s.Colors[ImGuiCol_ButtonHovered]=ImVec4(.20f,.29f,.48f,1);
 s.Colors[ImGuiCol_ButtonActive]=ImVec4(.25f,.36f,.59f,1);
 s.Colors[ImGuiCol_CheckMark]=BLUE;
 s.Colors[ImGuiCol_Header]=ImVec4(.16f,.23f,.39f,.8f);
 s.Colors[ImGuiCol_Text]=ImVec4(.91f,.94f,1,1);
 s.Colors[ImGuiCol_TextDisabled]=ImVec4(.49f,.56f,.69f,1);
}
static void DrawBackdrop(){
 ImDrawList*d=ImGui::GetBackgroundDrawList();ImVec2 s=ImGui::GetIO().DisplaySize;
 d->AddRectFilledMultiColor(ImVec2(0,0),s,IM_COL32(9,12,19,255),IM_COL32(13,24,55,255),IM_COL32(7,12,19,255),IM_COL32(18,10,31,255));
 d->AddCircleFilled(ImVec2(s.x*.72f,-s.y*.05f),s.y*.82f,IM_COL32(34,69,166,75),80);
 d->AddCircleFilled(ImVec2(s.x*.08f,s.y*.98f),s.y*.52f,IM_COL32(105,44,151,55),80);
 ImVec2 c(s.x*.81f,s.y*.72f);float r=s.y*.47f;
 d->AddCircle(c,r,IM_COL32(113,101,40,24),100,18);d->AddCircle(c,r*.82f,IM_COL32(38,61,123,24),100,12);
 d->AddText(ImGui::GetFont(),s.y*.33f,ImVec2(c.x-r*.73f,c.y-r*.25f),IM_COL32(125,111,45,19),"CS2");
}
static bool NavButton(const char*label,bool active,ImVec2 size){if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.15f,.22f,.42f,.95f));bool hit=ImGui::Button(label,size);if(active)ImGui::PopStyleColor();return hit;}
static void AddCard(int idx,float width){
 auto& a=g_apps[idx]; ImGui::PushID(idx);
 ImGui::BeginChild("project-card",ImVec2(width,166),true,ImGuiWindowFlags_NoScrollbar);
 ImVec2 p=ImGui::GetWindowPos(), z=ImGui::GetWindowSize(); ImDrawList* d=ImGui::GetWindowDrawList();
 ImU32 tint= a.favorite?IM_COL32(91,72,145,58):IM_COL32(55,91,164,52);
 d->AddRectFilledMultiColor(p,ImVec2(p.x+z.x,p.y+z.y),IM_COL32(24,31,48,245),tint,IM_COL32(10,15,25,245),IM_COL32(13,18,29,245));
 d->AddCircleFilled(ImVec2(p.x+z.x-5,p.y+8),55,tint,48);
 d->AddRect(p,ImVec2(p.x+z.x,p.y+z.y),IM_COL32(91,116,165,80),14,0,1.0f);
 ImGui::Dummy(ImVec2(0,3));
 ImGui::TextColored(ImVec4(.94f,.96f,1,1),"%s",a.name.c_str());
 ImGui::Dummy(ImVec2(0,7));
 ImGui::TextDisabled("%s",a.version.c_str()); ImGui::SameLine();
 ImGui::TextColored(BLUE,a.path.empty()?"НЕ ВЫБРАН ФАЙЛ":"ГОТОВО К ЗАПУСКУ");
 ImGui::SetCursorPosY(111);
 ImGui::TextColored(ImVec4(.98f,.72f,.32f,1),"%s",a.favorite?"★ В ИЗБРАННОМ":"THE WALL  /  APP");
 ImGui::SetCursorPosY(132);
 if(ImGui::SmallButton(a.favorite?"★":"☆"))a.favorite=!a.favorite;
 ImGui::SameLine(ImGui::GetContentRegionAvail().x-75);
 if(a.path.empty()){if(ImGui::SmallButton("ВЫБРАТЬ EXE")){auto path=PickExe();if(!path.empty())a.path=path;}}
 else if(ImGui::SmallButton("ЗАПУСТИТЬ"))Launch(a);
 ImGui::EndChild(); ImGui::PopID();
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
 ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
 ImGui::Begin("THE WALL##root",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBackground);
 const float width=ImGui::GetWindowWidth();
 ImGui::BeginChild("topbar",ImVec2(0,58),false,ImGuiWindowFlags_NoScrollbar);
 ImGui::SetNextItemWidth(280); ImGui::InputTextWithHint("##search","⌕  Поиск проектов...",g_search,sizeof(g_search));
 ImGui::SameLine(width-350);
 ImGui::TextColored(ImVec4(.66f,.73f,.88f,1),"WORKSPACE");
 ImGui::SameLine(); ImGui::TextColored(ImVec4(.72f,.58f,1,1),"●");
 ImGui::SameLine(); ImGui::Text("THE WALL");
 ImGui::SameLine(); if(ImGui::SmallButton("—"))ShowWindow(g_hwnd,SW_MINIMIZE);
 ImGui::SameLine(); if(ImGui::SmallButton("×"))PostMessageW(g_hwnd,WM_CLOSE,0,0);
 ImGui::EndChild();
 ImGui::Dummy(ImVec2(0,5));
 ImGui::BeginChild("hero",ImVec2(0,132),true,ImGuiWindowFlags_NoScrollbar);
 ImVec2 hp=ImGui::GetWindowPos(), hs=ImGui::GetWindowSize(); ImDrawList* hd=ImGui::GetWindowDrawList();
 hd->AddRectFilledMultiColor(hp,ImVec2(hp.x+hs.x,hp.y+hs.y),IM_COL32(25,43,82,190),IM_COL32(39,61,117,170),IM_COL32(22,27,50,190),IM_COL32(17,24,42,190));
 hd->AddCircleFilled(ImVec2(hp.x+hs.x-30,hp.y+10),100,IM_COL32(85,110,220,28),64);
 ImGui::Dummy(ImVec2(0,4));
 ImGui::TextColored(ImVec4(.59f,.72f,1,1),"PERSONAL APP LIBRARY");
 ImGui::Dummy(ImVec2(0,4));
 ImGui::Text("ТВОЁ ПРОСТРАНСТВО");
 ImGui::TextDisabled("Собирай любимые приложения в одном месте и запускай в один клик.");
 ImGui::SetCursorPos(ImVec2(hs.x-185,48));
 ImGui::BeginGroup();
 ImGui::TextColored(ImVec4(.96f,.97f,1,1),"%d  ПРОЕКТОВ",(int)g_apps.size());
 ImGui::TextDisabled("ТВОЯ КОЛЛЕКЦИЯ");
 ImGui::EndGroup();
 ImGui::EndChild();
 ImGui::Dummy(ImVec2(0,12));
 ImGui::Text("МОЯ БИБЛИОТЕКА"); ImGui::SameLine();
 ImGui::TextColored(BLUE,"%02d",(int)g_apps.size());
 ImGui::SameLine(); ImGui::TextDisabled("  /  УПРАВЛЯЙ СВОИМИ ПРИЛОЖЕНИЯМИ");
 ImGui::SameLine(width-160);
 if(ImGui::Button("+  ДОБАВИТЬ",ImVec2(142,36)))g_addPopup=true;
 ImGui::Dummy(ImVec2(0,5));
 const char* filters[]={"Все проекты","Избранное","Добавленные"};
 for(int i=0;i<3;i++){if(i)ImGui::SameLine();if(NavButton(filters[i],g_filter==i,ImVec2(124,35)))g_filter=i;}
 ImGui::Dummy(ImVec2(0,10));
 if(g_page==2){
  ImGui::Text("НАСТРОЙКИ"); ImGui::Dummy(ImVec2(0,8));
  ImGui::BeginChild("settings-card",ImVec2(0,120),true);
  ImGui::Checkbox("Закрывать THE WALL после запуска приложения",&g_autoClose);
  ImGui::TextDisabled("Приложения запускаются локально с твоего компьютера.");
  ImGui::EndChild();
 } else {
  float avail=ImGui::GetContentRegionAvail().x, gap=14;
  int columns=avail>850?4:avail>570?3:avail>360?2:1;
  float cardW=(avail-gap*(columns-1))/columns;
  int shown=0;
  for(int i=0;i<(int)g_apps.size();i++){
   auto&a=g_apps[i];
   if((g_page==1||g_filter==1)&&!a.favorite)continue;
   if(g_filter==2&&a.path.empty())continue;
   if(g_search[0]&&a.name.find(g_search)==std::string::npos)continue;
   if(shown%columns)ImGui::SameLine(0,gap);
   AddCard(i,cardW); shown++;
  }
  if(shown==0){
   ImGui::BeginChild("empty-state",ImVec2(0,190),true);
   ImGui::Dummy(ImVec2(0,27));
   ImGui::SetCursorPosX(24); ImGui::TextColored(ImVec4(.68f,.76f,1,1),"✦");
   ImGui::SetCursorPosX(24); ImGui::Text("ТВОЯ БИБЛИОТЕКА ПОКА ПУСТА");
   ImGui::SetCursorPosX(24); ImGui::TextDisabled("Добавь первый проект — он появится здесь отдельной карточкой.");
   ImGui::SetCursorPosX(24); if(ImGui::Button("+  ДОБАВИТЬ ПЕРВЫЙ ПРОЕКТ",ImVec2(225,38)))g_addPopup=true;
   ImGui::EndChild();
  }
 }
 ImGui::SetCursorPosY(ImGui::GetWindowHeight()-66);
 ImGui::BeginChild("bottomnav",ImVec2(0,54),true,ImGuiWindowFlags_NoScrollbar);
 float navW=150, start=(ImGui::GetWindowWidth()-navW*3-24)/2;
 ImGui::SetCursorPosX(start);
 if(NavButton("⌂   ПРОЕКТЫ",g_page==0,ImVec2(navW,38)))g_page=0;
 ImGui::SameLine(0,12);
 if(NavButton("+   ДОБАВИТЬ",false,ImVec2(navW,38))){g_page=0;g_addPopup=true;}
 ImGui::SameLine(0,12);
 if(NavButton("⚙   НАСТРОЙКИ",g_page==2,ImVec2(navW,38)))g_page=2;
 ImGui::EndChild();
 DrawAddPopup();
 ImGui::End();
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