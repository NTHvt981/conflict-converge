#include "app/ui/RmlUiHost.h"

// raylib.h before rlgl.h: rlgl defines its own Matrix only when
// RL_MATRIX_TYPE is absent (raylib.h sets it), so the reverse order
// redefines Matrix (MSVC C2011). Same order as the probe's main.cpp.
#include <raylib.h>
#include <rlgl.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Debugger.h>

#include "app/ui/RmlRaylibFileInterface.h"
#include "app/ui/RmlRaylibRenderInterface.h"
#include "app/ui/RmlRaylibSystemInterface.h"

RmlUiHost::RmlUiHost() = default;

RmlUiHost::~RmlUiHost()
{
    Shutdown();
}

bool RmlUiHost::Init(const std::string &dataDir, const std::string &fontsDir)
{
    if (ready_ || !IsWindowReady())
    {
        return false;
    }
    render_ = std::make_unique<RmlRaylibRenderInterface>();
    system_ = std::make_unique<RmlRaylibSystemInterface>();
    files_ = std::make_unique<RmlRaylibFileInterface>();
    Rml::SetSystemInterface(system_.get());
    Rml::SetFileInterface(files_.get());
    Rml::SetRenderInterface(render_.get());
    if (!Rml::Initialise())
    {
        return false;
    }
    dataDir_ = dataDir;
    context_ =
        Rml::CreateContext("main", Rml::Vector2i(GetScreenWidth(), GetScreenHeight()));
    if (context_ == nullptr)
    {
        Rml::Shutdown();
        return false;
    }
    // Manual uiScale-only policy (base 1.0): raylib upscales the framebuffer
    // itself, so OS DPI stays out of the dp-ratio. Phase 2 binds uiScale to
    // the settings value every frame via BeginFrame.
    context_->SetDensityIndependentPixelRatio(1.0f);
    if (!Rml::Debugger::Initialise(context_))
    {
        Rml::Shutdown();
        context_ = nullptr;
        return false;
    }
    Rml::Debugger::SetVisible(false);
    if (!Rml::LoadFontFace(fontsDir + "/OpenSansPX.ttf", "opensanspx", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal) ||
        !Rml::LoadFontFace(fontsDir + "/OpenSansPXBold.ttf", "opensanspx", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold))
    {
        Rml::Shutdown();
        context_ = nullptr;
        return false;
    }
    ready_ = true;
    return true;
}

void RmlUiHost::Shutdown()
{
    if (!ready_)
    {
        return;
    }
    ready_ = false;
    if (context_ != nullptr)
    {
        context_->Update();
        context_ = nullptr;
    }
    Rml::Shutdown();
}

void RmlUiHost::BeginFrame(int width, int height, float uiScale)
{
    if (!ready_ || context_ == nullptr)
    {
        return;
    }
    if (width != lastWidth_ || height != lastHeight_)
    {
        lastWidth_ = width;
        lastHeight_ = height;
        context_->SetDimensions(Rml::Vector2i(width, height));
    }
    context_->SetDensityIndependentPixelRatio(uiScale);
    context_->Update();
}

void RmlUiHost::Render()
{
    if (!ready_ || context_ == nullptr)
    {
        return;
    }
    // Same flush discipline as the probe: draw each geometry batch under the
    // GL state (scissor/texture/blend) active right now, then restore alpha
    // blending for the raylib/raygui draws that follow.
    rlDrawRenderBatchActive();
    context_->Render();
    rlDrawRenderBatchActive();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
