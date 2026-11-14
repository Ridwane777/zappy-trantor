#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace zappy {

enum class Resource : std::size_t {
    Food = 0,
    Linemate,
    Deraumere,
    Sibur,
    Mendiane,
    Phiras,
    Thystame,
};

constexpr std::size_t RESOURCE_COUNT = 7;

constexpr std::array<Resource, RESOURCE_COUNT> ALL_RESOURCES = {
    Resource::Food, Resource::Linemate, Resource::Deraumere, Resource::Sibur,
    Resource::Mendiane, Resource::Phiras, Resource::Thystame,
};

constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES = {
    "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame",
};

constexpr std::size_t index(Resource r) { return static_cast<std::size_t>(r); }

constexpr std::string_view name(Resource r) { return RESOURCE_NAMES[index(r)]; }

// Returns the resource called `text`, or nothing for an unknown name.
std::optional<Resource> parseResource(std::string_view text);

// Quantities of each resource, indexed by Resource.
using ResourceCounts = std::array<int, RESOURCE_COUNT>;

} // namespace zappy
