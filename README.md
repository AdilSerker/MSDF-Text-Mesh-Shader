# MSDF Text Mesh Shader

A Vulkan-based text rendering project using Multi-channel Signed Distance Field (MSDF) technique with mesh shaders.

## 📋 Overview

This project implements high-quality text rendering using MSDF atlases and Vulkan mesh shaders, providing efficient and scalable text display with excellent visual quality at any size.

## 🛠️ Building

### Prerequisites

- CMake 3.15 or higher
- C++20 compatible compiler
- Vulkan SDK
- [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) (for font generation)

### Build Instructions

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 🔤 Font Generation

To generate MSDF font atlases, use the msdf-atlas-gen tool:

```powershell
.\msdf-atlas-gen.exe `
  -font "C:\Windows\Fonts\arial.ttf" `
  -type msdf `
  -size 48 `
  -pxrange 4 `
  -format rgba `
  -imageout "assets/font.rgba" `
  -json "assets/font.json"
```

### Parameters

- `-font`: Path to the TrueType font file
- `-type`: Distance field type (msdf, sdf, psdf)
- `-size`: Font size in pixels
- `-pxrange`: Distance field range in pixels
- `-format`: Output image format
- `-imageout`: Output image file path
- `-json`: Output JSON metadata file path

## 📁 Project Structure

```
├── src/                    # Source code
│   ├── main.cpp           # Application entry point
│   ├── platform/          # Platform abstraction
│   └── vk/                # Vulkan rendering code
├── shaders/               # GLSL shader sources
├── assets/                # Font assets (MSDF atlases)
└── build/                 # Build output directory
```

## 🚀 Running

After building, run the executable from the build directory:

```bash
.\build\Debug\app.exe
```

## 📝 License

This project is provided as-is for educational and research purposes.