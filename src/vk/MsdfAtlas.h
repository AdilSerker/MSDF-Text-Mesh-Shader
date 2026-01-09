#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct MsdfAtlasInfo {
    int width = 0;
    int height = 0;
    float pxRange = 2.0f;
};

bool loadFileBytes(const std::string& path, std::vector<uint8_t>& out);
bool loadMsdfAtlasInfoFromJson(const std::string& jsonPath, MsdfAtlasInfo& out);
