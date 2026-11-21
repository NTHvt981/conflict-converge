#include "test_harness.h"

#include "app/ui/RmlUiHost.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/RenderInterface.h>

#include <filesystem>
#include <string>

namespace
{

class HeadlessRenderInterface : public Rml::RenderInterface
{
public:
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
												Rml::Span<const int> indices) override
	{
		return ++nextHandle_;
	}

	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
						Rml::TextureHandle texture) override
	{
	}

	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {}

	Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source) override
	{
		return 0;
	}

	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
									   Rml::Vector2i source_dimensions) override
	{
		return ++nextHandle_;
	}

	void ReleaseTexture(Rml::TextureHandle texture) override {}

	void EnableScissorRegion(bool enable) override {}

	void SetScissorRegion(Rml::Rectanglei region) override {}

private:
	Rml::CompiledGeometryHandle nextHandle_ = 0;
};

std::string FindDataRoot()
{
	const std::string candidates[] = { "data", "../data", "../../data" };
	for (const std::string &root : candidates)
	{
		if (std::filesystem::exists(root + "/ui/hud.rml"))
		{
			return root;
		}
	}
	return {};
}

} // namespace

void RunUiTests()
{
	const std::string root = FindDataRoot();
	if (root.empty())
	{
		std::printf("Ui: data root not found (cwd must be the repo root or prj/)\n");
		CC_CHECK(false);
		return;
	}

	HeadlessRenderInterface render;
	RmlUiHost host;
	CC_CHECK(host.Init(root + "/ui", root + "/fonts", &render, 1280, 720));
	CC_CHECK(host.IsReady());
	CC_CHECK(host.context() != nullptr);
	if (host.context() != nullptr)
	{
		Rml::ElementDocument *doc = host.context()->LoadDocument(root + "/ui/hud.rml");
		CC_CHECK(doc != nullptr);
		if (doc != nullptr)
		{
			CC_CHECK(doc->GetElementById("res-line") != nullptr);
		}
	}

	host.Shutdown();
	CC_CHECK(!host.IsReady());
}
