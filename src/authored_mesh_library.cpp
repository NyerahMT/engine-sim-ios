#include "../include/authored_mesh_library.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
struct Vec3 { float x, y, z; };

bool required(const std::string &name) {
    return name == "Piston" || name == "ConnectingRod" || name == "CylinderHead"
        || name == "Crankshaft" || name == "CrankSnout" || name == "CrankSnoutThreads"
        || name == "Valve" || name == "Logo";
}

bool parseIndex(const std::string &token, unsigned int *position, unsigned int *normal) {
    const std::size_t separator = token.find("//");
    if (separator == std::string::npos) return false;
    const long parsedPosition = std::strtol(token.substr(0, separator).c_str(), nullptr, 10);
    const long parsedNormal = std::strtol(token.substr(separator + 2).c_str(), nullptr, 10);
    if (parsedPosition <= 0 || parsedNormal <= 0) return false;
    *position = static_cast<unsigned int>(parsedPosition - 1);
    *normal = static_cast<unsigned int>(parsedNormal - 1);
    return true;
}

std::string siblingPath(const std::string &path, const std::string &name) {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? name : path.substr(0, separator + 1) + name;
}

bool loadLogoPng(const std::string &path, AuthoredMeshLibrary::Mesh *mesh) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) stbi_image_free(pixels);
        return false;
    }

    mesh->vertices.clear();
    mesh->indices.clear();

    constexpr unsigned char alphaThreshold = 96;
    const float aspect = static_cast<float>(height) / static_cast<float>(width);

    for (int y = 0; y < height; ++y) {
        int x = 0;

        while (x < width) {
            while (x < width && pixels[(y * width + x) * 4 + 3] < alphaThreshold) ++x;
            if (x >= width) break;

            const int x0 = x;
            while (x < width && pixels[(y * width + x) * 4 + 3] >= alphaThreshold) ++x;
            const int x1 = x;

            if (mesh->vertices.size() + 4 > 65535) {
                stbi_image_free(pixels);
                return false;
            }

            const float left = static_cast<float>(x0) / width - 0.5f;
            const float right = static_cast<float>(x1) / width - 0.5f;
            const float top = (0.5f - static_cast<float>(y) / height) * aspect;
            const float bottom = (0.5f - static_cast<float>(y + 1) / height) * aspect;
            const unsigned short base = static_cast<unsigned short>(mesh->vertices.size());

            mesh->vertices.push_back({ { left, bottom, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, {} });
            mesh->vertices.push_back({ { right, bottom, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, {} });
            mesh->vertices.push_back({ { right, top, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, {} });
            mesh->vertices.push_back({ { left, top, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, {} });

            mesh->indices.push_back(base + 0);
            mesh->indices.push_back(base + 1);
            mesh->indices.push_back(base + 2);
            mesh->indices.push_back(base + 0);
            mesh->indices.push_back(base + 2);
            mesh->indices.push_back(base + 3);
        }
    }

    stbi_image_free(pixels);
    return !mesh->vertices.empty() && !mesh->indices.empty();
}
}

bool AuthoredMeshLibrary::load(const std::string &path) {
    std::ifstream file(path);
    if (!file) return false;

    m_meshes.clear();
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    Mesh *currentMesh = nullptr;
    std::unordered_map<std::uint64_t, unsigned short> vertexIndices;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string kind;
        stream >> kind;
        if (kind == "v") {
            Vec3 position = {};
            if (!(stream >> position.x >> position.y >> position.z)) return false;
            positions.push_back(position);
        }
        else if (kind == "vn") {
            Vec3 normal = {};
            if (!(stream >> normal.x >> normal.y >> normal.z)) return false;
            normals.push_back(normal);
        }
        else if (kind == "o") {
            std::string name;
            if (!(stream >> name)) return false;
            currentMesh = nullptr;
            vertexIndices.clear();
            if (required(name)) currentMesh = &m_meshes[name];
        }
        else if (kind == "f") {
            if (currentMesh == nullptr) continue;
            std::string token;
            unsigned int position = 0;
            unsigned int normal = 0;
            int corners = 0;
            while (stream >> token) {
                if (!parseIndex(token, &position, &normal) || position >= positions.size() || normal >= normals.size()) return false;
                const std::uint64_t key = (static_cast<std::uint64_t>(position) << 32) | normal;
                const auto existing = vertexIndices.find(key);
                if (existing != vertexIndices.end()) {
                    currentMesh->indices.push_back(existing->second);
                }
                else {
                    if (currentMesh->vertices.size() > 65535) return false;
                    const unsigned short index = static_cast<unsigned short>(currentMesh->vertices.size());
                    const Vec3 &p = positions[position];
                    const Vec3 &n = normals[normal];
                    currentMesh->vertices.push_back({ { p.x, p.y, p.z, 1 }, { n.x, n.y, n.z, 0 }, {} });
                    currentMesh->indices.push_back(index);
                    vertexIndices.emplace(key, index);
                }
                ++corners;
            }
            if (corners != 3) return false;
        }
    }

    Mesh &iosLogo = m_meshes["LogoIOS"];
    if (!loadLogoPng(siblingPath(path, "ies_logo.png"), &iosLogo)) return false;

    return find("Piston") != nullptr && find("ConnectingRod") != nullptr
        && find("CylinderHead") != nullptr && find("Crankshaft") != nullptr
        && find("CrankSnout") != nullptr && find("CrankSnoutThreads") != nullptr
        && find("Valve") != nullptr && find("Logo") != nullptr
        && find("LogoIOS") != nullptr;
}

const AuthoredMeshLibrary::Mesh *AuthoredMeshLibrary::find(const std::string &name) const {
    const auto found = m_meshes.find(name);
    return found == m_meshes.end() ? nullptr : &found->second;
}
