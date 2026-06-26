#include "app/Islands.hpp"
#include <cmath>
#include <algorithm>

static constexpr float PI  = 3.14159265f;
static constexpr float GAP = 0.5f;   // espace entre îles voisines, en fraction de baseR

// Anneau dont le rayon grandit avec le nombre d'équipes pour que des îles de
// taille PLEINE (baseR, jamais rétrécie) ne se chevauchent jamais : distance
// entre 2 centres voisins = 2*ring*sin(pi/N), on veut >= 2*baseR + GAP*baseR.
static float ringRadius(int count, float baseR)
{
    if (count <= 1) return 0.f;
    return (baseR + baseR * GAP * 0.5f) / std::sin(PI / (float)count);
}

glm::vec2 islandCenter(int idx, int count, float cx, float cz, float baseR)
{
    if (count <= 1) return { cx, cz };
    float ring = ringRadius(count, baseR);
    float a = 2.f * PI * (float)idx / (float)count - PI / 2.f;   // 1ère île en haut
    return { cx + ring * std::cos(a), cz + ring * std::sin(a) };
}

float islandRadius(int /*count*/, float baseR)
{
    // Chaque île est une copie PLEINE TAILLE de la carte d'origine — jamais
    // rétrécie — c'est l'anneau (ringRadius) qui s'agrandit pour les espacer.
    return baseR;
}

float worldExtentRadius(int count, float baseR)
{
    if (count <= 1) return baseR;
    return ringRadius(count, baseR) + baseR;
}
