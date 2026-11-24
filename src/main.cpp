#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>

namespace {
enum class Page { Menu, Campaign, QuickFight, Arsenal, Eras, Settings };
Page page = Page::Menu;
int selected = 0;
int hover = -1;
bool fullscreenHint = false;
const wchar_t* title = L"CONFLICT";
const wchar_t* subtitle = L"AGES OF WAR";

struct Item { const wchar_t* label; const wchar_t* detail; Page target; };
const Item menuItems[] = {
    {L"CAMPAÑA", L"Avanza por las eras de la historia", Page::Campaign},
    {L"COMBATE RÁPIDO", L"Entra directamente a la arena", Page::QuickFight},
    {L"ARSENAL", L"Descubre armas de cada época", Page::Arsenal},
    {L"ÉPOCAS", L"Del año 0 hasta la era actual", Page::Eras},
    {L"AJUSTES", L"Configura tu experiencia", Page::Settings}
};
constexpr int MENU_COUNT = sizeof(menuItems) / sizeof(menuItems[0]);

COLORREF bg = RGB(13, 15, 18);
COLORREF panel = RGB(23, 26, 30);
COLORREF panel2 = RGB(31, 34, 38);
COLORREF gold = RGB(190, 151, 83);
COLORREF pale = RGB(224, 218, 202);
COLORREF muted = RGB(139, 140, 137);

void Fill(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b);
}
void Text(HDC dc, const wchar_t* s, int x, int y, int size, COLORREF color, bool bold=false, UINT align=DT_LEFT) {
    HFONT f = CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    HFONT old=(HFONT)SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    RECT r{x,y,1450,y+size+18}; DrawTextW(dc,s,-1,&r,align|DT_SINGLELINE|DT_VCENTER);
    SelectObject(dc,old); DeleteObject(f);
}
void Line(HDC dc,int x1,int y1,int x2,int y2,COLORREF c,int width=1) {
    HPEN p=CreatePen(PS_SOLID,width,c); HPEN old=(HPEN)SelectObject(dc,p);
    MoveToEx(dc,x1,y1,nullptr); LineTo(dc,x2,y2); SelectObject(dc,old); DeleteObject(p);
}
void Frame(HDC dc, RECT r, COLORREF c) {
    HPEN p=CreatePen(PS_SOLID,1,c); HBRUSH old=(HBRUSH)SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    HPEN op=(HPEN)SelectObject(dc,p); Rectangle(dc,r.left,r.top,r.right,r.bottom);
    SelectObject(dc,op); SelectObject(dc,old); DeleteObject(p);
}
RECT ButtonRect(int i) { return RECT{70, 270+i*78, 550, 330+i*78}; }

void DrawBackground(HDC dc,int w,int h) {
    Fill(dc,RECT{0,0,w,h},bg);
    for(int i=0;i<9;i++) Line(dc,0,95+i*72,w,95+i*72,RGB(19,21,24));
    HPEN p=CreatePen(PS_SOLID,5,RGB(65,57,45)); HPEN old=(HPEN)SelectObject(dc,p);
    MoveToEx(dc,w-350,135,nullptr); LineTo(dc,w-115,365);
    MoveToEx(dc,w-115,135,nullptr); LineTo(dc,w-350,365);
    SelectObject(dc,old); DeleteObject(p);
    Line(dc,w-315,225,w-150,225,RGB(48,43,36),2);
    Line(dc,w-315,275,w-150,275,RGB(48,43,36),2);
    Text(dc,L"ESTRATEGIA  /  COMBATE  /  HISTORIA",70,38,12,muted,true);
    Line(dc,70,66,w-70,66,RGB(55,49,40));
    Text(dc,L"CONFLICT",70,h-48,12,muted,true);
    Text(dc,L"PROTOTIPO DE INTERFAZ  •  PRE-ALPHA",w-380,h-48,12,muted,true);
}
void DrawMenu(HDC dc,int w,int h) {
    Text(dc,title,70,105,62,pale,true);
    Text(dc,subtitle,74,174,17,gold,true);
    Text(dc,L"UNA HISTORIA ESCRITA EN ACERO",72,211,13,muted,false);
    for(int i=0;i<MENU_COUNT;i++) {
        RECT r=ButtonRect(i);
        bool active=(i==selected || i==hover);
        Fill(dc,r,active?RGB(48,39,30):panel);
        Frame(dc,r,active?gold:RGB(49,51,53));
        if(active) Fill(dc,RECT{r.left,r.top,r.left+4,r.bottom},gold);
        Text(dc,menuItems[i].label,r.left+22,r.top+7,19,active?pale:RGB(185,185,179),true);
        Text(dc,menuItems[i].detail,r.left+22,r.top+32,11,muted);
        Text(dc,(i==selected?L">":L""),r.right-36,r.top+15,18,gold,true);
    }
    Text(dc,L"SELECCIONA TU SIGUIENTE BATALLA",70,690,12,muted,true);
    Text(dc,L"↑  ↓  NAVEGAR       ENTER  CONFIRMAR       ESC  VOLVER",70,716,12,muted);
}
void DrawSubpage(HDC dc,int w,int h) {
    const wchar_t* heading=L"";
    const wchar_t* lines[4]={};
    switch(page) {
    case Page::Campaign:
        heading=L"CAMPAÑA";
        lines[0]=L"Una travesía a través de las eras.";
        lines[1]=L"Comienza en el año 0 y descubre cómo cambia el combate.";
        lines[2]=L"PRÓXIMAMENTE  —  Progresión histórica y desafíos.";
        break;
    case Page::QuickFight:
        heading=L"COMBATE RÁPIDO";
        lines[0]=L"Prepara una pelea directa en una arena 2D.";
        lines[1]=L"Primer objetivo: movimiento, ataque, bloqueo y vida.";
        lines[2]=L"PRÓXIMAMENTE  —  Primer prototipo jugable.";
        break;
    case Page::Arsenal:
        heading=L"ARSENAL";
        lines[0]=L"PALOS  /  LANZAS  /  ESPADAS  /  ESCUDOS";
        lines[1]=L"Cada arma tendrá alcance, velocidad y estilo propios.";
        lines[2]=L"El arsenal crecerá junto con las épocas.";
        break;
    case Page::Eras:
        heading=L"CRONOLOGÍA";
        lines[0]=L"AÑO 0       ANTIGÜEDAD       EDAD MEDIA";
        lines[1]=L"EDAD MODERNA       ERA INDUSTRIAL       ACTUALIDAD";
        lines[2]=L"Las épocas avanzan por etapas: no es un juego en tiempo real.";
        break;
    case Page::Settings:
        heading=L"AJUSTES";
        lines[0]=L"RESOLUCIÓN  —  Ventana adaptable";
        lines[1]=L"CONTROLES  —  Teclado y ratón para navegar";
        lines[2]=L"Más opciones estarán disponibles en futuras versiones.";
        break;
    default: break;
    }
    Text(dc,L"CONFLICT  /  MENÚ PRINCIPAL",70,38,12,muted,true);
    Line(dc,70,66,w-70,66,RGB(55,49,40));
    Text(dc,heading,70,130,42,pale,true);
    Line(dc,70,194,w-70,194,gold,2);
    for(int i=0;i<3;i++) if(lines[i]) Text(dc,lines[i],76,245+i*55,18,i==2?gold:pale,i==2);
    RECT back{70,h-135,310,h-78}; Fill(dc,back,panel2); Frame(dc,back,gold);
    Text(dc,L"VOLVER  [ESC]",92,h-126,16,pale,true);
    Text(dc,L"CONFLICT  •  PROTOTIPO DE INTERFAZ",70,h-48,12,muted,true);
}
void Paint(HWND hwnd) {
    PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps);
    RECT client; GetClientRect(hwnd,&client); int w=client.right,h=client.bottom;
    HDC mem=CreateCompatibleDC(dc); HBITMAP bmp=CreateCompatibleBitmap(dc,w,h);
    HBITMAP old=(HBITMAP)SelectObject(mem,bmp);
    DrawBackground(mem,w,h);
    if(page==Page::Menu) DrawMenu(mem,w,h); else DrawSubpage(mem,w,h);
    BitBlt(dc,0,0,w,h,mem,0,0,SRCCOPY);
    SelectObject(mem,old); DeleteObject(bmp); DeleteDC(mem); EndPaint(hwnd,&ps);
}
void Activate(HWND hwnd) {
    if(page!=Page::Menu) { page=Page::Menu; selected=0; InvalidateRect(hwnd,nullptr,FALSE); return; }
    page=menuItems[selected].target; InvalidateRect(hwnd,nullptr,FALSE);
}
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_PAINT: Paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE) { page=Page::Menu; selected=0; InvalidateRect(hwnd,nullptr,FALSE); }
        else if(page==Page::Menu && (wp==VK_DOWN || wp==VK_UP)) {
            selected=(selected+(wp==VK_DOWN?1:MENU_COUNT-1))%MENU_COUNT; InvalidateRect(hwnd,nullptr,FALSE);
        } else if(wp==VK_RETURN) Activate(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if(page==Page::Menu) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; int next=-1;
            for(int i=0;i<MENU_COUNT;i++) { RECT r=ButtonRect(i); if(PtInRect(&r,p)) next=i; }
            if(next!=hover) { hover=next; InvalidateRect(hwnd,nullptr,FALSE); }
        }
        return 0;
    case WM_LBUTTONUP:
        if(page==Page::Menu) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            for(int i=0;i<MENU_COUNT;i++) { RECT r=ButtonRect(i); if(PtInRect(&r,p)) { selected=i; Activate(hwnd); break; } }
        } else { page=Page::Menu; selected=0; InvalidateRect(hwnd,nullptr,FALSE); }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR,int show) {
    SetProcessDPIAware();
    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance;
    wc.lpszClassName=L"ConflictWindow"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
    RECT r{0,0,1100,780}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"CONFLICT — Ages of War",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
        r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
    if(!hwnd) return 1;
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}