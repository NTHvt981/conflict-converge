#include "RmlRaylibRenderInterface.h"

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
	int fbH = GetScreenHeight();
	int x = region.Left();
	int y = fbH - region.Top() - region.Height();
	int w = region.Width() > 0 ? region.Width() : 0;
	int h = region.Height() > 0 ? region.Height() : 0;
	rlScissor(x, y, w, h);
}
