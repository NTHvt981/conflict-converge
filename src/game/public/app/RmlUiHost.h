#pragma once

#include <memory>
#include <string>

namespace Rml {
class Context;
class ElementDocument;
}

class FontEngineInterfaceBitmap;
class RmlRaylibFileInterface;
class RmlRaylibRenderInterface;
class RmlRaylibSystemInterface;

// RmlUi overlay host (Phase 1: render-only proof). Owns the RmlUi context and
// the raylib backend interfaces. Every method no-ops until Init succeeds, so
// headless test binaries can construct (never Init) safely.
class RmlUiHost
{
public:
    RmlUiHost();
    RmlUiHost(const RmlUiHost &) = delete;
    RmlUiHost &operator=(const RmlUiHost &) = delete;
    ~RmlUiHost();

    // Creates the context and shows dataDir/stub.rml. Returns false with no
    // window (headless tests) or on load failure; call once.
    bool Init(const std::string &dataDir);
    void Shutdown();
    bool IsReady() const { return ready_; }
    Rml::Context *context() { return context_; }
    // Hides the Phase 1 proof document (called by RmlUiMenus::Init once the
    // real documents take over; the stub is deleted in Phase 3).
    void HideStub();

    // Syncs dimensions/dp-ratio (manual uiScale-only policy, base 1.0) and
    // ticks the context. Render draws it screen-space: call inside
    // BeginDrawing, after the world.
    void BeginFrame(int width, int height, float uiScale);
    void Render();

private:
    std::unique_ptr<RmlRaylibRenderInterface> render_;
    std::unique_ptr<RmlRaylibSystemInterface> system_;
    std::unique_ptr<RmlRaylibFileInterface> files_;
    std::unique_ptr<FontEngineInterfaceBitmap> fonts_;
    Rml::Context *context_ = nullptr;
    Rml::ElementDocument *stub_ = nullptr;
    std::string dataDir_;
    bool ready_ = false;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
};
