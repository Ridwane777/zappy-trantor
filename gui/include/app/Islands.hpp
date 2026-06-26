#pragma once
#include <glm/glm.hpp>

// Disposition « une île par équipe » : N copies PLEINE TAILLE de la même
// carte, placées sur un anneau autour du centre du monde (l'anneau s'élargit
// avec N, les îles elles ne rétrécissent jamais). Avec 0 ou 1 équipe -> une
// seule île centrale (comportement d'origine). baseR = rayon de la carte
// d'origine.
glm::vec2 islandCenter(int idx, int count, float cx, float cz, float baseR);

// Rayon d'une île — toujours baseR (copie pleine taille), quel que soit N.
float     islandRadius(int count, float baseR);

// Rayon englobant tout l'anneau d'îles depuis le centre du monde (pour cadrer
// la caméra / calculer les bornes de zoom). count<=1 -> juste baseR.
float     worldExtentRadius(int count, float baseR);
