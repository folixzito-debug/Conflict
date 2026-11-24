#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <algorithm>
#include <cmath>

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
void ResetFight(){
 player={260.0f,0.0f,100,1,false,0.0f,0.0f,0.0f};
 enemy={760.0f,0.0f,100,-1,false,0.0f,0.4f,0.0f};
 gameOver=false; playerWon=false; roundNumber++;
}
void StartFight(){ page=Page::QuickFight; ResetFight(); SetTimer(nullptr,1,16,nullptr); }
void Fill(HDC d,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(d,&r,b);DeleteObject(b);}
void Text(HDC d,const wchar_t*s,int x,int y,int size,COLORREF c,bool bold=false,UINT align=DT_LEFT){
 HFONT f=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
 HFONT old=(HFONT)SelectObject(d,f);SetBkMode(d,TRANSPARENT);SetTextColor(d,c);
 RECT r{x,y,clientW-25,y+size+18};DrawTextW(d,s,-1,&r,align|DT_SINGLELINE|DT_VCENTER);SelectObject(d,old);DeleteObject(f);
}
void Line(HDC d,int x1,int y1,int x2,int y2,COLORREF c,int width=1){HPEN p=CreatePen(PS_SOLID,width,c),o=(HPEN)SelectObject(d,p);MoveToEx(d,x1,y1,nullptr);LineTo(d,x2,y2);SelectObject(d,o);DeleteObject(p);}
void Frame(HDC d,RECT r,COLORREF c){HPEN p=CreatePen(PS_SOLID,1,c),*unused=nullptr; HGDIOBJ op=SelectObject(d,p),ob=SelectObject(d,GetStockObject(HOLLOW_BRUSH));Rectangle(d,r.left,r.top,r.right,r.bottom);SelectObject(d,op);SelectObject(d,ob);DeleteObject(p);}
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
void DrawFighter(HDC d,const Fighter&f,bool isPlayer,int ground){
 int x=(int)f.x, y=ground-12;COLORREF armor=isPlayer?RGB(69,105,139):RGB(142,75,61),edge=isPlayer?RGB(128,166,194):RGB(191,133,104);
 // shadow
 HBRUSH sh=CreateSolidBrush(RGB(24,23,21));RECT sr{x-35,ground-7,x+36,ground+3};FillRect(d,&sr,sh);DeleteObject(sh);
 // legs, torso, helmet
 Fill(d,{x-19,y-58,x-5,y-12},RGB(74,70,62));Fill(d,{x+5,y-58,x+19,y-12},RGB(74,70,62));
 Fill(d,{x-25,y-115,x+25,y-52},armor);Frame(d,{x-25,y-115,x+25,y-52},edge);
 Fill(d,{x-18,y-146,x+18,y-112},RGB(112,105,89));Frame(d,{x-18,y-146,x+18,y-112},gold);
 Fill(d,{x-20,y-135,x+20,y-127},RGB(35,35,34));
 // arm
 Line(d,x+(f.facing*16),y-101,x+(f.facing*38),y-79,edge,9);
 // sword; extends during attack
 int reach=(f.attack>0)?72:48;
 int sx=x+f.facing*38, sy=y-79;
 Line(d,sx,sy,sx+f.facing*reach,sy-20,RGB(185,190,192),4);
 Line(d,sx-f.facing*3,sy+7,sx+f.facing*8,sy-7,gold,5);
 if(f.blocking) {Line(d,x+f.facing*31,y-135,x+f.facing*65,y-45,gold,5);}
 if(f.hurt>0){Frame(d,{x-33,y-153,x+33,y+1},RGB(230,190,120));}
}
void DrawArena(HDC d){
 Fill(d,{0,0,clientW,clientH},RGB(20,23,27));
 // sky and distant battlements
 Fill(d,{0,0,clientW,390},RGB(30,34,39));
 for(int i=0;i<9;i++){int bx=i*145-20;Fill(d,{bx,245,bx+115,395},RGB(39,42,44));Fill(d,{bx+12,220,bx+32,245},RGB(39,42,44));Fill(d,{bx+72,220,bx+92,245},RGB(39,42,44));}
 Line(d,0,395,clientW,395,gold,2);
 Fill(d,{0,397,clientW,clientH},RGB(54,48,39));
 for(int x=0;x<clientW;x+=100)Line(d,x,398,x-35,clientH,RGB(69,60,48));
 for(int y=435;y<clientH;y+=58)Line(d,0,y,clientW,y,RGB(65,57,46));
 DrawBar(d,65,58,390,player.hp,L"JUGADOR",false);
 DrawBar(d,clientW-455,58,390,enemy.hp,L"RIVAL",true);
 Text(d,L"RONDA  01",clientW/2-55,64,14,gold,true);
 int ground=clientH-95;
 DrawFighter(d,player,true,ground);DrawFighter(d,enemy,false,ground);
 Line(d,0,ground,clientW,ground,RGB(150,122,78),2);
 Text(d,L"A / D o FLECHAS  MOVER     J  ATACAR     K  BLOQUEAR     R  REINICIAR     ESC  MENÚ",35,clientH-53,13,pale,true);
 if(gameOver){Fill(d,{clientW/2-245,clientH/2-92,clientW/2+245,clientH/2+88},RGB(17,19,21));Frame(d,{clientW/2-245,clientH/2-92,clientW/2+245,clientH/2+88},gold);Text(d,playerWon?L"VICTORIA":L"DERROTA",clientW/2-150,clientH/2-60,36,playerWon?gold:pale,true);Text(d,L"Pulsa R para otra ronda o ESC para salir",clientW/2-205,clientH/2+8,15,pale);}
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
void Hit(Fighter&target,Fighter&attacker,bool fromPlayer){
 if(target.hp<=0||target.hurt>0)return;
 float dist=std::abs(target.x-attacker.x);
 if(dist<112){int damage=target.blocking?4:14;target.hp=std::max(0,target.hp-damage);target.hurt=0.18f;}
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
 if(keys['J']&&player.cooldown<=0){player.attack=0.22f;player.cooldown=0.48f;Hit(enemy,player,true);}
 // Simple rival AI: approach, then strike when close.
 float dist=std::abs(enemy.x-player.x);
 enemy.blocking=dist<125 && player.attack>0.05f;
 if(dist>100)enemy.x+=(enemy.x>player.x?-1:1)*125*dt;
 else if(enemy.cooldown<=0){enemy.attack=0.2f;enemy.cooldown=0.95f;Hit(player,enemy,false);}
 enemy.x=std::clamp(enemy.x,45.0f,(float)clientW-45);
 if(player.hp<=0||enemy.hp<=0){gameOver=true;playerWon=enemy.hp<=0;}
 InvalidateRect(hwnd,nullptr,FALSE);
}
void Activate(HWND hwnd){if(page!=Page::Menu){page=Page::Menu;InvalidateRect(hwnd,nullptr,FALSE);return;}Page target=menuItems[selected].target;if(target==Page::QuickFight){StartFight();}else{page=target;InvalidateRect(hwnd,nullptr,FALSE);}}
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
   else if(page==Page::QuickFight&&wp=='R'){ResetFight();InvalidateRect(hwnd,nullptr,FALSE);}
   return 0;
 case WM_KEYUP:if(wp<256)keys[wp]=false;return 0;
 case WM_MOUSEMOVE:if(page==Page::Menu){POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};int n=-1;for(int i=0;i<MENU_COUNT;i++)if(PtInRect(&MenuRect(i),p))n=i;if(n!=hover){hover=n;InvalidateRect(hwnd,nullptr,FALSE);}}return 0;
 case WM_LBUTTONUP:if(page==Page::Menu){POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};for(int i=0;i<MENU_COUNT;i++){RECT r=MenuRect(i);if(PtInRect(&r,p)){selected=i;Activate(hwnd);break;}}}else if(page!=Page::QuickFight){page=Page::Menu;selected=0;InvalidateRect(hwnd,nullptr,FALSE);}return 0;
 case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
 }return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetProcessDPIAware();WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=L"ConflictWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
 RECT r{0,0,1100,780};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"CONFLICT — Ages of War",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!hwnd)return 1;ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;
}