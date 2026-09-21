#pragma once

#include "FontEngineInterfaceBitmap.h"
#include <RmlUi/Core/BaseXMLParser.h>
#include <RmlUi/Core/Texture.h>
#include <RmlUi/Core/Types.h>

class FontFaceBitmap;
using Rml::TextureSource;

namespace FontProviderBitmap {
void Initialise();
void Shutdown();
bool LoadFontFace(const String& file_name, const String& family = "");
FontFaceBitmap* GetFontFaceHandle(const String& family, FontStyle style, FontWeight weight, int size);
} // namespace FontProviderBitmap

struct BitmapGlyph {
	int advance = 0;
	Vector2f offset = {0, 0};
	Vector2f position = {0, 0};
	Vector2f dimension = {0, 0};
};

// A mapping of characters to their glyphs.
using FontGlyphs = Rml::UnorderedMap<Character, BitmapGlyph>;

// Mapping of combined (left, right) character to kerning in pixels.
using FontKerning = Rml::UnorderedMap<uint64_t, int>;

class FontFaceBitmap {
public:
	FontFaceBitmap(String family, FontStyle style, FontWeight weight, FontMetrics metrics, String texture_name, String texture_path,
		Vector2f texture_dimensions, FontGlyphs&& glyphs, FontKerning&& kerning);

	// Clone of another face, rendering at scale * native size. The glyph
	// source rects stay in native texture pixels (so UVs keep sampling the
	// same texels); advances, offsets, quad sizes and metrics are scaled.
	// Lets dp-sized text (font-size:Ndp with dp_ratio != 1) grow/shrink
	// together with dp-sized boxes instead of staying at 22px.
	// PROBE-ONLY: GPU-stretching a 22px bitmap looks blocky at non-integer
	// scales (1.25/1.5x) and soft/blurry if the atlas filter is bilinear.
	// Production needs an SDF/FreeType engine or pre-baked multi-size
	// atlases; do not carry these clones over (see RmlRaylibRenderInterface
	// font-atlas filter note).
	FontFaceBitmap(const FontFaceBitmap& other, float scale);

	// Get width of string.
	int GetStringWidth(StringView string, Character prior_character);

	// Generate the string geometry, returning its width.
	int GenerateString(RenderManager& render_manager, StringView string, Vector2f string_position, ColourbPremultiplied colour, TexturedMeshList& mesh_list);

	const FontMetrics& GetMetrics() const { return scaled_metrics; }

	const String& GetFamily() const { return family; }
	FontStyle GetStyle() const { return style; }
	FontWeight GetWeight() const { return weight; }
	float GetScale() const { return scale; }
	int GetNativeSize() const { return metrics.size; }

private:
	int GetKerning(Character left, Character right) const;

	String family;
	FontStyle style;
	FontWeight weight;

	FontMetrics metrics;
	FontMetrics scaled_metrics;
	float scale = 1.0f;

	TextureSource texture_source;
	Vector2f texture_dimensions;

	FontGlyphs glyphs;
	FontKerning kerning;
};

/*
    Parses the font meta data from an xml file.
*/

class FontParserBitmap : public Rml::BaseXMLParser {
public:
	FontParserBitmap() {}
	virtual ~FontParserBitmap();

	/// Called when the parser finds the beginning of an element tag.
	void HandleElementStart(const String& name, const Rml::XMLAttributes& attributes) override;
	/// Called when the parser finds the end of an element tag.
	void HandleElementEnd(const String& name) override;
	/// Called when the parser encounters data.
	void HandleData(const String& data, Rml::XMLDataType type) override;

	String family;
	FontStyle style = FontStyle::Normal;
	FontWeight weight = FontWeight::Normal;

	String texture_name;
	Vector2f texture_dimensions = {0, 0};

	FontMetrics metrics = {};
	FontGlyphs glyphs;
	FontKerning kerning;
};
