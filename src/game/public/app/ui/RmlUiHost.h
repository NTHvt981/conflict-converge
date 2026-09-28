#pragma once

#include <memory>
#include <string>

namespace Rml {
class Context;
}

class RmlRaylibFileInterface;
class RmlRaylibRenderInterface;
class RmlRaylibSystemInterface;

// RmlUi overlay host. Owns the RmlUi context and the raylib backend
// interfaces. Every method no-ops until Init succeeds, so headless test
// binaries can construct (never Init) safely.
class RmlUiHost
{
public:
    RmlUiHost();
    RmlUiHost(const RmlUiHost &) = delete;
    RmlUiHost &operator=(const RmlUiHost &) = delete;
    ~RmlUiHost();

    // Creates the context and loads the fonts. Returns false with no window
    // (headless tests) or on load failure; call once.
    bool Init(const std::string &dataDir, const std::string &fontsDir);
    void Shutdown();
    bool IsReady() const { return ready_; }
    Rml::Context *context() { return context_; }

    // Syncs dimensions/dp-ratio (auto-fit of the 1280x720 base times the
    // uiScale setting, clamped) and ticks the context. Render draws it
    // screen-space: call inside BeginDrawing, after the world.
    void BeginFrame(int width, int height, float uiScale);
    void Render();

    // Headless-testable dp-ratio math behind BeginFrame.
    static float AutoDpRatio(int width, int height, float uiScale);

private:
    std::unique_ptr<RmlRaylibRenderInterface> render_;
    std::unique_ptr<RmlRaylibSystemInterface> system_;
    std::unique_ptr<RmlRaylibFileInterface> files_;
    Rml::Context *context_ = nullptr;
    std::string dataDir_;
    bool ready_ = false;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
};
