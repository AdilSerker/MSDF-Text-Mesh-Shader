#include "vk/MsdfFont.h"
#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

static bool readFileBytes(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize sz = f.tellg();
    if (sz <= 0) return false;
    out.resize((size_t)sz);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return true;
}

static bool readBounds(const nlohmann::json& j, const char* key, MsdfBounds& b)
{
    if (!j.contains(key) || j[key].is_null())
        return false;

    const auto& o = j[key];
    if (!o.contains("left") || !o.contains("bottom") || !o.contains("right") || !o.contains("top"))
        return false;

    b.left   = o["left"].get<float>();
    b.bottom = o["bottom"].get<float>();
    b.right  = o["right"].get<float>();
    b.top    = o["top"].get<float>();
    return true;
}

bool MsdfFont::loadFromJson(const std::string& jsonPath)
{
    std::vector<uint8_t> bytes;
    if (!readFileBytes(jsonPath, bytes))
    {
        std::cerr << "Failed to read json: " << jsonPath << "\n";
        return false;
    }

    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(bytes.begin(), bytes.end());
    }
    catch (const std::exception& e)
    {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("atlas"))
    {
        std::cerr << "JSON has no atlas section\n";
        return false;
    }

    const auto& a = j["atlas"];
    m_atlasW = a.value("width", 0);
    m_atlasH = a.value("height", 0);

    std::string yOrigin = a.value("yOrigin", "bottom");
    m_atlasYBottom = (yOrigin == "bottom");

    // у msdf-atlas-gen чаще всего distanceRange
    m_pxRange = a.value("distanceRange", m_pxRange);
    // на всякий случай
    if (a.contains("pxRange"))
        m_pxRange = a.value("pxRange", m_pxRange);

    if (j.contains("metrics"))
    {
        const auto& m = j["metrics"];
        m_metrics.emSize = m.value("emSize", m_metrics.emSize);
        m_metrics.lineHeight = m.value("lineHeight", m_metrics.lineHeight);
        m_metrics.ascender = m.value("ascender", m_metrics.ascender);
        m_metrics.descender = m.value("descender", m_metrics.descender);
    }

    if (!j.contains("glyphs") || !j["glyphs"].is_array())
    {
        std::cerr << "JSON has no glyphs array\n";
        return false;
    }

    m_glyphs.clear();
    for (const auto& g : j["glyphs"])
    {
        MsdfGlyph glyph{};
        glyph.codepoint = g.value("unicode", 0);
        glyph.advance = g.value("advance", 0.0f);

        glyph.hasPlane = readBounds(g, "planeBounds", glyph.plane);
        glyph.hasAtlas = readBounds(g, "atlasBounds", glyph.atlas);

        m_glyphs[glyph.codepoint] = glyph;
    }

    if (m_atlasW <= 0 || m_atlasH <= 0)
    {
        std::cerr << "Invalid atlas size in json\n";
        return false;
    }

    std::cout << "Font JSON loaded: glyphs=" << m_glyphs.size()
              << ", atlas=" << m_atlasW << "x" << m_atlasH
              << ", pxRange=" << m_pxRange
              << ", emSize=" << m_metrics.emSize << "\n";

    return true;
}

const MsdfGlyph* MsdfFont::find(uint32_t cp) const
{
    auto it = m_glyphs.find(cp);
    if (it == m_glyphs.end())
        return nullptr;
    return &it->second;
}
