#include "net/GuiProtocolParser.hpp"

#include <charconv>
#include <sstream>

namespace {

constexpr std::size_t MAX_LOG = 100;

// "#12" or "12" -> 12, anything else -> -1.
int parseId(const std::string &tok)
{
    std::size_t start = (!tok.empty() && tok[0] == '#') ? 1 : 0;
    int value = -1;
    auto [ptr, ec] = std::from_chars(tok.data() + start, tok.data() + tok.size(), value);
    return (ec == std::errc() && ptr == tok.data() + tok.size()) ? value : -1;
}

const char *resourceName(int i)
{
    return (i >= 0 && i < GUI_RES_COUNT) ? GUI_RESOURCE_NAMES[i] : "?";
}

void logEvent(GuiState &s, int id, const std::string &action, int kind)
{
    std::string team;
    auto it = s.players.find(id);
    if (it != s.players.end())
        team = it->second.team;
    s.log.push_back({id, team, action, kind});
    s.logTotal++;
    if (s.log.size() > MAX_LOG)
        s.log.pop_front();
}

} // namespace

bool GuiProtocolParser::apply(const std::string &line, GuiState &s)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "msz") {
        int w, h;
        if (iss >> w >> h && w > 0 && h > 0)
            s.resizeGrid(w, h);
    } else if (cmd == "sgt" || cmd == "sst") {
        int t;
        if (iss >> t)
            s.timeUnit = t;
    } else if (cmd == "tna") {
        std::string team;
        if (iss >> team)
            s.teams.push_back(team);
    } else if (cmd == "bct") {
        int x, y;
        if (iss >> x >> y && s.inBounds(x, y)) {
            GuiTile &t = s.tileAt(x, y);
            for (int i = 0; i < GUI_RES_COUNT; ++i)
                iss >> t.res[i];
            s.resDirty = true;
        }
    } else if (cmd == "pnw") {
        GuiPlayer p;
        std::string idTok;
        if (iss >> idTok >> p.x >> p.y >> p.orientation >> p.level >> p.team) {
            p.id = parseId(idTok);
            s.players[p.id] = p;
        }
    } else if (cmd == "ppo") {
        std::string idTok;
        int x, y, o;
        if (iss >> idTok >> x >> y >> o) {
            auto it = s.players.find(parseId(idTok));
            if (it != s.players.end()) {
                it->second.x = x;
                it->second.y = y;
                it->second.orientation = o;
            }
        }
    } else if (cmd == "plv") {
        std::string idTok;
        int lvl;
        if (iss >> idTok >> lvl) {
            int id = parseId(idTok);
            auto it = s.players.find(id);
            if (it != s.players.end())
                it->second.level = lvl;
            logEvent(s, id, "reached level " + std::to_string(lvl), 1);
        }
    } else if (cmd == "pgt") {
        std::string idTok;
        int r;
        if (iss >> idTok >> r)
            logEvent(s, parseId(idTok), std::string("grabbed ") + resourceName(r), 0);
    } else if (cmd == "pin") {
        std::string idTok;
        int x, y;
        if (iss >> idTok >> x >> y) {
            auto it = s.players.find(parseId(idTok));
            if (it != s.players.end()) {
                it->second.x = x;
                it->second.y = y;
                for (int i = 0; i < GUI_RES_COUNT; ++i)
                    iss >> it->second.inv[i];
                it->second.invKnown = true;
            }
        }
    } else if (cmd == "pdi") {
        std::string idTok;
        if (iss >> idTok) {
            int id = parseId(idTok);
            logEvent(s, id, "died", 2);
            s.players.erase(id);
        }
    } else if (cmd == "enw") {
        GuiEgg e;
        std::string eggTok, playerTok;
        if (iss >> eggTok >> playerTok >> e.x >> e.y) {
            e.id = parseId(eggTok);
            auto pit = s.players.find(parseId(playerTok));
            if (pit != s.players.end())
                e.team = pit->second.team;
            s.eggs[e.id] = e;
        }
    } else if (cmd == "edi" || cmd == "ebo") {   // egg destroyed, or hatched into a player
        std::string eggTok;
        if (iss >> eggTok)
            s.eggs.erase(parseId(eggTok));
    } else if (cmd == "pic") {
        int x, y, lvl;
        if (iss >> x >> y >> lvl) {
            std::string idTok;
            int firstId = -1;
            while (iss >> idTok) {
                int id = parseId(idTok);
                if (firstId < 0)
                    firstId = id;
                auto it = s.players.find(id);
                if (it != s.players.end())
                    it->second.incanting = true;
            }
            if (firstId >= 0)
                logEvent(s, firstId, "begins an incantation", 3);
        }
    } else if (cmd == "pie") {
        int x, y, result;
        if (iss >> x >> y >> result)
            for (auto &kv : s.players)
                if (kv.second.x == x && kv.second.y == y)
                    kv.second.incanting = false;
    } else if (cmd == "pex") {
        std::string idTok;
        if (iss >> idTok) {
            int id = parseId(idTok);
            auto it = s.players.find(id);
            if (it != s.players.end())
                it->second.ejected = true;
            logEvent(s, id, "was ejected", 4);
        }
    } else if (cmd == "seg") {
        std::string team;
        if (iss >> team)
            s.winner = team;
    } else if (cmd == "smg") {
        std::string message;
        std::getline(iss >> std::ws, message);
        logEvent(s, -1, "server: " + message, 5);
    } else if (cmd == "suc" || cmd == "sbp" || cmd == "pfk" || cmd == "pdr" || cmd == "pbc") {
        // acknowledged, nothing to show beyond the raw feed
    } else {
        return false;
    }
    return true;
}
