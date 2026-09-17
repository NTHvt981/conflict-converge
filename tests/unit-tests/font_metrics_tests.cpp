// Headless font-metric tests over data/font_metrics.txt (baked by
// tools/bake_font_metrics.py from raylib's static charsWidth table).
//
// Why this works without a GPU: measuring never touches GL -- raygui's
// GetLineWidth only reads glyph advance/rec widths, baseSize, and the
// TEXT_SPACING style. The default font sets advanceX=0 for every glyph,
// so the width of a line is entirely determined by the baked table:
//   x += recWidth[cp] * (fontSize / baseSize) + textSpacing, truncated.
// HeadlessLineWidth below replicates that loop op-for-op (ASCII/Latin-1
// single bytes only -- every UI string in the game qualifies); the
// goldens in the data file were proven float32/float64-stable at bake
// time, so a match here proves the replication, and re-running the bake
// script after a raylib update proves the table.

#include "test_harness.h"

#include <fstream>
#include <string>
#include <vector>

namespace
{

struct BakedFont
{
    int baseSize = 0;
    int firstChar = 0;
    int textSpacing = 0;
    std::vector<int> advances; // rec widths, index = codepoint - firstChar
    struct Golden
    {
        int size = 0;
        std::string text;
        int width = 0;
    };
    std::vector<Golden> goldens;
};

int ParseInt(const std::string &s, std::size_t pos, std::size_t &end)
{
    int value = 0;
    end = pos;
    while (end < s.size() && s[end] >= '0' && s[end] <= '9')
    {
        value = value * 10 + (s[end] - '0');
        ++end;
    }
    return value;
}

bool LoadBakedFont(const char *path, BakedFont &out)
{
    std::ifstream in(path);
    if (!in)
    {
        return false;
    }
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq != std::string::npos)
        {
            std::size_t end = 0;
            const int value = ParseInt(line, eq + 1, end);
            const std::string key = line.substr(0, eq);
            if (key == "baseSize")
            {
                out.baseSize = value;
            }
            else if (key == "firstChar")
            {
                out.firstChar = value;
            }
            else if (key == "textSpacing")
            {
                out.textSpacing = value;
            }
            else if (key == "count")
            {
                out.advances.reserve(static_cast<std::size_t>(value));
            }
            else if (key == "advances")
            {
                std::size_t pos = eq + 1;
                while (pos < line.size())
                {
                    out.advances.push_back(ParseInt(line, pos, pos));
                    if (pos < line.size() && line[pos] == ',')
                    {
                        ++pos;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            continue;
        }
        // golden|<size>|<text>|<width> (text never contains '|').
        if (line.rfind("golden|", 0) == 0)
        {
            const std::size_t a = line.find('|', 7);
            const std::size_t b = a != std::string::npos ? line.find('|', a + 1) : std::string::npos;
            if (a == std::string::npos || b == std::string::npos)
            {
                return false;
            }
            std::size_t end = 0;
            BakedFont::Golden golden;
            golden.size = ParseInt(line, 7, end);
            golden.text = line.substr(a + 1, b - a - 1);
            golden.width = ParseInt(line, b + 1, end);
            out.goldens.push_back(golden);
        }
    }
    return static_cast<bool>(in) || in.eof();
}

// raygui GetLineWidth, op-for-op, over the baked table: single line only
// (stops at \n/\0 like the original), unknown bytes fall back to '?'
// (codepoint 63, mirroring GetGlyphIndex), advanceX==0 branch always
// taken (rec.width), TEXT_SPACING per glyph, float sum truncated to int.
int HeadlessLineWidth(const BakedFont &font, const char *text, int fontSize)
{
    if (text == nullptr || text[0] == '\0' || font.baseSize <= 0)
    {
        return 0;
    }
    const float scale = static_cast<float>(fontSize) / static_cast<float>(font.baseSize);
    float x = 0.0f;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p != '\0' && *p != '\n'; ++p)
    {
        int codepoint = *p;
        if (codepoint < font.firstChar ||
            codepoint >= font.firstChar + static_cast<int>(font.advances.size()))
        {
            codepoint = 63; // '?' fallback, like GetGlyphIndex
        }
        const float glyphWidth =
            static_cast<float>(font.advances[static_cast<std::size_t>(codepoint - font.firstChar)]) *
            scale;
        x += glyphWidth + static_cast<float>(font.textSpacing);
    }
    return static_cast<int>(x);
}

} // namespace

void RunFontMetricsTests()
{
    BakedFont font;
    CC_CHECK(LoadBakedFont("data/font_metrics.txt", font));

    // --- file integrity: the table raygui measures against ---
    CC_CHECK(font.baseSize == 10);
    CC_CHECK(font.firstChar == 32);
    CC_CHECK(font.textSpacing == 1);
    CC_CHECK(font.advances.size() == 224);
    for (int advance : font.advances)
    {
        CC_CHECK(advance >= 1 && advance <= 9); // bitmap-font cell range
    }
    CC_CHECK(!font.goldens.empty());

    // --- replication shape: empty, single glyph, fallback ---
    CC_CHECK(HeadlessLineWidth(font, "", 10) == 0);
    CC_CHECK(HeadlessLineWidth(font, nullptr, 10) == 0);
    // 'A' is codepoint 65: width*1.0 + spacing, single term (exact).
    CC_CHECK(HeadlessLineWidth(font, "A", 10) == font.advances[65 - 32] + 1);
    // Control byte falls back to '?' (codepoint 63).
    CC_CHECK(HeadlessLineWidth(font, "\x01", 10) ==
             HeadlessLineWidth(font, "?", 10));

    // --- goldens: byte-identical to raygui's in-engine measurement ---
    for (const BakedFont::Golden &golden : font.goldens)
    {
        CC_CHECK(HeadlessLineWidth(font, golden.text.c_str(), golden.size) == golden.width);
    }
}
