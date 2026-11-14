#pragma once

#include "game/World.hpp"
#include <string>
#include <vector>

// Text of every GUI protocol message, built in one place. Each function returns
// one line ending with '\n'.
namespace zappy::gui {

std::string msz(const Map &map);
std::string bct(const Map &map, Position pos);
std::string mct(const Map &map);
std::string tna(const std::string &team);
std::string pnw(const Player &p);
std::string ppo(const Player &p);
std::string plv(const Player &p);
std::string pin(const Player &p);
std::string pex(const Player &p);
std::string pbc(const Player &p, const std::string &message);
std::string pic(Position pos, int level, const std::vector<int> &playerIds);
std::string pie(Position pos, bool success);
std::string pfk(const Player &p);
std::string pdr(const Player &p, Resource r);
std::string pgt(const Player &p, Resource r);
std::string pdi(const Player &p);
std::string enw(const Egg &egg, int parentId);
std::string ebo(const Egg &egg);
std::string edi(const Egg &egg);
std::string sgt(int freq);
std::string sst(int freq);
std::string seg(const std::string &team);
std::string smg(const std::string &message);
std::string suc();
std::string sbp();

} // namespace zappy::gui
