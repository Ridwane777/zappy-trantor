#pragma once
// Lien repo <-> CDN, en streaming RAM : un asset absent en local n'est PAS écrit
// sur le disque — ses octets sont lus depuis le CDN public (R2) directement en
// mémoire et passés au loader (loadFromMemory / parse depuis une string).
//
// Pour éviter le freeze + accélérer : un store mémoire thread-safe + un préfetch
// PARALLÈLE. On télécharge tous les assets en parallèle (plusieurs curl en même
// temps) dans le store au démarrage ; ensuite chaque loader lit le store en RAM,
// instantané. Rien n'est jamais écrit sur le disque.
//
// URL de base configurable via la variable d'environnement ZAPPY_CDN_URL.

#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

namespace zappy {

inline const std::string &cdnBase()
{
    static const std::string base = [] {
        const char *env = std::getenv("ZAPPY_CDN_URL");
        return std::string(env ? env
            : "https://pub-6d9e00b90894418295af58a44ecfe1df.r2.dev");
    }();
