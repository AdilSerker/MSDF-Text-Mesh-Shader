#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

struct MsdfBounds
{
    float left = 0, bottom = 0, right = 0, top = 0;
};

struct MsdfGlyph
{
    uint32_t codepoint = 0;
    float advance = 0.0f;

    bool hasPlane = false;
    bool hasAtlas = false;

    MsdfBounds plane; // font units (relative to baseline)
    MsdfBounds atlas; // pixels in atlas
};

struct MsdfMetrics
{
    float emSize = 48.0f;
    float lineHeight = 0.0f;
    float ascender = 0.0f;
    float descender = 0.0f;
};

class MsdfFont
{
public:
    bool loadFromJson(const std::string& jsonPath);

    const MsdfGlyph* find(uint32_t cp) const;

    int atlasW() const { return m_atlasW; }
    int atlasH() const { return m_atlasH; }
    float pxRange() const { return m_pxRange; }
    const MsdfMetrics& metrics() const { return m_metrics; }

    bool atlasYBottom() const { return m_atlasYBottom; }
    bool m_atlasYBottom = true;

private:
    int m_atlasW = 0;
    int m_atlasH = 0;
    float m_pxRange = 4.0f;

    MsdfMetrics m_metrics{};
    std::unordered_map<uint32_t, MsdfGlyph> m_glyphs;
};
