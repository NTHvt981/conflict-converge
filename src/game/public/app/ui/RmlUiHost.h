#pragma once

#include <memory>
#include <string>

namespace Rml {
class Context;
class RenderInterface;
}

class RmlRaylibFileInterface;
class RmlRaylibRenderInterface;
class RmlRaylibSystemInterface;

// Owns the RmlUi context and raylib backends. Every method no-ops until Init
// succeeds, so headless tests can construct without a window.
class RmlUiHost
{
public:
	RmlUiHost();
	RmlUiHost(const RmlUiHost &) = delete;
	RmlUiHost &operator=(const RmlUiHost &) = delete;
	~RmlUiHost();

	// A non-null headlessRender selects the no-window path; contextWidth/
	// contextHeight size the context. Default arguments keep the windowed path.
	bool Init(const std::string &dataDir, const std::string &fontsDir, Rml::RenderInterface *headlessRender = nullptr, int contextWidth = 0, int contextHeight = 0);
	void Shutdown();
	bool IsReady() const { return ready_; }
	Rml::Context *context() { return context_; }

	// dp-ratio = auto-fit of the 1280x720 base * uiScale, clamped; ticks the context.
	// Render draws screen-space: call inside BeginDrawing, after the world.
	void BeginFrame(int width, int height, float uiScale);
	void Render();
	void ToggleDebugger();
	double LastUpdateMs() const { return updateMs_; }
	double LastRenderMs() const { return renderMs_; }
	int LastTriangles() const;

	static float AutoDpRatio(int width, int height, float uiScale);

private:
	std::unique_ptr<RmlRaylibRenderInterface> render_;
	Rml::RenderInterface *headlessRender_ = nullptr;
	std::unique_ptr<RmlRaylibSystemInterface> system_;
	std::unique_ptr<RmlRaylibFileInterface> files_;
	Rml::Context *context_ = nullptr;
	std::string dataDir_;
	bool ready_ = false;
	int lastWidth_ = 0;
	int lastHeight_ = 0;
	double updateMs_ = 0.0;
	double renderMs_ = 0.0;
};
