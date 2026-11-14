#include "game/Resource.hpp"

namespace zappy {

std::optional<Resource> parseResource(std::string_view text)
{
    for (Resource r : ALL_RESOURCES)
        if (name(r) == text)
            return r;
    return std::nullopt;
}

} // namespace zappy
