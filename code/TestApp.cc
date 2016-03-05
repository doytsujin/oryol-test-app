//------------------------------------------------------------------------------
//  TestApp.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/Main.h"
#include "Gfx/Gfx.h"
#include "Assets/Gfx/ShapeBuilder.h"
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

    Id renderTarget;
    MeshBlock torusMeshBlock;
    MeshBlock sphereMeshBlock;
    Id offscreenDrawState;
    Id displayDrawState;
    ClearState offscreenClearState;
    ClearState displayClearState;
    glm::mat4 view;
    glm::mat4 offscreenProj;
    glm::mat4 displayProj;
    float32 angleX = 0.0f;
    float32 angleY = 0.0f;
    Shaders::RenderTarget::VSParams offscreenParams;
    Shaders::Main::VSParams displayVSParams;
    Shaders::Main::FSTextures displayFSTextures;
};
OryolMain(TestApp);

//------------------------------------------------------------------------------
AppState::Code
TestApp::OnRunning() {
    
    // update animated parameters
    this->angleY += 0.01f;
    this->angleX += 0.02f;
    this->offscreenParams.ModelViewProjection = this->computeMVP(this->offscreenProj, this->angleX, this->angleY, glm::vec3(0.0f, 0.0f, -3.0f));
    this->displayVSParams.ModelViewProjection = this->computeMVP(this->displayProj, -this->angleX * 0.25f, this->angleY * 0.25f, glm::vec3(0.0f, 0.0f, -1.5f));;

    // render donut to offscreen render target
    Gfx::ApplyRenderTarget(this->renderTarget, this->offscreenClearState);
    Gfx::ApplyDrawState(this->offscreenDrawState, this->torusMeshBlock);
    Gfx::ApplyUniformBlock(this->offscreenParams);
    Gfx::Draw(0);
    
    // render sphere to display, with offscreen render target as texture
    Gfx::ApplyDefaultRenderTarget(this->displayClearState);
    Gfx::ApplyDrawState(this->displayDrawState, this->sphereMeshBlock, this->displayFSTextures);
    Gfx::ApplyUniformBlock(this->displayVSParams);
    Gfx::Draw(0);
    
    Gfx::CommitFrame();
    
    // continue running or quit?
    return Gfx::QuitRequested() ? AppState::Cleanup : AppState::Running;
}

//------------------------------------------------------------------------------
AppState::Code
TestApp::OnInit() {
    // setup rendering system
    auto gfxSetup = GfxSetup::WindowMSAA4(800, 600, "Oryol Test App");
    Gfx::Setup(gfxSetup);

    // create an offscreen render target, we explicitly want repeat texture wrap mode
    // and linear blending...
    auto rtSetup = TextureSetup::RenderTarget(128, 128);
    rtSetup.ColorFormat = PixelFormat::RGBA8;
    rtSetup.DepthFormat = PixelFormat::DEPTH;
    rtSetup.Sampler.WrapU = TextureWrapMode::Repeat;
    rtSetup.Sampler.WrapV = TextureWrapMode::Repeat;
    rtSetup.Sampler.MagFilter = TextureFilterMode::Linear;
    rtSetup.Sampler.MinFilter = TextureFilterMode::Linear;
    this->renderTarget = Gfx::CreateResource(rtSetup);
    
    // create offscreen rendering resources
    ShapeBuilder shapeBuilder;
    shapeBuilder.Layout
        .Add(VertexAttr::Position, VertexFormat::Float3)
        .Add(VertexAttr::Normal, VertexFormat::Byte4N);
    shapeBuilder.Box(1.0f, 1.0f, 1.0f, 1);
    this->torusMeshBlock[0] = Gfx::CreateResource(shapeBuilder.Build());
    Id offScreenShader = Gfx::CreateResource(Shaders::RenderTarget::Setup());
    auto offdsSetup = DrawStateSetup::FromLayoutAndShader(shapeBuilder.Layout, offScreenShader);
    offdsSetup.DepthStencilState.DepthWriteEnabled = true;
    offdsSetup.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    offdsSetup.BlendState.ColorFormat = rtSetup.ColorFormat;
    offdsSetup.BlendState.DepthFormat = rtSetup.DepthFormat;    
    this->offscreenDrawState = Gfx::CreateResource(offdsSetup);

    // create display rendering resources
    shapeBuilder.Layout
        .Clear()
        .Add(VertexAttr::Position, VertexFormat::Float3)
        .Add(VertexAttr::Normal, VertexFormat::Byte4N)
        .Add(VertexAttr::TexCoord0, VertexFormat::Float2);
    shapeBuilder.Sphere(0.5f, 72.0f, 40.0f);
    this->sphereMeshBlock[0] = Gfx::CreateResource(shapeBuilder.Build());
    Id dispShader = Gfx::CreateResource(Shaders::Main::Setup());
    auto dispdsSetup = DrawStateSetup::FromLayoutAndShader(shapeBuilder.Layout, dispShader);
    dispdsSetup.DepthStencilState.DepthWriteEnabled = true;
    dispdsSetup.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    dispdsSetup.RasterizerState.SampleCount = gfxSetup.SampleCount;
    this->displayDrawState = Gfx::CreateResource(dispdsSetup);
    this->displayFSTextures.Texture = this->renderTarget;

    // setup clear states
    this->offscreenClearState.Color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);
    this->displayClearState.Color = glm::vec4(0.25f, 0.5f, 1.0f, 1.0f);

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
