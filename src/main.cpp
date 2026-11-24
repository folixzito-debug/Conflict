#include <windows.h>
#include <windowsx.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace {
enum class Page { Menu, Campaign, QuickFight, Arsenal, Eras, Settings };
Page page = Page::Menu;
int selected = 0, hover = -1;
bool keys[256]{};
int clientW=1100, clientH=780;
const int MENU_COUNT=5;
struct MenuItem { const wchar_t* label; const wchar_t* detail; Page target; };
const MenuItem menuItems[MENU_COUNT]={
 {L"CAMPAÑA",L"Avanza por las eras de la historia",Page::Campaign},
 {L"COMBATE RÁPIDO",L"Entra directamente a la arena",Page::QuickFight},
 {L"ARSENAL",L"Descubre armas de cada época",Page::Arsenal},
 {L"ÉPOCAS",L"Del año 0 hasta la era actual",Page::Eras},
 {L"AJUSTES",L"Configura tu experiencia",Page::Settings}
};
COLORREF bg=RGB(13,15,18), panel=RGB(23,26,30), gold=RGB(190,151,83);
COLORREF pale=RGB(224,218,202), muted=RGB(139,140,137);
struct Fighter { float x,y; int hp; int facing; bool blocking; float attack; float cooldown; float hurt; };
Fighter player{}, enemy{};
bool gameOver=false, playerWon=false;
int roundNumber=1;

using RustDamageFn = int (__cdecl*)(int, int);
using RustCanHitFn = int (__cdecl*)(float, float);
HMODULE rustModule=nullptr;
RustDamageFn rustDamage=nullptr;
RustCanHitFn rustCanHit=nullptr;
bool RustReady(){ return rustDamage!=nullptr && rustCanHit!=nullptr; }
void LoadRustCore(){
 wchar_t exePath[MAX_PATH]{};
 DWORD n=GetModuleFileNameW(nullptr,exePath,MAX_PATH);
 if(!n || n>=MAX_PATH) return;
 std::wstring dllPath(exePath);
 size_t slash=dllPath.find_last_of(L"\\/");
 if(slash!=std::wstring::npos) dllPath.resize(slash+1);
 dllPath+=L"conflict_core.dll";
 rustModule=LoadLibraryW(dllPath.c_str());
 if(!rustModule) return;
 rustDamage=reinterpret_cast<RustDamageFn>(GetProcAddress(rustModule,"conflict_damage"));
 rustCanHit=reinterpret_cast<RustCanHitFn>(GetProcAddress(rustModule,"conflict_can_hit"));
 if(!RustReady()){
   FreeLibrary(rustModule); rustModule=nullptr; rustDamage=nullptr; rustCanHit=nullptr;
 }
}
void ResetFight(bool nextRound=false){
 if(nextRound) ++roundNumber; else roundNumber=1;
 player={260.0f,0.0f,100,1,false,0.0f,0.0f,0.0f};
 enemy={760.0f,0.0f,100,-1,false,0.0f,0.4f,0.0f};
 gameOver=false; playerWon=false;
}
void StartFight(HWND hwnd){ page=Page::QuickFight; ResetFight(false); SetTimer(hwnd,1,16,nullptr); }
void Fill(HDC d,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(d,&r,b);DeleteObject(b);}
void Text(HDC d,const wchar_t*s,int x,int y,int size,COLORREF c,bool bold=false,UINT align=DT_LEFT){
 HFONT f=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
 HFONT old=(HFONT)SelectObject(d,f);SetBkMode(d,TRANSPARENT);SetTextColor(d,c);
 RECT r{x,y,clientW-25,y+size+18};DrawTextW(d,s,-1,&r,align|DT_SINGLELINE|DT_VCENTER);SelectObject(d,old);DeleteObject(f);
}
void Line(HDC d,int x1,int y1,int x2,int y2,COLORREF c,int width=1){HPEN p=CreatePen(PS_SOLID,width,c),o=(HPEN)SelectObject(d,p);MoveToEx(d,x1,y1,nullptr);LineTo(d,x2,y2);SelectObject(d,o);DeleteObject(p);}
void Frame(HDC d,RECT r,COLORREF c){HPEN p=CreatePen(PS_SOLID,1,c); HGDIOBJ op=SelectObject(d,p),ob=SelectObject(d,GetStockObject(HOLLOW_BRUSH));Rectangle(d,r.left,r.top,r.right,r.bottom);SelectObject(d,op);SelectObject(d,ob);DeleteObject(p);}
RECT MenuRect(int i){return RECT{70,270+i*78,550,330+i*78};}
void DrawMenu(HDC d){
 Text(d,L"ESTRATEGIA  /  COMBATE  /  HISTORIA",70,38,12,muted,true);Line(d,70,66,clientW-70,66,RGB(55,49,40));
 Text(d,L"CONFLICT",70,105,62,pale,true);Text(d,L"AGES OF WAR",74,174,17,gold,true);Text(d,L"UNA HISTORIA ESCRITA EN ACERO",72,211,13,muted);
 for(int i=0;i<MENU_COUNT;i++){RECT r=MenuRect(i);bool a=i==selected||i==hover;Fill(d,r,a?RGB(48,39,30):panel);Frame(d,r,a?gold:RGB(49,51,53));if(a)Fill(d,{r.left,r.top,r.left+4,r.bottom},gold);Text(d,menuItems[i].label,r.left+22,r.top+7,19,a?pale:RGB(185,185,179),true);Text(d,menuItems[i].detail,r.left+22,r.top+32,11,muted);Text(d,i==selected?L">":L"",r.right-36,r.top+15,18,gold,true);}
 Text(d,L"↑  ↓  NAVEGAR       ENTER  CONFIRMAR       ESC  VOLVER",70,clientH-64,12,muted);
}
void DrawBar(HDC d,int x,int y,int w,int hp,const wchar_t*name,bool flip){
 Text(d,name,x,y-27,15,pale,true);Fill(d,{x,y,x+w,y+18},RGB(48,43,40));int fw=w*std::max(0,hp)/100;
 Fill(d,flip?RECT{x+w-fw,y,x+w,y+18}:RECT{x,y,x+fw,y+18},hp>30?RGB(91,139,83):RGB(174,76,61));Frame(d,{x,y,x+w,y+18},gold);
}
void EllipseShape(HDC d,RECT r,COLORREF fill,COLORREF outline){
 HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,outline);
 HGDIOBJ ob=SelectObject(d,b),op=SelectObject(d,p);Ellipse(d,r.left,r.top,r.right,r.bottom);
 SelectObject(d,ob);SelectObject(d,op);DeleteObject(b);DeleteObject(p);
}
void PolyShape(HDC d,const POINT*pts,int count,COLORREF fill,COLORREF outline){
 HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,outline);
 HGDIOBJ ob=SelectObject(d,b),op=SelectObject(d,p);Polygon(d,pts,count);
 SelectObject(d,ob);SelectObject(d,op);DeleteObject(b);DeleteObject(p);
}
void DrawFighter(HDC d,const Fighter&f,bool isPlayer,int ground){
 int x=(int)f.x, y=ground-7;
 COLORREF armor=isPlayer?RGB(46,78,107):RGB(112,49,43);
 COLORREF bright=isPlayer?RGB(105,144,174):RGB(174,103,79);
 COLORREF dark=isPlayer?RGB(25,45,63):RGB(66,31,29);
 COLORREF cloth=isPlayer?RGB(42,48,56):RGB(51,37,33);
 // Ground shadow and boots.
 EllipseShape(d,{x-38,ground-10,x+39,ground+7},RGB(27,24,21),RGB(27,24,21));
 Line(d,x-12,y-50,x-23,y-9,cloth,13);Line(d,x+12,y-50,x+22,y-9,cloth,13);
 Line(d,x-23,y-9,x-6,y-7,RGB(28,26,23),9);Line(d,x+22,y-9,x+38,y-7,RGB(28,26,23),9);
 // Greaves and knee plates.
 Fill(d,{x-19,y-49,x-6,y-16},RGB(91,88,78));Fill(d,{x+6,y-49,x+19,y-16},RGB(91,88,78));
 Line(d,x-18,y-34,x-6,y-34,gold,2);Line(d,x+6,y-34,x+18,y-34,gold,2);
 // Body silhouette and tunic.
 POINT torso[6]={{x-23,y-112},{x+23,y-112},{x+29,y-61},{x+18,y-47},{x-18,y-47},{x-29,y-61}};
 PolyShape(d,torso,6,armor,dark);Line(d,x-23,y-106,x+23,y-106,bright,2);
 Line(d,x,y-105,x,y-53,dark,2);
 Fill(d,{x-20,y-57,x+20,y-48},RGB(58,45,34));Line(d,x-19,y-55,x+19,y-55,gold,3);
 EllipseShape(d,{x-5,y-59,x+5,y-49},gold,RGB(94,67,39));
 // Shoulder plates, arms and gauntlets.
 EllipseShape(d,{x-34,y-111,x-12,y-88},bright,dark);
 EllipseShape(d,{x+12,y-111,x+34,y-88},bright,dark);
 Line(d,x+f.facing*20,y-98,x+f.facing*37,y-77,bright,11);
 Line(d,x+f.facing*37,y-77,x+f.facing*43,y-71,RGB(155,148,130),9);
 // Helmet, neck guard and visor.
 Fill(d,{x-9,y-120,x+9,y-111},RGB(126,111,85));
 EllipseShape(d,{x-21,y-151,x+21,y-113},RGB(117,119,116),RGB(45,49,50));
 Fill(d,{x-21,y-135,x+21,y-126},RGB(83,88,88));Line(d,x-19,y-132,x+19,y-132,gold,2);
 Fill(d,{x-13,y-145,x+13,y-140},RGB(28,32,34));
 Line(d,x-17,y-119,x+17,y-119,RGB(173,164,141),2);
 // Sword with a short slash arc while attacking.
 int sx=x+f.facing*41,sy=y-74;
 int reach=f.attack>0?83:56;
 int tipX=sx+f.facing*reach,tipY=sy-(f.attack>0?42:25);
 Line(d,sx,sy,tipX,tipY,RGB(42,43,43),7);
 Line(d,sx,sy,tipX,tipY,RGB(190,198,199),4);
 Line(d,sx,sy-2,tipX,tipY-2,RGB(235,231,215),1);
 Line(d,sx-f.facing*10,sy+8,sx+f.facing*10,sy-8,gold,5);
 if(f.blocking){
   POINT shield[5]={{x+f.facing*28,y-131},{x+f.facing*59,y-119},{x+f.facing*62,y-74},{x+f.facing*43,y-58},{x+f.facing*27,y-82}};
   PolyShape(d,shield,5,RGB(123,91,57),gold);
   Line(d,x+f.facing*36,y-117,x+f.facing*53,y-77,RGB(205,178,126),2);
 }
 if(f.hurt>0){Frame(d,{x-39,y-155,x+39,y+4},RGB(242,204,140));}
}
void DrawBanner(HDC d,int x,int top,COLORREF cloth){
 Line(d,x,top,x,top+145,RGB(119,105,80),5);
 POINT flag[5]={{x+3,top+8},{x+68,top+17},{x+65,top+104},{x+35,top+124},{x+3,top+105}};
 PolyShape(d,flag,5,cloth,RGB(191,156,103));
 Line(d,x+16,top+26,x+53,top+92,RGB(221,194,143),3);
 Line(d,x+53,top+26,x+16,top+92,RGB(221,194,143),3);
}
void DrawTorch(HDC d,int x,int y){
 EllipseShape(d,{x-24,y-29,x+24,y+20},RGB(89,58,35),RGB(29,28,25));
 Fill(d,{x-5,y-53,x+5,y-5},RGB(105,69,39));
 EllipseShape(d,{x-13,y-63,x+13,y-32},RGB(221,115,42),RGB(221,115,42));
 EllipseShape(d,{x-7,y-56,x+7,y-36},RGB(246,190,83),RGB(246,190,83));
}
void DrawArena(HDC d){
 Fill(d,{0,0,clientW,clientH},RGB(17,20,24));
 // Dusky sky behind the fortress.
 Fill(d,{0,0,clientW,170},RGB(31,38,46));
 Fill(d,{0,170,clientW,315},RGB(43,44,44));
 Fill(d,{0,315,clientW,405},RGB(54,48,40));
 EllipseShape(d,{clientW/2-165,52,clientW/2+165,285},RGB(37,40,43),RGB(37,40,43));
 // Back wall and battlements.
 Fill(d,{0,235,clientW,411},RGB(68,64,56));
 for(int x=0;x<clientW;x+=96){
   Fill(d,{x+5,214,x+76,241},RGB(83,77,66));
   Fill(d,{x+16,198,x+32,218},RGB(83,77,66));
   Fill(d,{x+49,198,x+65,218},RGB(83,77,66));
 }
 for(int row=0;row<4;row++){
   int yy=252+row*37;
   Line(d,0,yy,clientW,yy,RGB(48,48,45),2);
   for(int x=(row%2)*48;x<clientW;x+=96)Line(d,x,yy-35,x,yy,RGB(49,48,45),2);
 }
 // Central gate, framed by stone blocks.
 Fill(d,{clientW/2-118,254,clientW/2+118,411},RGB(34,34,33));
 EllipseShape(d,{clientW/2-118,205,clientW/2+118,355},RGB(34,34,33),RGB(103,91,72));
 Fill(d,{clientW/2-105,295,clientW/2+105,411},RGB(27,29,30));
 for(int i=0;i<8;i++)Line(d,clientW/2-92+i*26,300,clientW/2-92+i*26,411,RGB(62,59,52),3);
 // Side towers and stone columns.
 for(int side=0;side<2;side++){
   int x=side?clientW-112:28;
   Fill(d,{x,165,x+84,411},RGB(91,82,68));Frame(d,{x,165,x+84,411},RGB(39,39,37));
   for(int yy=185;yy<400;yy+=43)Line(d,x,yy,x+84,yy,RGB(54,52,47),2);
   Fill(d,{x-10,155,x+94,177},RGB(111,97,76));
   Fill(d,{x+19,207,x+64,275},RGB(34,34,32));
   DrawTorch(d,x+42,331);
 }
 DrawBanner(d,clientW/2-310,184,RGB(105,45,39));
 DrawBanner(d,clientW/2+245,184,RGB(42,65,88));
 // Audience silhouettes behind the fighting area.
 for(int row=0;row<2;row++)for(int x=155;x<clientW-145;x+=34){
   int y=349+row*22;
   EllipseShape(d,{x-6,y-17,x+7,y-4},RGB(32+row*4,31+row*3,29),RGB(32,31,29));
   Line(d,x,y-5,x,y+12,RGB(31,30,29),6);
 }
 Line(d,0,408,clientW,408,gold,3);
 // Stone arena floor with perspective seams and individual block joints.
 Fill(d,{0,411,clientW,clientH},RGB(73,64,51));
 for(int row=0;row<7;row++){
   int yy=420+row*std::max(25,(clientH-420)/7);
   Line(d,0,yy,clientW,yy,RGB(51,48,42),2);
   int step=112-row*7;
   for(int x=(row%2)*step;x<clientW;x+=step)Line(d,x,yy-step/2,x,yy+step/2,RGB(54,50,43),2);
 }
 for(int x=0;x<clientW;x+=160)Line(d,x,412,x-105,clientH,RGB(91,78,59),1);
 // Foreground edge and corner stones.
 Fill(d,{0,clientH-30,clientW,clientH},RGB(39,37,33));
 Line(d,0,clientH-30,clientW,clientH-30,RGB(130,107,72),2);
 // Combat HUD.
 Fill(d,{38,28,clientW-38,105},RGB(18,21,24));Frame(d,{38,28,clientW-38,105},RGB(91,78,59));
 DrawBar(d,65,64,365,player.hp,L"GUERRERO AZUL",false);
 DrawBar(d,clientW-430,64,365,enemy.hp,L"GUERRERO ROJO",true);
 Text(d,L"CONFLICT",clientW/2-68,34,16,pale,true,DT_CENTER);
 Text(d,RustReady()?L"RUST CORE  /  ONLINE":L"RUST CORE  /  FALLBACK",clientW/2-95,70,10,RustReady()?RGB(133,177,132):muted,true,DT_CENTER);
 wchar_t roundLabel[32]{};wsprintfW(roundLabel,L"RONDA  %02d",roundNumber);
 Text(d,roundLabel,clientW/2-55,119,13,gold,true,DT_CENTER);
 int ground=clientH-82;
 DrawFighter(d,player,true,ground);DrawFighter(d,enemy,false,ground);
 Line(d,0,ground,clientW,ground,RGB(153,126,83),2);
 Text(d,L"A / D o FLECHAS  MOVER     J  ATACAR     K  BLOQUEAR     R  REINICIAR     ESC  MENÚ",35,clientH-53,12,pale,true,DT_CENTER);
 if(gameOver){
   Fill(d,{clientW/2-255,clientH/2-100,clientW/2+255,clientH/2+96},RGB(17,19,21));
   Frame(d,{clientW/2-255,clientH/2-100,clientW/2+255,clientH/2+96},gold);
   Text(d,playerWon?L"VICTORIA":L"DERROTA",clientW/2-155,clientH/2-69,36,playerWon?gold:pale,true,DT_CENTER);
   Text(d,L"La arena tiene un vencedor",clientW/2-145,clientH/2-15,14,muted,false,DT_CENTER);
   Text(d,L"Pulsa R para otra ronda o ESC para salir",clientW/2-205,clientH/2+45,14,pale,false,DT_CENTER);
 }
}
void DrawSubpage(HDC d){
 const wchar_t*head=L"";const wchar_t*l1=L"",*l2=L"",*l3=L"";
 switch(page){case Page::Campaign:head=L"CAMPAÑA";l1=L"Una travesía a través de las eras.";l2=L"Comienza en el año 0 y descubre cómo cambia el combate.";l3=L"PRÓXIMAMENTE — progresión histórica y desafíos.";break;
 case Page::Arsenal:head=L"ARSENAL";l1=L"PALOS / LANZAS / ESPADAS / ESCUDOS";l2=L"Cada arma tendrá alcance, velocidad y estilo propios.";l3=L"El arsenal crecerá junto con las épocas.";break;
 case Page::Eras:head=L"CRONOLOGÍA";l1=L"AÑO 0       ANTIGÜEDAD       EDAD MEDIA";l2=L"EDAD MODERNA       ERA INDUSTRIAL       ACTUALIDAD";l3=L"Las épocas avanzan por etapas.";break;
 case Page::Settings:head=L"AJUSTES";l1=L"RESOLUCIÓN — ventana adaptable";l2=L"CONTROLES — teclado y ratón";l3=L"Más opciones estarán disponibles en futuras versiones.";break;default:break;}
 Text(d,L"CONFLICT  /  MENÚ PRINCIPAL",70,38,12,muted,true);Line(d,70,66,clientW-70,66,RGB(55,49,40));Text(d,head,70,130,42,pale,true);Line(d,70,194,clientW-70,194,gold,2);Text(d,l1,76,245,18,pale);Text(d,l2,76,300,18,pale);Text(d,l3,76,355,18,gold,true);Text(d,L"VOLVER  [ESC]",70,clientH-110,16,pale,true);
}
void Paint(HWND hwnd){PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT rc;GetClientRect(hwnd,&rc);clientW=rc.right;clientH=rc.bottom;HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,clientW,clientH);HBITMAP old=(HBITMAP)SelectObject(mem,bmp);Fill(mem,{0,0,clientW,clientH},bg);if(page==Page::Menu)DrawMenu(mem);else if(page==Page::QuickFight)DrawArena(mem);else DrawSubpage(mem);BitBlt(dc,0,0,clientW,clientH,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(hwnd,&ps);}
void Hit(Fighter&target,Fighter&attacker){
 if(target.hp<=0||target.hurt>0)return;
 float dist=std::abs(target.x-attacker.x);
 bool inRange=rustCanHit?rustCanHit(dist,112.0f)!=0:dist<112.0f;
 if(inRange){
   int damage=rustDamage?rustDamage(14,target.blocking?1:0):(target.blocking?4:14);
   target.hp=std::max(0,target.hp-damage);target.hurt=0.18f;
 }
}
void Update(HWND hwnd){
 if(page!=Page::QuickFight||gameOver)return;
 const float dt=0.016f;
 player.blocking=keys['K'];
 if(keys['A']||keys[VK_LEFT]){player.x-=245*dt;player.facing=-1;}
 if(keys['D']||keys[VK_RIGHT]){player.x+=245*dt;player.facing=1;}
 player.x=std::clamp(player.x,45.0f,(float)clientW-45);
 enemy.facing=enemy.x>player.x?-1:1;
 if(player.attack>0)player.attack=std::max(0.0f,player.attack-dt);
 if(enemy.attack>0)enemy.attack=std::max(0.0f,enemy.attack-dt);
 if(player.cooldown>0)player.cooldown-=dt;
 if(enemy.cooldown>0)enemy.cooldown-=dt;
 if(player.hurt>0)player.hurt-=dt;
 if(enemy.hurt>0)enemy.hurt-=dt;
 if(keys['J']&&player.cooldown<=0){player.attack=0.22f;player.cooldown=0.48f;Hit(enemy,player);}
 // Simple rival AI: approach, then strike when close.
 float dist=std::abs(enemy.x-player.x);
 enemy.blocking=dist<125 && player.attack>0.05f;
 if(dist>100)enemy.x+=(enemy.x>player.x?-1:1)*125*dt;
 else if(enemy.cooldown<=0){enemy.attack=0.2f;enemy.cooldown=0.95f;Hit(player,enemy);}
 enemy.x=std::clamp(enemy.x,45.0f,(float)clientW-45);
 if(player.hp<=0||enemy.hp<=0){gameOver=true;playerWon=enemy.hp<=0;}
 InvalidateRect(hwnd,nullptr,FALSE);
}
void Activate(HWND hwnd){if(page!=Page::Menu){page=Page::Menu;InvalidateRect(hwnd,nullptr,FALSE);return;}Page target=menuItems[selected].target;if(target==Page::QuickFight){StartFight(hwnd);}else{page=target;InvalidateRect(hwnd,nullptr,FALSE);}}
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_PAINT:Paint(hwnd);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_TIMER:if(wp==1)Update(hwnd);return 0;
 case WM_KEYDOWN:
   if(wp<256)keys[wp]=true;
   if(wp==VK_ESCAPE){page=Page::Menu;selected=0;KillTimer(hwnd,1);InvalidateRect(hwnd,nullptr,FALSE);}
   else if(page==Page::Menu&&(wp==VK_DOWN||wp==VK_UP)){selected=(selected+(wp==VK_DOWN?1:MENU_COUNT-1))%MENU_COUNT;InvalidateRect(hwnd,nullptr,FALSE);}
   else if(page==Page::Menu&&wp==VK_RETURN)Activate(hwnd);
   else if(page==Page::QuickFight&&wp=='R'){ResetFight(true);InvalidateRect(hwnd,nullptr,FALSE);}
   return 0;
 case WM_KEYUP:if(wp<256)keys[wp]=false;return 0;
 case WM_MOUSEMOVE:if(page==Page::Menu){POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};int n=-1;for(int i=0;i<MENU_COUNT;i++){RECT r=MenuRect(i);if(PtInRect(&r,p))n=i;}if(n!=hover){hover=n;InvalidateRect(hwnd,nullptr,FALSE);}}return 0;
 case WM_LBUTTONUP:if(page==Page::Menu){POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};for(int i=0;i<MENU_COUNT;i++){RECT r=MenuRect(i);if(PtInRect(&r,p)){selected=i;Activate(hwnd);break;}}}else if(page!=Page::QuickFight){page=Page::Menu;selected=0;InvalidateRect(hwnd,nullptr,FALSE);}return 0;
 case WM_DESTROY:KillTimer(hwnd,1);if(rustModule){FreeLibrary(rustModule);rustModule=nullptr;}PostQuitMessage(0);return 0;
 }return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetProcessDPIAware();WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=L"ConflictWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
 RECT r{0,0,1100,780};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"CONFLICT — Ages of War",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!hwnd)return 1;LoadRustCore();ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;
}