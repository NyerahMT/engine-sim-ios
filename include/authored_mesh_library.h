#ifndef ATG_ENGINE_SIM_AUTHORED_MESH_LIBRARY_H
#define ATG_ENGINE_SIM_AUTHORED_MESH_LIBRARY_H

#include "render_math.h"

#include <string>
#include <unordered_map>
#include <vector>

// Loads the portable OBJ exported from art/assets.blend.
class AuthoredMeshLibrary {
public:
    struct Mesh {
        std::vector<EngineSimVertex> vertices;
        std::vector<unsigned short> indices;
    };

    bool load(const std::string &path);
    const Mesh *find(const std::string &name) const;

private:
    std::unordered_map<std::string, Mesh> m_meshes;
};

#endif
