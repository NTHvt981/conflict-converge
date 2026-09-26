#include "app/RmlRaylibRenderInterface.h"

#include <rlgl.h>

Rml::CompiledGeometryHandle RmlRaylibRenderInterface::CompileGeometry(
	Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
	auto* geometry = new RmlRaylibCompiledGeometry();
	geometry->vertices.assign(vertices.data(), vertices.data() + vertices.size());
	geometry->indices.assign(indices.data(), indices.data() + indices.size());
	return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void RmlRaylibRenderInterface::RenderGeometry(
	Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	rlDrawRenderBatchActive();

	rlDisableDepthTest();
	rlDisableBackfaceCulling();
	rlEnableColorBlend();
	rlSetBlendMode(RL_BLEND_ALPHA_PREMULTIPLY);

	rlSetTexture(static_cast<unsigned int>(texture));

	auto* compiled = reinterpret_cast<RmlRaylibCompiledGeometry*>(geometry);

	rlBegin(RL_TRIANGLES);
	for (int index : compiled->indices)
	{
		const Rml::Vertex& v = compiled->vertices[static_cast<size_t>(index)];
		rlColor4ub(v.colour.red, v.colour.green, v.colour.blue, v.colour.alpha);
		rlTexCoord2f(v.tex_coord.x, v.tex_coord.y);
		rlVertex2f(v.position.x + translation.x, v.position.y + translation.y);
	}
	rlEnd();

	// Flush immediately so this geometry draws under the GL state (scissor,
	// texture, blend) active right now. Without this, the batched vertices
	// would only draw at the next flush, potentially after RmlUi changed or
	// disabled that state (e.g. overflow: hidden regions drew unclipped).
	rlDrawRenderBatchActive();
}

void RmlRaylibRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
	delete reinterpret_cast<RmlRaylibCompiledGeometry*>(geometry);
}

Rml::TextureHandle RmlRaylibRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
	Image image = LoadImage(source.c_str());
	if (image.data == nullptr)
		return 0;

	ImageAlphaPremultiply(&image);
	Texture2D texture = LoadTextureFromImage(image);
	UnloadImage(image);

	if (texture.id == 0)
		return 0;

	// FreeType glyph textures stay bilinear at any uiScale.
	SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

	texture_dimensions.x = texture.width;
	texture_dimensions.y = texture.height;

	Rml::TextureHandle handle = static_cast<Rml::TextureHandle>(texture.id);
	textures[handle] = texture;
	return handle;
}

Rml::TextureHandle RmlRaylibRenderInterface::GenerateTexture(
	Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
	unsigned int id =
		rlLoadTexture(source.data(), source_dimensions.x, source_dimensions.y, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
	if (id == 0)
		return 0;

	Texture2D texture = {};
	texture.id = id;
	texture.width = source_dimensions.x;
	texture.height = source_dimensions.y;
	texture.mipmaps = 1;
	texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

	return static_cast<Rml::TextureHandle>(id);
}

void RmlRaylibRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
	auto it = textures.find(texture);
	if (it != textures.end())
	{
		UnloadTexture(it->second);
		textures.erase(it);
	}
	else
	{
		rlUnloadTexture(static_cast<unsigned int>(texture));
	}
}

void RmlRaylibRenderInterface::EnableScissorRegion(bool enable)
{
	if (enable)
		rlEnableScissorTest();
	else
		rlDisableScissorTest();
}

void RmlRaylibRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
	// rlScissor expects framebuffer pixels. Scale the RmlUi device-pixel
	// region by render/logical ratio so clipping stays correct with
	// FLAG_WINDOW_HIGHDPI (ratio > 1) and unchanged without it (ratio = 1).
	const int screenW = GetScreenWidth();
	const int screenH = GetScreenHeight();
	const int renderW = GetRenderWidth();
	const int renderH = GetRenderHeight();
	const float scaleX = (screenW > 0 && renderW > 0) ? static_cast<float>(renderW) / static_cast<float>(screenW) : 1.0f;
	const float scaleY = (screenH > 0 && renderH > 0) ? static_cast<float>(renderH) / static_cast<float>(screenH) : 1.0f;
	const int x = static_cast<int>(static_cast<float>(region.Left()) * scaleX + 0.5f);
	const int wIn = region.Width() > 0 ? region.Width() : 0;
	const int hIn = region.Height() > 0 ? region.Height() : 0;
	const int w = static_cast<int>(static_cast<float>(wIn) * scaleX + 0.5f);
	const int h = static_cast<int>(static_cast<float>(hIn) * scaleY + 0.5f);
	const int top = static_cast<int>(static_cast<float>(region.Top()) * scaleY + 0.5f);
	const int y = renderH - top - h;
	rlScissor(x, y, w, h);
}
