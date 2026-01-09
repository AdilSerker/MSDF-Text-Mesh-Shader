#include "vk/MsdfAtlas.h"
#include <fstream>
#include <iostream>
#include <cctype>

bool loadFileBytes(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Failed to open file: " << path << "\n";
        return false;
    }
    std::streamsize size = f.tellg();
    if (size <= 0) {
        std::cerr << "Empty file: " << path << "\n";
        return false;
    }
    out.resize((size_t)size);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), size);
    return true;
}

static bool parse_int_field(const std::string& s, const char* key, int& out)
{
    size_t p = s.find(key);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    ++p;
    while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;

    bool neg = false;
    if (p < s.size() && s[p] == '-') { neg = true; ++p; }

    int v = 0;
    bool any = false;
    while (p < s.size() && std::isdigit((unsigned char)s[p])) {
        any = true;
        v = v * 10 + (s[p] - '0');
        ++p;
    }
    if (!any) return false;
    out = neg ? -v : v;
    return true;
}

static bool parse_float_field(const std::string& s, const char* key, float& out)
{
    size_t p = s.find(key);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    ++p;
    while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;

    // very small, permissive float parse
    size_t start = p;
    while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '-' || s[p] == '+' || s[p] == '.' || s[p] == 'e' || s[p] == 'E'))
        ++p;

    if (p <= start) return false;
    out = std::stof(s.substr(start, p - start));
    return true;
}

bool loadMsdfAtlasInfoFromJson(const std::string& jsonPath, MsdfAtlasInfo& out)
{
    std::vector<uint8_t> bytes;
    if (!loadFileBytes(jsonPath, bytes))
        return false;

    std::string js(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    // Берём только кусок вокруг "atlas" (чтобы не схватить width/height из glyph bounds)
    size_t atlasPos = js.find("\"atlas\"");
    if (atlasPos == std::string::npos) {
        std::cerr << "JSON has no \"atlas\" section: " << jsonPath << "\n";
        return false;
    }

    std::string sub = js.substr(atlasPos, std::min<size_t>(js.size() - atlasPos, 4000));

    if (!parse_int_field(sub, "\"width\"", out.width) || !parse_int_field(sub, "\"height\"", out.height)) {
        std::cerr << "Failed to parse atlas width/height from: " << jsonPath << "\n";
        return false;
    }

    // pxRange может быть либо в atlas секции, либо отсутствовать (тогда пользуемся тем, что ты задал в генераторе)
    float pr = out.pxRange;
    if (parse_float_field(sub, "\"pxRange\"", pr))
        out.pxRange = pr;

    return true;
}
