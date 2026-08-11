#include "../include/authored_mesh_library.h"

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

    return find("Piston") != nullptr && find("ConnectingRod") != nullptr
        && find("CylinderHead") != nullptr && find("Crankshaft") != nullptr
        && find("CrankSnout") != nullptr && find("CrankSnoutThreads") != nullptr
        && find("Valve") != nullptr && find("Logo") != nullptr;
}

const AuthoredMeshLibrary::Mesh *AuthoredMeshLibrary::find(const std::string &name) const {
    const auto found = m_meshes.find(name);
    return found == m_meshes.end() ? nullptr : &found->second;
}
