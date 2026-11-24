#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
constexpr wchar_t CLASS_NAME[]=L"ConflictModernWindow";
constexpr float SIDEBAR=222.0f;
constexpr int REGION_COUNT=6;
enum class Page{Map,Army,Report,Settings};
enum class Action{None,Map,Army,Report,Settings,Recruit,NextTurn,SelectRegion};
struct Hit{D2D1_RECT_F rect{};Action action{Action::None};int value{};};
struct Region{const wchar_t*name;const wchar_t*faction;int troops;int owner;};
const std::array<Region,REGION_COUNT> regions{{
 {L"NORTHREACH",L"AZURE UNION",245,0},{L"WESTHOLD",L"EMBER STATE",180,1},
 {L"CENTRALIS",L"AZURE UNION",320,0},{L"EASTVALE",L"EMBER STATE",155,1},
 {L"SOUTHBAY",L"VERDANT PACT",210,2},{L"ISLES",L"VERDANT PACT",95,2}
}};
const std::array<D2D1_COLOR_F,3> faction{{
 D2D1::ColorF(.22f,.66f,.93f,1),D2D1::ColorF(.93f,.39f,.31f,1),D2D1::ColorF(.32f,.76f,.56f,1)
}};
const D2D1_COLOR_F BG=D2D1::ColorF(.047f,.059f,.075f,1);
const D2D1_COLOR_F PANEL=D2D1::ColorF(.075f,.094f,.118f,1);
const D2D1_COLOR_F CARD=D2D1::ColorF(.090f,.114f,.141f,1);
const D2D1_COLOR_F BORDER=D2D1::ColorF(.17f,.21f,.25f,1);
const D2D1_COLOR_F WHITE=D2D1::ColorF(.91f,.94f,.97f,1);
const D2D1_COLOR_F MUTED=D2D1::ColorF(.48f,.55f,.62f,1);
const D2D1_COLOR_F ACCENT=D2D1::ColorF(.31f,.70f,.98f,1);
const D2D1_COLOR_F MAPBG=D2D1::ColorF(.065f,.10f,.12f,1);
const D2D1_COLOR_F LAND=D2D1::ColorF(.12f,.17f,.19f,1);

ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swapChain;
ComPtr<ID2D1Factory> d2dFactory;ComPtr<ID2D1RenderTarget> target;ComPtr<ID2D1SolidColorBrush> brush;
ComPtr<IDWriteFactory> writeFactory;ComPtr<IDWriteTextFormat> smallFont,bodyFont,boldFont,titleFont,heroFont;
using ResetFn=void(__cdecl*)();using CountFn=int(__cdecl*)();using ValueFn=int(__cdecl*)(int);
using RecruitFn=int(__cdecl*)(int,int);using NextFn=int(__cdecl*)();using TransferFn=int(__cdecl*)(int,int,int);
HMODULE rustDll{};ResetFn rustReset{};CountFn rustCount{};ValueFn rustTroops{},rustOwner{},rustTotal{},rustDay{};
RecruitFn rustRecruit{};NextFn rustNext{};TransferFn rustTransfer{};bool rustReady=false;
Page page=Page::Map;int selected=2,clientW=1440,clientH=900,fallbackDay=1;bool borderless=false;
LONG_PTR oldStyle{};RECT oldRect{};std::vector<Hit> hits;std::wstring toast;int toastFrames=0;

int Troops(int i){return rustReady?std::max(0,rustTroops(i)):((i>=0&&i<REGION_COUNT)?regions[i].troops:0);}
int Owner(int i){return rustReady?std::clamp(rustOwner(i),0,2):((i>=0&&i<REGION_COUNT)?regions[i].owner:0);}
int Total(){if(rustReady)return std::max(0,rustTotal(0));int n=0;for(int i=0;i<REGION_COUNT;++i)n+=Troops(i);return n;}
int Day(){return rustReady?std::max(1,rustDay(0)):fallbackDay;}
void Notify(const std::wstring&s){toast=s;toastFrames=240;}

void LoadRust(){
 wchar_t path[MAX_PATH]{};DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!n||n>=MAX_PATH)return;
 std::wstring dll(path);auto slash=dll.find_last_of(L"\\/");if(slash!=std::wstring::npos)dll.resize(slash+1);dll+=L"conflict_core.dll";
 rustDll=LoadLibraryW(dll.c_str());if(!rustDll)return;
 rustReset=reinterpret_cast<ResetFn>(GetProcAddress(rustDll,"conflict_reset"));
 rustCount=reinterpret_cast<CountFn>(GetProcAddress(rustDll,"conflict_region_count"));
 rustTroops=reinterpret_cast<ValueFn>(GetProcAddress(rustDll,"conflict_region_troops"));
 rustOwner=reinterpret_cast<ValueFn>(GetProcAddress(rustDll,"conflict_region_owner"));
 rustTotal=reinterpret_cast<ValueFn>(GetProcAddress(rustDll,"conflict_total_troops"));
 rustDay=reinterpret_cast<ValueFn>(GetProcAddress(rustDll,"conflict_current_day"));
 rustRecruit=reinterpret_cast<RecruitFn>(GetProcAddress(rustDll,"conflict_recruit"));
 rustNext=reinterpret_cast<NextFn>(GetProcAddress(rustDll,"conflict_next_turn"));
 rustTransfer=reinterpret_cast<TransferFn>(GetProcAddress(rustDll,"conflict_transfer"));
 rustReady=rustReset&&rustCount&&rustTroops&&rustOwner&&rustTotal&&rustDay&&rustRecruit&&rustNext&&rustTransfer&&rustCount()==REGION_COUNT;
 if(rustReady)rustReset();else{FreeLibrary(rustDll);rustDll=nullptr;}
}
HRESULT MakeFormat(float size,DWRITE_FONT_WEIGHT weight,DWRITE_TEXT_ALIGNMENT align,IDWriteTextFormat**out){
 HRESULT h=writeFactory->CreateTextFormat(L"Segoe UI",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"es-ES",out);
 if(SUCCEEDED(h)){(*out)->SetTextAlignment(align);(*out)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);(*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);}return h;
}
HRESULT MakeTarget(){
 ComPtr<IDXGISurface> surface;HRESULT h=swapChain->GetBuffer(0,IID_PPV_ARGS(&surface));if(FAILED(h))return h;
 auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);
 h=d2dFactory->CreateDxgiSurfaceRenderTarget(surface.Get(),props,&target);if(FAILED(h))return h;
 return target->CreateSolidColorBrush(WHITE,&brush);
}
HRESULT InitGraphics(HWND hwnd){
 DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
 sd.BufferCount=2;sd.OutputWindow=hwnd;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};D3D_FEATURE_LEVEL obtained{};
 HRESULT h=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,&sd,&swapChain,&device,&obtained,&context);
 if(FAILED(h))return h;
 h=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2dFactory.GetAddressOf());if(FAILED(h))return h;
 h=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf()));if(FAILED(h))return h;
 if(FAILED(MakeFormat(11,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,&smallFont)))return E_FAIL;
 if(FAILED(MakeFormat(14,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,&bodyFont)))return E_FAIL;
 if(FAILED(MakeFormat(14,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,&boldFont)))return E_FAIL;
 if(FAILED(MakeFormat(22,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,&titleFont)))return E_FAIL;
 if(FAILED(MakeFormat(30,DWRITE_FONT_WEIGHT_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,&heroFont)))return E_FAIL;
 return MakeTarget();
}
void Resize(UINT w,UINT h){if(!swapChain||!w||!h)return;target.Reset();brush.Reset();context->OMSetRenderTargets(0,nullptr,nullptr);if(SUCCEEDED(swapChain->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0)))MakeTarget();}
void Fill(const D2D1_RECT_F&r,D2D1_COLOR_F c){brush->SetColor(c);target->FillRectangle(r,brush.Get());}
void Round(const D2D1_RECT_F&r,D2D1_COLOR_F c,float rad=10){brush->SetColor(c);target->FillRoundedRectangle(D2D1::RoundedRect(r,rad,rad),brush.Get());}
void Outline(const D2D1_RECT_F&r,D2D1_COLOR_F c,float rad=10,float w=1){brush->SetColor(c);target->DrawRoundedRectangle(D2D1::RoundedRect(r,rad,rad),brush.Get(),w);}
void Stroke(float x1,float y1,float x2,float y2,D2D1_COLOR_F c,float w=1){brush->SetColor(c);target->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush.Get(),w);}
void Dot(float x,float y,float rad,D2D1_COLOR_F c){brush->SetColor(c);target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,y),rad,rad),brush.Get());}
void Label(float x,float y,float w,float h,const std::wstring&s,IDWriteTextFormat*f,D2D1_COLOR_F c=WHITE){if(s.empty()||!f)return;brush->SetColor(c);target->DrawText(s.c_str(),static_cast<UINT32>(s.size()),f,D2D1::RectF(x,y,x+w,y+h),brush.Get());}
void Small(float x,float y,float w,const std::wstring&s,D2D1_COLOR_F c=MUTED){Label(x,y,w,22,s,smallFont.Get(),c);}
void Body(float x,float y,float w,const std::wstring&s,D2D1_COLOR_F c=WHITE){Label(x,y,w,28,s,bodyFont.Get(),c);}
void Bold(float x,float y,float w,const std::wstring&s,D2D1_COLOR_F c=WHITE){Label(x,y,w,28,s,boldFont.Get(),c);}
void Polygon(const std::vector<D2D1_POINT_2F>&p,D2D1_COLOR_F fill,D2D1_COLOR_F edge,float width){
 if(p.size()<3)return;ComPtr<ID2D1PathGeometry>g;ComPtr<ID2D1GeometrySink>s;if(FAILED(d2dFactory->CreatePathGeometry(&g))||FAILED(g->Open(&s)))return;
 s->BeginFigure(p[0],D2D1_FIGURE_BEGIN_FILLED);s->AddLines(p.data()+1,static_cast<UINT32>(p.size()-1));s->EndFigure(D2D1_FIGURE_END_CLOSED);if(FAILED(s->Close()))return;
 brush->SetColor(fill);target->FillGeometry(g.Get(),brush.Get());brush->SetColor(edge);target->DrawGeometry(g.Get(),brush.Get(),width);
}
void Button(const D2D1_RECT_F&r,const std::wstring&s,Action a,int v=0,bool primary=false){
 hits.push_back({r,a,v});Round(r,primary?ACCENT:CARD,9);if(!primary)Outline(r,BORDER,9);
 Label(r.left+8,r.top+1,r.right-r.left-16,r.bottom-r.top-2,s,boldFont.Get(),primary?BG:WHITE);
}
void Stat(float x,float y,float w,const std::wstring&head,const std::wstring&value,D2D1_COLOR_F color){
 auto r=D2D1::RectF(x,y,x+w,y+88);Round(r,CARD,12);Outline(r,BORDER,12);Round(D2D1::RectF(x+16,y+17,x+20,y+70),color,2);
 Small(x+32,y+12,w-45,head);Label(x+32,y+36,w-45,40,value,titleFont.Get());
}
D2D1_POINT_2F Center(int i,const D2D1_RECT_F&m){
 const float p[6][2]={{.13f,.27f},{.28f,.69f},{.49f,.45f},{.70f,.25f},{.73f,.72f},{.91f,.49f}};
 return D2D1::Point2F(m.left+(m.right-m.left)*p[i][0],m.top+(m.bottom-m.top)*p[i][1]);
}
void TroopDots(float x,float y,int n,D2D1_COLOR_F c){int visible=std::clamp(5+n/65,6,20);for(int i=0;i<visible;++i){int row=i/5,col=i%5;Dot(x+(col-2)*12.0f+(row%2)*4.0f,y+(row-1.5f)*12.0f,4.1f,c);}}

void Sidebar(){
 Fill(D2D1::RectF(0,0,SIDEBAR,static_cast<float>(clientH)),PANEL);Stroke(SIDEBAR,0,SIDEBAR,static_cast<float>(clientH),BORDER);
 Round(D2D1::RectF(22,24,58,60),ACCENT,9);Label(22,24,36,36,L"C",titleFont.Get(),BG);Bold(70,24,130,L"CONFLICT");Small(70,48,130,L"STRATEGY SYSTEM");
 Small(24,111,170,L"WORKSPACE");
 const struct{const wchar_t*n;Page p;Action a;}nav[]={{L"Mapa global",Page::Map,Action::Map},{L"Ejército",Page::Army,Action::Army},{L"Informe",Page::Report,Action::Report},{L"Ajustes",Page::Settings,Action::Settings}};
 for(int i=0;i<4;++i){float y=142+i*54.0f;auto r=D2D1::RectF(12,y,SIDEBAR-12,y+44);if(page==nav[i].p){Round(r,D2D1::ColorF(.12f,.22f,.30f,1),8);Round(D2D1::RectF(12,y+8,15,y+36),ACCENT,2);}Label(31,y+2,165,40,nav[i].n,boldFont.Get(),page==nav[i].p?WHITE:MUTED);hits.push_back({r,nav[i].a,0});}
 Round(D2D1::RectF(16,clientH-126.0f,SIDEBAR-16,clientH-22.0f),CARD,10);Small(29,clientH-111.0f,150,L"SIMULATION CORE");
 Dot(31,clientH-72.0f,4,rustReady?D2D1::ColorF(.28f,.82f,.52f,1):D2D1::ColorF(.95f,.64f,.24f,1));
 Bold(44,clientH-86.0f,140,rustReady?L"RUST ONLINE":L"SAFE MODE");Small(29,clientH-54.0f,150,L"BUILD 0.4.0  /  2026");
}
void Header(){
 Stroke(SIDEBAR,87,static_cast<float>(clientW),87,BORDER);
 const wchar_t*h=L"MAPA GLOBAL";const wchar_t*s=L"Vista general del territorio y despliegue de unidades";
 if(page==Page::Army){h=L"EJÉRCITO";s=L"Conteo y distribución de unidades por región";}
 if(page==Page::Report){h=L"INFORME";s=L"Resumen del estado de la simulación";}
 if(page==Page::Settings){h=L"AJUSTES";s=L"Renderizado, ventana y compatibilidad";}
 Label(250,12,600,38,h,titleFont.Get());Small(250,50,660,s);Round(D2D1::RectF(clientW-230.0f,25,clientW-32.0f,61),CARD,8);
 Label(clientW-222.0f,26,182,34,L"2026  /  TURNO "+std::to_wstring(Day()),boldFont.Get(),ACCENT);
}
void MapPage(){
 float x=250,y=112,w=std::max(170.0f,(clientW-310.0f)/3.0f-14.0f);
 Stat(x,y,w,L"TOTAL DE TROPAS",std::to_wstring(Total()),ACCENT);Stat(x+w+14,y,w,L"TERRITORIOS",L"06",faction[2]);Stat(x+2*(w+14),y,w,L"TURNO ACTUAL",std::to_wstring(Day()),D2D1::ColorF(.92f,.67f,.32f,1));
 auto map=D2D1::RectF(250,222,clientW-328.0f,clientH-30.0f);Round(map,MAPBG,14);Outline(map,BORDER,14);Small(map.left+18,map.top+12,220,L"MAPA DE REGIONES");Small(map.right-205,map.top+12,190,L"6 ZONAS ACTIVAS");
 const int links[7][2]={{0,2},{1,2},{2,3},{2,4},{3,5},{4,5},{0,1}};std::array<D2D1_POINT_2F,REGION_COUNT> centers{};
 for(int i=0;i<REGION_COUNT;++i)centers[i]=Center(i,map);
 for(auto&l:links)Stroke(centers[l[0]].x,centers[l[0]].y,centers[l[1]].x,centers[l[1]].y,D2D1::ColorF(.24f,.34f,.38f,.7f),2);
 for(int i=0;i<REGION_COUNT;++i){
  auto p=centers[i];auto col=faction[Owner(i)];float rx=76,ry=51;
  std::vector<D2D1_POINT_2F>shape={D2D1::Point2F(p.x-rx*.90f,p.y-ry*.42f),D2D1::Point2F(p.x-rx*.48f,p.y-ry*.98f),D2D1::Point2F(p.x+rx*.25f,p.y-ry*.92f),D2D1::Point2F(p.x+rx*.91f,p.y-ry*.45f),D2D1::Point2F(p.x+rx*.76f,p.y+ry*.48f),D2D1::Point2F(p.x+rx*.17f,p.y+ry*.95f),D2D1::Point2F(p.x-rx*.70f,p.y+ry*.70f),D2D1::Point2F(p.x-rx*1.02f,p.y+ry*.05f)};
  bool chosen=i==selected;auto land=chosen?D2D1::ColorF(col.r*.42f+.06f,col.g*.42f+.06f,col.b*.42f+.06f,1):LAND;
  Polygon(shape,land,chosen?col:BORDER,chosen?3.0f:1.2f);TroopDots(p.x,p.y-6,Troops(i),col);
  auto count=D2D1::RectF(p.x-43,p.y+26,p.x+43,p.y+54);Round(count,PANEL,8);Outline(count,chosen?col:BORDER,8,chosen?1.5f:1);
  Label(count.left,count.top,86,28,std::to_wstring(Troops(i)),boldFont.Get());Label(p.x-80,p.y+57,160,22,regions[i].name,smallFont.Get(),chosen?WHITE:MUTED);
  hits.push_back({D2D1::RectF(p.x-rx,p.y-ry,p.x+rx,p.y+ry+37),Action::SelectRegion,i});
 }
 float px=clientW-310.0f;auto panel=D2D1::RectF(px,222,clientW-24.0f,clientH-30.0f);Round(panel,PANEL,14);Outline(panel,BORDER,14);
 Small(px+20,244,230,L"TERRITORIO SELECCIONADO");Label(px+20,276,250,34,regions[selected].name,titleFont.Get());Dot(px+28,342,5,faction[Owner(selected)]);Small(px+43,331,220,regions[selected].faction);
 Small(px+20,385,230,L"UNIDADES DESPLEGADAS");Label(px+20,408,240,52,std::to_wstring(Troops(selected)),heroFont.Get());
 Small(px+20,467,235,L"Cada punto representa un grupo");Small(px+20,487,235,L"de unidades en el mapa.");
 Button(D2D1::RectF(px+18,532,px+260,578),L"RECLUTAR  +50",Action::Recruit,0,true);Stroke(px+18,609,px+260,609,BORDER);
 Small(px+20,628,230,L"GESTIÓN DE TURNO");Body(px+20,653,230,L"Refuerzos automáticos");
 Button(D2D1::RectF(px+18,705,px+260,751),L"SIGUIENTE TURNO",Action::NextTurn);
 Small(px+20,clientH-56,235,rustReady?L"Simulación ejecutada en Rust":L"Respaldo C++");
}
void ArmyPage(){
 float x=250,w=std::min(520.0f,(clientW-300.0f)/2);Label(250,112,520,34,L"FUERZAS POR TERRITORIO",titleFont.Get());Small(250,151,620,L"Unidades representadas visualmente como grupos circulares.");
 for(int i=0;i<REGION_COUNT;++i){int col=i%2,row=i/2;float left=x+col*(w+18),top=200+row*150;auto r=D2D1::RectF(left,top,left+w,top+126);Round(r,CARD,12);Outline(r,BORDER,12);Dot(left+33,top+38,7,faction[Owner(i)]);Bold(left+54,top+18,w-70,regions[i].name);Small(left+54,top+52,w-70,regions[i].faction);Label(left+20,top+81,w-40,35,std::to_wstring(Troops(i))+L" UNIDADES",boldFont.Get(),ACCENT);}
 Stat(x,clientH-110,w,L"TOTAL EN EL MAPA",std::to_wstring(Total()),ACCENT);
}
void ReportPage(){
 float x=250;Label(x,112,500,34,L"ESTADO DEL MUNDO",titleFont.Get());Small(x,151,600,L"Resumen de la partida actual.");
 Stat(x,205,300,L"TOTAL DE UNIDADES",std::to_wstring(Total()),ACCENT);Stat(x+320,205,300,L"REGIONES",L"06",faction[2]);Stat(x+640,205,300,L"TURNO",std::to_wstring(Day()),D2D1::ColorF(.92f,.67f,.32f,1));
 auto panel=D2D1::RectF(x,325,clientW-40.0f,570);Round(panel,CARD,12);Outline(panel,BORDER,12);Bold(x+24,345,600,L"DISTRIBUCIÓN REGIONAL");
 for(int i=0;i<REGION_COUNT;++i){float top=390+i*27.0f;Small(x+24,top,155,regions[i].name,WHITE);Round(D2D1::RectF(x+190,top+5,x+610,top+17),PANEL,5);float ratio=std::clamp(Troops(i)/500.0f,0.0f,1.0f);Round(D2D1::RectF(x+190,top+5,x+190+420*ratio,top+17),faction[Owner(i)],5);Small(x+635,top,120,std::to_wstring(Troops(i)),WHITE);}
}
void SettingsPage(){
 float x=250;Label(x,112,620,34,L"RENDERIZADO Y COMPATIBILIDAD",titleFont.Get());Small(x,151,700,L"Configuración nativa de Windows.");
 auto panel=D2D1::RectF(x,205,clientW-45.0f,700);Round(panel,CARD,12);Outline(panel,BORDER,12);
 Bold(x+25,230,400,L"GRÁFICOS");Small(x+25,276,220,L"API DE RENDERIZADO");Bold(x+280,276,400,L"DIRECT3D 11 + DXGI",ACCENT);Stroke(x+25,318,clientW-75.0f,318,BORDER);
 Small(x+25,340,220,L"MODO DE VENTANA");Bold(x+280,340,500,borderless?L"SIN BORDES":L"VENTANA REDIMENSIONABLE");Small(x+25,394,220,L"CAMBIAR MODO");
 Button(D2D1::RectF(x+280,385,x+510,428),borderless?L"VOLVER A VENTANA":L"ACTIVAR SIN BORDES",Action::None);
 Stroke(x+25,456,clientW-75.0f,456,BORDER);Bold(x+25,479,400,L"NVIDIA APP OVERLAY");
 Body(x+25,522,880,L"El programa no bloquea superposiciones externas ni captura Alt+Z.");
 Small(x+25,556,900,L"Compatibilidad prevista con Direct3D 11. Depende del controlador, NVIDIA App");
 Small(x+25,578,900,L"y de las funciones admitidas por cada aplicación.");
 Stroke(x+25,622,clientW-75.0f,622,BORDER);Bold(x+25,644,300,L"SIMULACIÓN");Small(x+280,644,500,rustReady?L"Rust cargado correctamente":L"Motor C++ de respaldo",rustReady?faction[2]:MUTED);
}
void Draw(){
 if(!target||!swapChain)return;hits.clear();target->BeginDraw();target->SetTransform(D2D1::Matrix3x2F::Identity());target->Clear(BG);
 Sidebar();Header();if(page==Page::Map)MapPage();else if(page==Page::Army)ArmyPage();else if(page==Page::Report)ReportPage();else SettingsPage();
 if(toastFrames>0&&!toast.empty()){float w=std::min(430.0f,clientW-260.0f),x=clientW-w-28.0f;auto r=D2D1::RectF(x,clientH-73.0f,x+w,clientH-28.0f);Round(r,CARD,9);Outline(r,BORDER,9);Label(x+15,r.top+2,w-30,40,toast,bodyFont.Get());--toastFrames;}
 HRESULT h=target->EndDraw();if(h==D2DERR_RECREATE_TARGET){target.Reset();brush.Reset();MakeTarget();return;}swapChain->Present(1,0);
}
void ToggleBorderless(HWND hwnd){
 if(!borderless){oldStyle=GetWindowLongPtrW(hwnd,GWL_STYLE);GetWindowRect(hwnd,&oldRect);MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&m);SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);SetWindowPos(hwnd,HWND_TOP,m.rcMonitor.left,m.rcMonitor.top,m.rcMonitor.right-m.rcMonitor.left,m.rcMonitor.bottom-m.rcMonitor.top,SWP_FRAMECHANGED|SWP_NOOWNERZORDER);borderless=true;}
 else{SetWindowLongPtrW(hwnd,GWL_STYLE,oldStyle);SetWindowPos(hwnd,nullptr,oldRect.left,oldRect.top,oldRect.right-oldRect.left,oldRect.bottom-oldRect.top,SWP_FRAMECHANGED|SWP_NOZORDER|SWP_NOOWNERZORDER);borderless=false;}
 InvalidateRect(hwnd,nullptr,FALSE);
}
void Act(HWND hwnd,const Hit&h){
 switch(h.action){
 case Action::Map:page=Page::Map;break;case Action::Army:page=Page::Army;break;case Action::Report:page=Page::Report;break;case Action::Settings:page=Page::Settings;break;
 case Action::SelectRegion:selected=std::clamp(h.value,0,REGION_COUNT-1);break;
 case Action::Recruit:if(rustReady&&rustRecruit(selected,50)==1)Notify(L"Se añadieron 50 unidades al territorio.");else Notify(L"No se pudo reclutar en este territorio.");break;
 case Action::NextTurn:if(rustReady)Notify(L"Turno "+std::to_wstring(rustNext())+L": refuerzos actualizados.");break;
 case Action::None:if(page==Page::Settings&&h.rect.left>500)ToggleBorderless(hwnd);break;
 }
 InvalidateRect(hwnd,nullptr,FALSE);
}
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_SIZE:clientW=LOWORD(lp);clientH=HIWORD(lp);Resize(static_cast<UINT>(clientW),static_cast<UINT>(clientH));InvalidateRect(hwnd,nullptr,FALSE);return 0;
 case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(hwnd,&ps);Draw();EndPaint(hwnd,&ps);return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_LBUTTONUP:{float x=static_cast<float>(GET_X_LPARAM(lp)),y=static_cast<float>(GET_Y_LPARAM(lp));for(auto it=hits.rbegin();it!=hits.rend();++it)if(x>=it->rect.left&&x<=it->rect.right&&y>=it->rect.top&&y<=it->rect.bottom){Act(hwnd,*it);break;}return 0;}
 case WM_KEYDOWN:if(wp==VK_F11)ToggleBorderless(hwnd);else if(wp==VK_ESCAPE&&borderless)ToggleBorderless(hwnd);return 0;
 case WM_GETMINMAXINFO:{auto*m=reinterpret_cast<MINMAXINFO*>(lp);m->ptMinTrackSize.x=1040;m->ptMinTrackSize.y=700;return 0;}
 case WM_DESTROY:if(rustDll){FreeLibrary(rustDll);rustDll=nullptr;}PostQuitMessage(0);return 0;
 default:return DefWindowProcW(hwnd,msg,wp,lp);
 }
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetProcessDPIAware();WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=CLASS_NAME;
 if(!RegisterClassExW(&wc))return 1;RECT r{0,0,1440,900};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 HWND hwnd=CreateWindowExW(0,CLASS_NAME,L"CONFLICT — Strategy System",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
 if(!hwnd)return 1;if(FAILED(InitGraphics(hwnd))){MessageBoxW(hwnd,L"No se pudo iniciar Direct3D 11. Actualiza los controladores gráficos.",L"CONFLICT — Error gráfico",MB_OK|MB_ICONERROR);DestroyWindow(hwnd);return 1;}
 LoadRust();ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);
}