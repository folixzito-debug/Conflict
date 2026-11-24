#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <limits>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {
constexpr wchar_t WINDOW_CLASS[] = L"Conflict3DWindow";
constexpr UINT MAX_UNITS = 64;
constexpr float PI = 3.1415926535f;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};

struct UnitView {
    float x;
    float z;
    float heading;
    std::uint32_t team;
    std::uint32_t alive;
    std::uint32_t kind;
    float supportPulse;
};

struct MatrixBuffer {
    XMFLOAT4X4 viewProjection;
    XMFLOAT4 lighting;
};

struct MeshBuilder {
    std::vector<Vertex> vertices;

    void Triangle(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c,
                  const XMFLOAT4& color) {
        XMVECTOR va = XMLoadFloat3(&a);
        XMVECTOR vb = XMLoadFloat3(&b);
        XMVECTOR vc = XMLoadFloat3(&c);
        XMVECTOR cross = XMVector3Cross(vb - va, vc - va);
        XMVECTOR normal = XMVectorGetX(XMVector3LengthSq(cross)) < 1.0e-10f
            ? XMVectorSet(0.0f,1.0f,0.0f,0.0f) : XMVector3Normalize(cross);
        XMFLOAT3 n{};
        XMStoreFloat3(&n, normal);
        vertices.push_back({a, n, color});
        vertices.push_back({b, n, color});
        vertices.push_back({c, n, color});
    }

    void Quad(const XMFLOAT3& a, const XMFLOAT3& b,
              const XMFLOAT3& c, const XMFLOAT3& d,
              const XMFLOAT4& color) {
        Triangle(a, b, c, color);
        Triangle(a, c, d, color);
    }

    XMFLOAT3 RotatePoint(float x,float y,float z,float yaw) const {
        const float c=std::cos(yaw), s=std::sin(yaw);
        return XMFLOAT3{x*c+z*s,y,-x*s+z*c};
    }

    void BoxYaw(float cx,float cy,float cz,float sx,float sy,float sz,
                float yaw,const XMFLOAT4& color) {
        const float x0=-sx*0.5f,x1=sx*0.5f;
        const float y0=-sy*0.5f,y1=sy*0.5f;
        const float z0=-sz*0.5f,z1=sz*0.5f;
        auto p=[&](float x,float y,float z) {
            const auto r=RotatePoint(x,y,z,yaw);
            return XMFLOAT3{cx+r.x,cy+r.y,cz+r.z};
        };
        const XMFLOAT3 p000=p(x0,y0,z0),p001=p(x0,y0,z1),p010=p(x0,y1,z0),p011=p(x0,y1,z1);
        const XMFLOAT3 p100=p(x1,y0,z0),p101=p(x1,y0,z1),p110=p(x1,y1,z0),p111=p(x1,y1,z1);
        Quad(p000,p001,p011,p010,color);
        Quad(p100,p110,p111,p101,color);
        Quad(p000,p010,p110,p100,color);
        Quad(p001,p101,p111,p011,color);
        Quad(p000,p100,p101,p001,color);
        Quad(p010,p011,p111,p110,color);
    }

    void Box(float cx,float cy,float cz,float sx,float sy,float sz,const XMFLOAT4& color) {
        BoxYaw(cx,cy,cz,sx,sy,sz,0.0f,color);
    }

    void Cylinder(float x, float y, float z, float radius, float height,
                  const XMFLOAT4& color, int segments=9) {
        for(int i=0;i<segments;++i) {
            const float a0=2.0f*PI*i/segments;
            const float a1=2.0f*PI*(i+1)/segments;
            const XMFLOAT3 b0{x+std::cos(a0)*radius,y,z+std::sin(a0)*radius};
            const XMFLOAT3 b1{x+std::cos(a1)*radius,y,z+std::sin(a1)*radius};
            const XMFLOAT3 t0{b0.x,y+height,b0.z};
            const XMFLOAT3 t1{b1.x,y+height,b1.z};
            Quad(b0,b1,t1,t0,color);
            Triangle({x,y+height,z},t0,t1,color);
            Triangle({x,y,z},b1,b0,color);
        }
    }

    void Sphere(float cx,float cy,float cz,float rx,float ry,float rz,
                const XMFLOAT4& color,int rings=9,int slices=12) {
        for(int ring=0;ring<rings;++ring) {
            const float lat0=-PI*0.5f+PI*ring/rings;
            const float lat1=-PI*0.5f+PI*(ring+1)/rings;
            for(int slice=0;slice<slices;++slice) {
                const float lon0=2.0f*PI*slice/slices;
                const float lon1=2.0f*PI*(slice+1)/slices;
                auto point=[&](float lat,float lon) {
                    return XMFLOAT3{
                        cx+std::cos(lat)*std::cos(lon)*rx,
                        cy+std::sin(lat)*ry,
                        cz+std::cos(lat)*std::sin(lon)*rz
                    };
                };
                const auto a=point(lat0,lon0), b=point(lat0,lon1);
                const auto c=point(lat1,lon1), d=point(lat1,lon0);
                Triangle(a,c,b,color);
                Triangle(a,d,c,color);
            }
        }
    }
};

constexpr XMFLOAT4 ROCK{0.37f,0.39f,0.38f,1.0f};
constexpr XMFLOAT4 ROCK_LIGHT{0.43f,0.45f,0.43f,1.0f};
constexpr XMFLOAT4 ROCK_DARK{0.28f,0.30f,0.30f,1.0f};
constexpr XMFLOAT4 TRUNK{0.25f,0.16f,0.10f,1.0f};
constexpr XMFLOAT4 LEAF_A{0.12f,0.30f,0.17f,1.0f};
constexpr XMFLOAT4 LEAF_B{0.18f,0.38f,0.20f,1.0f};
constexpr XMFLOAT4 LEAF_C{0.24f,0.43f,0.22f,1.0f};

float TerrainHeight(float x,float z) {
    return 0.34f + 0.16f*std::sin(x*0.22f)*std::cos(z*0.20f)
                 + 0.075f*std::cos(z*0.43f+x*0.11f);
}

void BuildTree(MeshBuilder& mesh,float x,float z,float scale,int quality) {
    const float ground=TerrainHeight(x,z);
    const int trunkSegments=quality==0?6:(quality==1?8:12);
    const int rings=quality==0?6:(quality==1?8:12);
    const int slices=quality==0?8:(quality==1?10:16);
    mesh.Cylinder(x,ground,z,0.13f*scale,1.55f*scale,TRUNK,trunkSegments);
    mesh.Sphere(x,ground+1.95f*scale,z,0.78f*scale,0.88f*scale,0.78f*scale,LEAF_A,rings,slices);
    if(quality>0) {
        mesh.Sphere(x-0.42f*scale,ground+1.72f*scale,z+0.22f*scale,
                    0.52f*scale,0.62f*scale,0.54f*scale,LEAF_B,rings-1,slices-1);
        mesh.Sphere(x+0.39f*scale,ground+1.78f*scale,z-0.18f*scale,
                    0.55f*scale,0.63f*scale,0.53f*scale,LEAF_C,rings-1,slices-1);
    }
    if(quality>=2) {
        mesh.Sphere(x,ground+2.42f*scale,z,0.42f*scale,0.45f*scale,0.42f*scale,LEAF_C,7,10);
    }
}

void BuildWorld(MeshBuilder& mesh,int quality) {
    // The rocky underside is a visible physical base, not a 2D map.
    mesh.Box(0.0f,-0.475f,0.0f,32.0f,1.15f,24.0f,ROCK);
    mesh.Box(0.0f,-1.14f,0.0f,31.4f,0.18f,23.4f,ROCK_DARK);
    // Low-poly broken stone facets around the base edge.
    for(int i=0;i<18;++i) {
        float x=-14.5f+i*1.7f;
        mesh.Box(x,-0.475f,-11.93f,1.25f,0.34f,0.12f,(i%2)?ROCK_LIGHT:ROCK_DARK);
        mesh.Box(x,-0.475f,11.93f,1.25f,0.34f,0.12f,(i%3)?ROCK:ROCK_LIGHT);
    }

    // Uneven soil plane with smooth rolling ground.
    const int NX=quality==0?24:(quality==1?48:(quality==2?80:120));
    const int NZ=quality==0?18:(quality==1?36:(quality==2?60:90));
    constexpr float XMIN=-14.7f, XMAX=14.7f, ZMIN=-10.7f, ZMAX=10.7f;
    for(int iz=0;iz<NZ;++iz) {
        for(int ix=0;ix<NX;++ix) {
            float x0=XMIN+(XMAX-XMIN)*ix/NX;
            float x1=XMIN+(XMAX-XMIN)*(ix+1)/NX;
            float z0=ZMIN+(ZMAX-ZMIN)*iz/NZ;
            float z1=ZMIN+(ZMAX-ZMIN)*(iz+1)/NZ;
            XMFLOAT3 a{x0,TerrainHeight(x0,z0),z0};
            XMFLOAT3 b{x1,TerrainHeight(x1,z0),z0};
            XMFLOAT3 c{x1,TerrainHeight(x1,z1),z1};
            XMFLOAT3 d{x0,TerrainHeight(x0,z1),z1};
            const float variation=0.018f*std::sin(x0*1.7f+z0*2.1f);
            XMFLOAT4 soil{0.30f+variation,0.35f+variation,0.23f+variation*0.4f,1.0f};
            mesh.Quad(a,b,c,d,soil);
        }
    }

    // Trees frame the open central clearing so troops stay visible.
    const float trees[][3]={
        {-13.2f,-9.1f,1.05f},{-10.8f,-9.4f,0.88f},{-7.8f,-9.2f,0.98f},
        {-3.7f,-9.3f,0.82f},{0.2f,-9.2f,0.96f},{4.0f,-9.3f,0.92f},
        {8.0f,-9.1f,1.04f},{11.3f,-9.0f,0.92f},{13.2f,-7.0f,1.0f},
        {-13.3f,-5.2f,0.95f},{13.4f,-3.8f,0.90f},{-13.3f,-1.0f,1.0f},
        {13.3f,0.8f,1.05f},{-13.2f,3.1f,0.88f},{13.1f,4.4f,0.95f},
        {-12.3f,8.5f,1.04f},{-9.0f,9.0f,0.92f},{-5.2f,9.0f,1.02f},
        {-1.4f,9.1f,0.90f},{2.5f,9.0f,0.98f},{6.5f,9.0f,0.91f},
        {10.0f,8.9f,1.02f},{13.0f,8.1f,0.92f}
    };
    for(const auto& tree:trees) BuildTree(mesh,tree[0],tree[1],tree[2],quality);

    // A few scattered stones give the soil a natural edge.
    const float stones[][2]={{-11.8f,-6.9f},{-8.7f,7.5f},{-5.9f,-7.8f},{6.0f,7.7f},
                             {9.8f,-7.2f},{12.1f,6.8f},{-12.0f,5.8f},{3.0f,7.7f}};
    for(const auto& s:stones) {
        float y=TerrainHeight(s[0],s[1]);
        mesh.Sphere(s[0],y+0.13f,s[1],0.38f,0.20f,0.31f,ROCK_LIGHT,
                    quality==0?4:(quality==1?6:9),quality==0?6:(quality==1?8:12));
    }
    if(quality==2) {
        for(int i=0;i<28;++i) {
            const float x=-13.8f+static_cast<float>(i%14)*2.12f;
            const float z=(i<14?-7.8f:7.8f);
            const float y=TerrainHeight(x,z);
            mesh.Sphere(x,y+0.035f,z,0.11f,0.045f,0.16f,
                        (i%2)?LEAF_B:ROCK_DARK,4,6);
        }
    }
}



void BuildSupportRing(MeshBuilder& mesh,float x,float y,float z,float pulse,int quality) {
    if(pulse<=0.0f) return;
    const int segments=quality==0?10:(quality==1?16:28);
    const float alpha=std::clamp(pulse/0.18f,0.0f,1.0f)*0.62f;
    const XMFLOAT4 glow{0.22f,0.82f,0.48f,alpha};
    const float inner=0.62f,outer=0.78f;
    for(int i=0;i<segments;++i) {
        const float a0=2.0f*PI*i/segments,a1=2.0f*PI*(i+1)/segments;
        const XMFLOAT3 p0{x+std::cos(a0)*inner,y,z+std::sin(a0)*inner};
        const XMFLOAT3 p1{x+std::cos(a0)*outer,y,z+std::sin(a0)*outer};
        const XMFLOAT3 p2{x+std::cos(a1)*outer,y,z+std::sin(a1)*outer};
        const XMFLOAT3 p3{x+std::cos(a1)*inner,y,z+std::sin(a1)*inner};
        mesh.Quad(p0,p1,p2,p3,glow);
    }
}

void BuildSphereUnit(MeshBuilder& mesh,const UnitView& unit,int quality,float time) {
    const float x=unit.x,z=unit.z,ground=TerrainHeight(x,z),yaw=unit.heading;
    const XMFLOAT4 team=unit.team==0?XMFLOAT4{0.12f,0.34f,0.76f,1.0f}:XMFLOAT4{0.76f,0.16f,0.13f,1.0f};
    const XMFLOAT4 bright=unit.team==0?XMFLOAT4{0.30f,0.56f,0.96f,1.0f}:XMFLOAT4{0.98f,0.37f,0.29f,1.0f};
    const XMFLOAT4 dark{0.10f,0.12f,0.14f,1.0f};
    const XMFLOAT4 steel{0.72f,0.77f,0.80f,1.0f};
    const int rings=quality==0?7:(quality==1?10:(quality==2?14:20));
    const int slices=quality==0?10:(quality==1?14:(quality==2?20:28));
    mesh.Sphere(x,ground+0.045f,z,0.49f,0.035f,0.49f,XMFLOAT4{0.10f,0.11f,0.10f,0.28f},rings,slices);
    mesh.Sphere(x,ground+0.43f,z,0.38f,0.38f,0.38f,team,rings,slices);
    // A small team-coloured badge sits on the front of every sphere.
    mesh.BoxYaw(x,ground+0.43f,z+0.365f,0.19f,0.075f,0.035f,yaw,bright);

    if(unit.kind==1) { // Medical kit: white case with a clear red cross.
        const XMFLOAT4 white{0.92f,0.94f,0.91f,1.0f},red{0.84f,0.12f,0.12f,1.0f};
        mesh.BoxYaw(x+0.34f,ground+0.42f,z+0.02f,0.34f,0.25f,0.23f,yaw,white);
        mesh.BoxYaw(x+0.34f,ground+0.42f,z+0.145f,0.075f,0.19f,0.025f,yaw,red);
        mesh.BoxYaw(x+0.34f,ground+0.42f,z+0.145f,0.21f,0.065f,0.025f,yaw,red);
        mesh.BoxYaw(x+0.34f,ground+0.565f,z+0.02f,0.18f,0.045f,0.10f,yaw,dark);
        BuildSupportRing(mesh,x,ground+0.06f,z,unit.supportPulse,quality);
    } else if(unit.kind==2) { // Small, stylised catapult mounted beside the sphere.
        const XMFLOAT4 wood{0.39f,0.23f,0.12f,1.0f},rope{0.72f,0.61f,0.43f,1.0f};
        mesh.BoxYaw(x+0.36f,ground+0.25f,z-0.02f,0.48f,0.11f,0.35f,yaw,wood);
        mesh.BoxYaw(x+0.36f,ground+0.49f,z-0.02f,0.09f,0.43f,0.10f,yaw,wood);
        mesh.BoxYaw(x+0.36f,ground+0.69f,z+0.08f,0.08f,0.32f,0.09f,yaw,rope);
        mesh.Sphere(x+0.36f,ground+0.88f,z+0.20f,0.12f,0.12f,0.12f,ROCK_DARK,6,8);
        mesh.Cylinder(x+0.36f,ground+0.20f,z-0.02f,0.12f,0.055f,dark,quality==3?16:8);
    } else if(unit.kind==3) { // Modern class remains a sphere; only a compact equipment pack is added.
        mesh.BoxYaw(x,ground+0.43f,z-0.39f,0.28f,0.30f,0.13f,yaw,dark);
        mesh.BoxYaw(x+0.12f,ground+0.68f,z-0.39f,0.045f,0.23f,0.045f,yaw,steel);
        mesh.Sphere(x+0.12f,ground+0.81f,z-0.39f,0.055f,0.055f,0.055f,bright,6,8);
    } else { // Swordsman: a clearly visible, stylised fantasy sword accessory.
        mesh.BoxYaw(x+0.40f,ground+0.55f,z+0.03f,0.085f,0.54f,0.09f,yaw,steel);
        mesh.BoxYaw(x+0.40f,ground+0.27f,z+0.03f,0.12f,0.19f,0.12f,yaw,wood);
        mesh.BoxYaw(x+0.40f,ground+0.47f,z+0.03f,0.31f,0.065f,0.13f,yaw,XMFLOAT4{0.82f,0.68f,0.32f,1.0f});
        mesh.BoxYaw(x+0.40f,ground+0.83f,z+0.03f,0.13f,0.08f,0.13f,yaw,steel);
    }
}

class Renderer {
public:
    bool Initialize(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC swapDesc{};
        swapDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.SampleDesc.Count=1;
        swapDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount=2;
        swapDesc.OutputWindow=hwnd;
        swapDesc.Windowed=TRUE;
        swapDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL obtained{};
        HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,
            &swapDesc,&swapChain,&device,&obtained,&context);
        if(FAILED(hr)) {
            hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,
                &swapDesc,&swapChain,&device,&obtained,&context);
        }
        if(FAILED(hr)) return false;
        if(!CreateShaders() || !CreateBuffers() || !CreateStates()) return false;
        SelectSampleCount();
        RECT client{};GetClientRect(hwnd,&client);
        return Resize(static_cast<UINT>(client.right-client.left),static_cast<UINT>(client.bottom-client.top));
    }

    bool Resize(UINT width,UINT height) {
        if(!swapChain || !width || !height) return true;
        renderWidth=width;renderHeight=height;
        if(context) context->OMSetRenderTargets(0,nullptr,nullptr);
        renderTarget.Reset();sceneColorTexture.Reset();depthView.Reset();depthTexture.Reset();
        HRESULT hr=swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
        if(FAILED(hr)) return false;
        ComPtr<ID3D11Texture2D> backBuffer;
        hr=swapChain->GetBuffer(0,IID_PPV_ARGS(&backBuffer));if(FAILED(hr))return false;

        if(sampleCount==1) {
            hr=device->CreateRenderTargetView(backBuffer.Get(),nullptr,&renderTarget);
            if(FAILED(hr))return false;
        } else {
            D3D11_TEXTURE2D_DESC colorDesc{};
            colorDesc.Width=width;colorDesc.Height=height;colorDesc.MipLevels=1;colorDesc.ArraySize=1;
            colorDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
            colorDesc.SampleDesc.Count=sampleCount;colorDesc.SampleDesc.Quality=0;
            colorDesc.Usage=D3D11_USAGE_DEFAULT;colorDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
            hr=device->CreateTexture2D(&colorDesc,nullptr,&sceneColorTexture);if(FAILED(hr))return false;
            hr=device->CreateRenderTargetView(sceneColorTexture.Get(),nullptr,&renderTarget);if(FAILED(hr))return false;
        }

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width=width;depthDesc.Height=height;depthDesc.MipLevels=1;depthDesc.ArraySize=1;
        depthDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count=sampleCount;depthDesc.SampleDesc.Quality=0;
        depthDesc.Usage=D3D11_USAGE_DEFAULT;depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        hr=device->CreateTexture2D(&depthDesc,nullptr,&depthTexture);if(FAILED(hr))return false;
        hr=device->CreateDepthStencilView(depthTexture.Get(),nullptr,&depthView);if(FAILED(hr))return false;
        viewport={0.0f,0.0f,static_cast<float>(width),static_cast<float>(height),0.0f,1.0f};
        aspect=static_cast<float>(width)/static_cast<float>(height);
        return true;
    }

    bool SetQuality(int value) {
        const int next=std::clamp(value,0,3);
        if(next==quality)return true;
        quality=next;
        SelectSampleCount();
        if(!RebuildWorldBuffer())return false;
        return Resize(renderWidth,renderHeight);
    }

    int Quality() const { return quality; }
    UINT Samples() const { return sampleCount; }

    void Render(const UnitView* units,UINT unitCount,float time) {
        if(!context || !renderTarget || !depthView) return;
        const float white[4]={1.0f,1.0f,1.0f,1.0f};
        context->OMSetRenderTargets(1,renderTarget.GetAddressOf(),depthView.Get());
        context->ClearRenderTargetView(renderTarget.Get(),white);
        context->ClearDepthStencilView(depthView.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.0f,0);
        context->RSSetViewports(1,&viewport);
        context->RSSetState(rasterizer.Get());
        context->OMSetDepthStencilState(depthState.Get(),0);
        context->IASetInputLayout(inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader.Get(),nullptr,0);
        context->PSSetShader(pixelShader.Get(),nullptr,0);
        context->VSSetConstantBuffers(0,1,constantBuffer.GetAddressOf());

        XMVECTOR eye=XMVectorSet(0.0f,24.0f,-36.0f,1.0f);
        XMVECTOR focus=XMVectorSet(0.0f,0.0f,0.0f,1.0f);
        XMMATRIX view=XMMatrixLookAtLH(eye,focus,XMVectorSet(0.0f,1.0f,0.0f,0.0f));
        XMMATRIX projection=XMMatrixPerspectiveFovLH(XMConvertToRadians(43.0f),aspect,0.1f,150.0f);
        MatrixBuffer matrices{};
        XMStoreFloat4x4(&matrices.viewProjection,view*projection);
        matrices.lighting=XMFLOAT4{
            quality==0?0.035f:(quality==1?0.10f:(quality==2?0.18f:0.26f)),
            quality==0?12.0f:(quality==1?24.0f:(quality==2?40.0f:64.0f)),
            0.0f,0.0f
        };
        context->UpdateSubresource(constantBuffer.Get(),0,nullptr,&matrices,0,0);

        const UINT stride=sizeof(Vertex),offset=0;
        context->IASetVertexBuffers(0,1,staticVertexBuffer.GetAddressOf(),&stride,&offset);
        context->Draw(staticVertexCount,0);

        UpdateTroops(units,unitCount,time);
        if(dynamicVertexCount>0) {
            context->OMSetBlendState(alphaBlend.Get(),nullptr,0xffffffff);
            context->IASetVertexBuffers(0,1,dynamicVertexBuffer.GetAddressOf(),&stride,&offset);
            context->Draw(dynamicVertexCount,0);
            context->OMSetBlendState(nullptr,nullptr,0xffffffff);
        }
        if(sampleCount>1) {
            ComPtr<ID3D11Texture2D> backBuffer;
            if(SUCCEEDED(swapChain->GetBuffer(0,IID_PPV_ARGS(&backBuffer)))) {
                context->ResolveSubresource(backBuffer.Get(),0,sceneColorTexture.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
            }
        }
        swapChain->Present(1,0);
    }

private:
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;

    bool CreateShaders() {
        static const char* shader=R"(
cbuffer Scene : register(b0) {
    row_major float4x4 viewProjection;
    float4 lighting;
};
struct VSInput { float3 position : POSITION; float3 normal : NORMAL; float4 color : COLOR; };
struct PSInput { float4 position : SV_POSITION; float3 normal : NORMAL; float4 color : COLOR; };
PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(float4(input.position,1.0f),viewProjection);
    output.normal = input.normal;
    output.color = input.color;
    return output;
}
float4 PSMain(PSInput input) : SV_TARGET {
    float3 n = normalize(input.normal);
    float3 l = normalize(float3(-0.45f,1.0f,-0.35f));
    float3 v = normalize(float3(0.0f,0.72f,-1.0f));
    float3 h = normalize(l+v);
    float diffuse = saturate(abs(dot(n,l)));
    float specular = pow(saturate(abs(dot(n,h))),lighting.y)*lighting.x;
    float ambient = 0.26f + 0.16f*saturate(abs(n.y));
    float3 color = input.color.rgb*(ambient+0.72f*diffuse)+specular.xxx;
    return float4(saturate(color),input.color.a);
}
)";
        ComPtr<ID3DBlob> vsBlob,psBlob,errors;
        HRESULT hr=D3DCompile(shader,std::strlen(shader),"Conflict3D",nullptr,nullptr,"VSMain","vs_5_0",
                              D3DCOMPILE_ENABLE_STRICTNESS,0,&vsBlob,&errors);
        if(FAILED(hr)) return false;
        hr=D3DCompile(shader,std::strlen(shader),"Conflict3D",nullptr,nullptr,"PSMain","ps_5_0",
                      D3DCOMPILE_ENABLE_STRICTNESS,0,&psBlob,&errors);
        if(FAILED(hr)) return false;
        hr=device->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),nullptr,&vertexShader);
        if(FAILED(hr))return false;
        hr=device->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),nullptr,&pixelShader);
        if(FAILED(hr))return false;
        const D3D11_INPUT_ELEMENT_DESC layout[]={
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}
        };
        hr=device->CreateInputLayout(layout,3,vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),&inputLayout);
        return SUCCEEDED(hr);
    }

    bool RebuildWorldBuffer() {
        MeshBuilder world;
        BuildWorld(world,quality);
        if(world.vertices.empty())return false;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=static_cast<UINT>(world.vertices.size()*sizeof(Vertex));
        desc.Usage=D3D11_USAGE_IMMUTABLE;
        desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=world.vertices.data();
        ComPtr<ID3D11Buffer> next;
        HRESULT hr=device->CreateBuffer(&desc,&data,&next);
        if(FAILED(hr))return false;
        staticVertexBuffer=std::move(next);
        staticVertexCount=static_cast<UINT>(world.vertices.size());
        return true;
    }

    void SelectSampleCount() {
        const UINT wanted=quality==0?1u:(quality==1?2u:(quality==2?4u:8u));
        sampleCount=1;
        for(UINT candidate=wanted;candidate>=2;candidate/=2) {
            UINT colorLevels=0,depthLevels=0;
            const HRESULT color=device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,candidate,&colorLevels);
            const HRESULT depth=device->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT,candidate,&depthLevels);
            if(SUCCEEDED(color)&&SUCCEEDED(depth)&&colorLevels>0&&depthLevels>0) {
                sampleCount=candidate;
                break;
            }
        }
    }

    bool CreateBuffers() {
        if(!RebuildWorldBuffer())return false;
        D3D11_BUFFER_DESC dynamicDesc{};
        dynamicDesc.ByteWidth=MAX_DYNAMIC_VERTICES*sizeof(Vertex);
        dynamicDesc.Usage=D3D11_USAGE_DYNAMIC;
        dynamicDesc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        dynamicDesc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        HRESULT hr=device->CreateBuffer(&dynamicDesc,nullptr,&dynamicVertexBuffer);if(FAILED(hr))return false;

        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth=sizeof(MatrixBuffer);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        hr=device->CreateBuffer(&cb,nullptr,&constantBuffer);
        return SUCCEEDED(hr);
    }

    bool CreateStates() {
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode=D3D11_FILL_SOLID;
        raster.CullMode=D3D11_CULL_NONE;
        raster.DepthClipEnable=TRUE;
        raster.MultisampleEnable=TRUE;
        raster.AntialiasedLineEnable=TRUE;
        HRESULT hr=device->CreateRasterizerState(&raster,&rasterizer);if(FAILED(hr))return false;
        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        hr=device->CreateDepthStencilState(&depth,&depthState);if(FAILED(hr))return false;

        D3D11_BLEND_DESC blend{};
        auto& rt=blend.RenderTarget[0];
        rt.BlendEnable=TRUE;
        rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;
        rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp=D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha=D3D11_BLEND_ONE;
        rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        return SUCCEEDED(device->CreateBlendState(&blend,&alphaBlend));
    }

    void UpdateTroops(const UnitView* units,UINT count,float time) {
        dynamicVertexCount=0;
        if(!units || !count)return;
        MeshBuilder models;
        const UINT limit=std::min(count,MAX_UNITS);
        for(UINT i=0;i<limit;++i) {
            if(!units[i].alive)continue;
            BuildSphereUnit(models,units[i],quality,time);
        }
        if(models.vertices.empty())return;
        dynamicVertexCount=static_cast<UINT>(std::min(models.vertices.size(),static_cast<std::size_t>(MAX_DYNAMIC_VERTICES)));
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(dynamicVertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) {
            dynamicVertexCount=0;
            return;
        }
        std::memcpy(mapped.pData,models.vertices.data(),dynamicVertexCount*sizeof(Vertex));
        context->Unmap(dynamicVertexBuffer.Get(),0);
    }

    ComPtr<ID3D11RenderTargetView> renderTarget;
    ComPtr<ID3D11Texture2D> sceneColorTexture;
    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> depthView;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> staticVertexBuffer,dynamicVertexBuffer,constantBuffer;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11DepthStencilState> depthState;
    ComPtr<ID3D11BlendState> alphaBlend;
    static constexpr UINT MAX_DYNAMIC_VERTICES=MAX_UNITS*1024;
    UINT staticVertexCount=0,dynamicVertexCount=0;
    UINT sampleCount=1,renderWidth=0,renderHeight=0;
    int quality=1;
    float aspect=16.0f/9.0f;
    D3D11_VIEWPORT viewport{};
};

using UpdateFn=void(__cdecl*)(float);
using GetUnitsFn=std::size_t(__cdecl*)(UnitView*,std::size_t);
using RoundStateFn=int(__cdecl*)();
HMODULE rustModule{};
UpdateFn rustUpdate{};
GetUnitsFn rustGetUnits{};
RoundStateFn rustRoundState{};
UnitView unitBuffer[MAX_UNITS]{};
Renderer* renderer=nullptr;
LONG_PTR savedStyle{};
RECT savedRect{};
bool borderless=false;

void LoadSimulation() {
    wchar_t path[MAX_PATH]{};
    DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!n||n>=MAX_PATH)return;
    std::wstring dll(path);
    const auto slash=dll.find_last_of(L"\\/");
    if(slash!=std::wstring::npos)dll.resize(slash+1);
    dll+=L"conflict_core.dll";
    rustModule=LoadLibraryW(dll.c_str());
    if(!rustModule)return;
    rustUpdate=reinterpret_cast<UpdateFn>(GetProcAddress(rustModule,"conflict_update"));
    rustGetUnits=reinterpret_cast<GetUnitsFn>(GetProcAddress(rustModule,"conflict_get_units"));
    rustRoundState=reinterpret_cast<RoundStateFn>(GetProcAddress(rustModule,"conflict_round_state"));
    if(!rustUpdate||!rustGetUnits||!rustRoundState) {
        FreeLibrary(rustModule);rustModule=nullptr;
        rustUpdate=nullptr;rustGetUnits=nullptr;rustRoundState=nullptr;
    }
}

void ToggleBorderless(HWND hwnd) {
    if(!borderless) {
        savedStyle=GetWindowLongPtrW(hwnd,GWL_STYLE);
        GetWindowRect(hwnd,&savedRect);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info);
        SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);
        SetWindowPos(hwnd,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,
            info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,
            SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
        borderless=true;
    } else {
        SetWindowLongPtrW(hwnd,GWL_STYLE,savedStyle);
        SetWindowPos(hwnd,nullptr,savedRect.left,savedRect.top,savedRect.right-savedRect.left,
            savedRect.bottom-savedRect.top,SWP_FRAMECHANGED|SWP_NOZORDER|SWP_NOOWNERZORDER);
        borderless=false;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_SIZE:
        if(renderer) renderer->Resize(LOWORD(lParam),HIWORD(lParam));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;
    }
    case WM_KEYDOWN:
        if(wParam==VK_F11)ToggleBorderless(hwnd);
        else if(wParam==VK_ESCAPE&&borderless)ToggleBorderless(hwnd);
        else if(wParam==VK_F1&&renderer) {
            if(renderer->SetQuality(0))SetWindowTextW(hwnd,L"CONFLICT | GRAFICOS: BAJOS (F1/F2/F3)");
        } else if(wParam==VK_F2&&renderer) {
            if(renderer->SetQuality(1))SetWindowTextW(hwnd,L"CONFLICT | GRAFICOS: MEDIOS (F1/F2/F3)");
        } else if(wParam==VK_F3&&renderer) {
            if(renderer->SetQuality(2))SetWindowTextW(hwnd,L"CONFLICT | GRAFICOS: ALTOS (F1/F2/F3)");
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x=960;info->ptMinTrackSize.y=640;return 0;
    }
    case WM_DESTROY:
        if(rustModule){FreeLibrary(rustModule);rustModule=nullptr;}
        PostQuitMessage(0);return 0;
    default:
        return DefWindowProcW(hwnd,message,wParam,lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=instance;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;wc.lpszClassName=WINDOW_CLASS;
    if(!RegisterClassExW(&wc))return 1;

    RECT bounds{0,0,1440,900};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,WINDOW_CLASS,L"CONFLICT | GRAFICOS: MEDIOS (F1/F2/F3)",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,
        nullptr,nullptr,instance,nullptr);
    if(!hwnd)return 1;

    Renderer graphics;
    if(!graphics.Initialize(hwnd)) {
        MessageBoxW(hwnd,L"No se pudo inicializar Direct3D 11. Actualiza los controladores gráficos.",
            L"CONFLICT",MB_OK|MB_ICONERROR);
        DestroyWindow(hwnd);return 1;
    }
    renderer=&graphics;
    LoadSimulation();
    if(!rustModule) {
        MessageBoxW(hwnd,L"No se pudo cargar el motor Rust. Comprueba que conflict_core.dll esté junto a Conflict.exe.",
            L"CONFLICT",MB_OK|MB_ICONERROR);
        DestroyWindow(hwnd);return 1;
    }

    ShowWindow(hwnd,show);UpdateWindow(hwnd);
    auto previous=std::chrono::steady_clock::now();
    MSG msg{};
    bool running=true;
    while(running) {
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT){running=false;break;}
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(!running)break;

        const auto now=std::chrono::steady_clock::now();
        float dt=std::chrono::duration<float>(now-previous).count();
        previous=now;
        dt=std::clamp(dt,0.0f,0.05f);

        rustUpdate(dt);
        const std::size_t count=rustGetUnits(unitBuffer,MAX_UNITS);
        const float elapsed=std::chrono::duration<float>(now.time_since_epoch()).count();
        graphics.Render(unitBuffer,static_cast<UINT>(std::min(count,static_cast<std::size_t>(MAX_UNITS))),elapsed);
        Sleep(1);
    }
    renderer=nullptr;
    return static_cast<int>(msg.wParam);
}