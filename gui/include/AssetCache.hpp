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
    return base;
}

namespace detail {
inline std::mutex &storeMutex() { static std::mutex m; return m; }
inline std::unordered_map<std::string, std::string> &store()
{
    static std::unordered_map<std::string, std::string> s;
    return s;
}

// Lecture brute : fichier local si présent, sinon stream depuis le CDN en RAM
// (curl vers stdout), SANS écrire de fichier.
inline std::string fetchRaw(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (f.good())
        return std::string((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

    const std::string prefix = "assets/";
    if (path.rfind(prefix, 0) != 0)
        return {};
    std::string rel = path.substr(prefix.size());

    std::string cmd = "curl -fsSL --connect-timeout 10 --max-time 120 '"
                    + cdnBase() + "/" + rel + "'";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return {};
    std::string out;
    char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0)
        out.append(buf, n);
    if (pclose(p) != 0 || out.empty())
        std::cerr << "[cdn] WARN: échec du chargement de " << rel << "\n";
    return out;
}
} // namespace detail

// Référence stable vers les octets de `path` (mis en cache RAM la 1re fois).
// Stable car les références d'éléments d'unordered_map survivent au rehash.
inline const std::string &assetBytesKept(const std::string &path)
{
    auto &st = detail::store();
    auto &mx = detail::storeMutex();
    {
        std::lock_guard<std::mutex> lk(mx);
        auto it = st.find(path);
        if (it != st.end()) return it->second;
    }
    std::string data = detail::fetchRaw(path);     // hors lock (réseau)
    if (data.empty()) {                            // NE PAS cacher un échec :
        static const std::string empty;            // un appel ultérieur réessaiera
        return empty;
    }
    std::lock_guard<std::mutex> lk(mx);
    return st.emplace(path, std::move(data)).first->second;
}

// Copie des octets de `path` (local ou CDN, caché en RAM). Vide si introuvable.
inline std::string assetBytes(const std::string &path)
{
    return assetBytesKept(path);
}

// NON-BLOQUANT : renvoie les octets seulement s'ils sont DÉJÀ en RAM (préfetchés),
// sinon vide — ne déclenche jamais de téléchargement. Pour le thread principal
