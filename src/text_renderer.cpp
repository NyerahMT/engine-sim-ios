#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include "../include/text_renderer.h"

#include <fstream>

struct TextRenderer::FontData {
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info = {};
};

TextRenderer::TextRenderer() = default;
TextRenderer::~TextRenderer() = default;

bool TextRenderer::loadFont(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize length = file.tellg();
    if (length <= 0) return false;
    auto font = std::make_unique<FontData>();
    font->bytes.resize(static_cast<size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(font->bytes.data()), length)
        || !stbtt_InitFont(&font->info, font->bytes.data(), 0)) return false;
    m_font = std::move(font);
    return true;
}

float TextRenderer::CalculateWidth(const std::string &text, float height) const {
    float widest = 0.0f, width = 0.0f;
    const float scale = m_font ? stbtt_ScaleForPixelHeight(&m_font->info, height) : 0.0f;
    for (const char c : text) {
        if (c == '\n') { widest = std::max(widest, width); width = 0.0f; continue; }
        if (m_font) { int advance = 0, bearing = 0; stbtt_GetCodepointHMetrics(&m_font->info, static_cast<unsigned char>(c), &advance, &bearing); width += advance * scale; }
        else width += height * characterAdvance(c);
    }
    return std::max(widest, width);
}

void TextRenderer::RenderText(const std::string &text, float x, float y, float height) {
    if (!m_renderCallback || height <= 0.0f) return;
    if (!m_font) return;
    const float scale = stbtt_ScaleForPixelHeight(&m_font->info, height);
    float cursorX = x, cursorY = y;
    std::vector<Run> runs;
    for (const char c : text) {
        if (c == '\n') { cursorX = x; cursorY += height * 1.25f; continue; }
        const int codepoint = static_cast<unsigned char>(c);
        int advance = 0, bearing = 0, width = 0, glyphHeight = 0, xoff = 0, yoff = 0;
        stbtt_GetCodepointHMetrics(&m_font->info, codepoint, &advance, &bearing);
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&m_font->info, scale, scale, codepoint, &width, &glyphHeight, &xoff, &yoff);
        for (int row = 0; bitmap != nullptr && row < glyphHeight; ++row) {
            int column = 0;
            while (column < width) {
                while (column < width && bitmap[row * width + column] < 128) ++column;
                const int first = column;
                while (column < width && bitmap[row * width + column] >= 128) ++column;
                if (first != column) runs.push_back({ cursorX + xoff + first,
                    cursorY - yoff - row - 0.5f, static_cast<float>(column - first), 1.0f });
            }
        }
        stbtt_FreeBitmap(bitmap, nullptr);
        cursorX += advance * scale;
    }
    if (!runs.empty()) m_renderCallback(runs, m_color);
}
