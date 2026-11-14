#include "app/GuiProtocol.hpp"
#include <sstream>

namespace zappy::gui {

namespace {

std::string id(int n) { return "#" + std::to_string(n); }
std::string at(Position p) { return std::to_string(p.x) + " " + std::to_string(p.y); }

std::string counts(const ResourceCounts &c)
{
    std::string out;
    for (int n : c)
        out += " " + std::to_string(n);
    return out;
}

} // namespace

std::string msz(const Map &map)
{
    return "msz " + std::to_string(map.width()) + " " + std::to_string(map.height()) + "\n";
}

std::string bct(const Map &map, Position pos)
{
    Position p = map.wrap(pos);
    return "bct " + at(p) + counts(map.tile(p).resources()) + "\n";
}

std::string mct(const Map &map)
{
    std::string out;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            out += bct(map, {x, y});
    return out;
}

std::string tna(const std::string &team) { return "tna " + team + "\n"; }

std::string pnw(const Player &p)
{
    return "pnw " + id(p.id()) + " " + at(p.position()) + " " +
           std::to_string(static_cast<int>(p.orientation())) + " " +
           std::to_string(p.level()) + " " + p.team() + "\n";
}

std::string ppo(const Player &p)
{
    return "ppo " + id(p.id()) + " " + at(p.position()) + " " +
           std::to_string(static_cast<int>(p.orientation())) + "\n";
}

std::string plv(const Player &p) { return "plv " + id(p.id()) + " " + std::to_string(p.level()) + "\n"; }

std::string pin(const Player &p)
{
    return "pin " + id(p.id()) + " " + at(p.position()) + counts(p.inventory()) + "\n";
}

std::string pex(const Player &p) { return "pex " + id(p.id()) + "\n"; }

std::string pbc(const Player &p, const std::string &message)
{
    return "pbc " + id(p.id()) + " " + message + "\n";
}

std::string pic(Position pos, int level, const std::vector<int> &playerIds)
{
    std::string out = "pic " + at(pos) + " " + std::to_string(level);
    for (int pid : playerIds)
        out += " " + id(pid);
    return out + "\n";
}

std::string pie(Position pos, bool success) { return "pie " + at(pos) + (success ? " 1\n" : " 0\n"); }
std::string pfk(const Player &p) { return "pfk " + id(p.id()) + "\n"; }

std::string pdr(const Player &p, Resource r)
{
    return "pdr " + id(p.id()) + " " + std::to_string(index(r)) + "\n";
}

std::string pgt(const Player &p, Resource r)
{
    return "pgt " + id(p.id()) + " " + std::to_string(index(r)) + "\n";
}

std::string pdi(const Player &p) { return "pdi " + id(p.id()) + "\n"; }

std::string enw(const Egg &egg, int parentId)
{
    return "enw " + id(egg.id) + " " + id(parentId) + " " + at(egg.pos) + "\n";
}

std::string ebo(const Egg &egg) { return "ebo " + id(egg.id) + "\n"; }
std::string edi(const Egg &egg) { return "edi " + id(egg.id) + "\n"; }
std::string sgt(int freq) { return "sgt " + std::to_string(freq) + "\n"; }
std::string sst(int freq) { return "sst " + std::to_string(freq) + "\n"; }
std::string seg(const std::string &team) { return "seg " + team + "\n"; }
std::string smg(const std::string &message) { return "smg " + message + "\n"; }
std::string suc() { return "suc\n"; }
std::string sbp() { return "sbp\n"; }

} // namespace zappy::gui
