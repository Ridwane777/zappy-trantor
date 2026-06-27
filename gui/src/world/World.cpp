#include "world/World.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "render/ObjLoader.hpp"
#include "GUIException.hpp"
#include <json.hpp>
using json = nlohmann::json;

#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include <sys/stat.h>

// Mesh is move-only (no default ctor), so store by unique_ptr.
static std::unordered_map<std::string, std::unique_ptr<Mesh>> g_meshes;

