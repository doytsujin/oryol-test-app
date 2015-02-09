//------------------------------------------------------------------------------
//  TestApp.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/App.h"
#include "Gfx/Gfx.h"
#include "Asset/Util/ShapeBuilder.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "shaders.h"

using namespace Oryol;

// derived application class
class TestApp : public App {
public:
    AppState::Code OnRunning();
    AppState::Code OnInit();
    AppState::Code OnCleanup();    
private:
    glm::mat4 computeMVP(const glm::mat4& proj, float32 rotX, float32 rotY, const glm::vec3& pos);

    GfxId renderTarget;
    GfxId offscreenDrawState;
    GfxId displayDrawState;
    glm::mat4 view;
    glm::mat4 offscreenProj;
    glm::mat4 displayProj;
    float32 angleX = 0.0f;
    float32 angleY = 0.0f;
};
OryolMain(TestApp);

//------------------------------------------------------------------------------
AppState::Code
TestApp::OnRunning() {
    
    // update angles
    this->angleY += 0.01f;
    this->angleX += 0.02f;
    
    // render donut to offscreen render target
    Gfx::ApplyOffscreenRenderTarget(this->renderTarget);
    Gfx::Clear(PixelChannel::All, glm::vec4(0.25f));
    Gfx::ApplyDrawState(this->offscreenDrawState);
    glm::mat4 donutMVP = this->computeMVP(this->offscreenProj, this->angleX, this->angleY, glm::vec3(0.0f, 0.0f, -3.0f));
    Gfx::ApplyVariable(Shaders::RenderTarget::ModelViewProjection, donutMVP);
    Gfx::Draw(0);
    
    // render sphere to display, with offscreen render target as texture
    Gfx::ApplyDefaultRenderTarget();
    Gfx::Clear(PixelChannel::All, glm::vec4(0.25f), 1.0f, 0);
    Gfx::ApplyDrawState(this->displayDrawState);
    glm::mat4 sphereMVP = this->computeMVP(this->displayProj, -this->angleX * 0.25f, this->angleY * 0.25f, glm::vec3(0.0f, 0.0f, -1.5f));
    Gfx::ApplyVariable(Shaders::Main::ModelViewProjection, sphereMVP);
    Gfx::ApplyVariable(Shaders::Main::Texture, this->renderTarget);
    Gfx::Draw(0);
    
    Gfx::CommitFrame();
    
    // continue running or quit?
    return Gfx::QuitRequested() ? AppState::Cleanup : AppState::Running;
}

//------------------------------------------------------------------------------
AppState::Code
TestApp::OnInit() {
    // setup rendering system
    Gfx::Setup(GfxSetup::WindowMSAA4(800, 600, "Oryol Test App"));

    // create an offscreen render target, we explicitly want repeat texture wrap mode
    // and linear blending...
    auto rtSetup = TextureSetup::RenderTarget(128, 128);
    rtSetup.ColorFormat = PixelFormat::RGB8;
    rtSetup.DepthFormat = PixelFormat::D16;
    rtSetup.WrapU = TextureWrapMode::Repeat;
    rtSetup.WrapV = TextureWrapMode::Repeat;
    rtSetup.MagFilter = TextureFilterMode::Linear;
    rtSetup.MinFilter = TextureFilterMode::Linear;
    this->renderTarget = Gfx::CreateResource(rtSetup);
    
    // create a donut (this will be rendered into the offscreen render target)
    ShapeBuilder shapeBuilder;
    shapeBuilder.Layout
        .Add(VertexAttr::Position, VertexFormat::Float3)
        .Add(VertexAttr::Normal, VertexFormat::Byte4N);
    shapeBuilder.Box(1.0f, 1.0f, 1.0f, 1).Build();
    GfxId torus = Gfx::CreateResource(shapeBuilder.GetMeshSetup(), shapeBuilder.GetStream());
    
    // create a sphere mesh with normals and uv coords
    shapeBuilder.Clear();
    shapeBuilder.Layout
        .Add(VertexAttr::Position, VertexFormat::Float3)
        .Add(VertexAttr::Normal, VertexFormat::Byte4N)
        .Add(VertexAttr::TexCoord0, VertexFormat::Float2);
    shapeBuilder.Sphere(0.5f, 72.0f, 40.0f).Build();
    GfxId sphere = Gfx::CreateResource(shapeBuilder.GetMeshSetup(), shapeBuilder.GetStream());

    // create shaders
    GfxId offScreenProg = Gfx::CreateResource(Shaders::RenderTarget::CreateSetup());
    GfxId dispProg = Gfx::CreateResource(Shaders::Main::CreateSetup());
    
    // create one draw state for offscreen rendering, and one draw state for main target rendering
    auto offdsSetup = DrawStateSetup::FromMeshAndProg(torus, offScreenProg);
    offdsSetup.DepthStencilState.DepthWriteEnabled = true;
    offdsSetup.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    this->offscreenDrawState = Gfx::CreateResource(offdsSetup);
    auto dispdsSetup = DrawStateSetup::FromMeshAndProg(sphere, dispProg);
    dispdsSetup.DepthStencilState.DepthWriteEnabled = true;
    dispdsSetup.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    this->displayDrawState = Gfx::CreateResource(dispdsSetup);
    
    // setup static transform matrices
    float32 fbWidth = Gfx::DisplayAttrs().FramebufferWidth;
    float32 fbHeight = Gfx::DisplayAttrs().FramebufferHeight;
    this->offscreenProj = glm::perspective(glm::radians(45.0f), 1.0f, 0.01f, 20.0f);
    this->displayProj = glm::perspectiveFov(glm::radians(45.0f), fbWidth, fbHeight, 0.01f, 100.0f);
    this->view = glm::mat4();
    
    return App::OnInit();
}

//------------------------------------------------------------------------------
AppState::Code
TestApp::OnCleanup() {
    // cleanup everything
    this->offscreenDrawState.Release();
    this->displayDrawState.Release();
    this->renderTarget.Release();
    Gfx::Discard();
    return App::OnCleanup();
}

//------------------------------------------------------------------------------
glm::mat4
TestApp::computeMVP(const glm::mat4& proj, float32 rotX, float32 rotY, const glm::vec3& pos) {
    glm::mat4 modelTform = glm::translate(glm::mat4(), pos);
    modelTform = glm::rotate(modelTform, rotX, glm::vec3(1.0f, 0.0f, 0.0f));
    modelTform = glm::rotate(modelTform, rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    return proj * this->view * modelTform;
}
