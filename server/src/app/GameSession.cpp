#include "app/GameSession.hpp"
#include "app/GuiProtocol.hpp"
#include "util/Parse.hpp"
#include <iostream>
#include <sstream>
#include <vector>

namespace zappy::app {

namespace {

std::vector<std::string> words(const std::string &line)
{
    std::istringstream iss(line);
    std::vector<std::string> out;
    for (std::string w; iss >> w;)
        out.push_back(w);
    return out;
}

// "#12" -> 12
std::optional<long> parsePlayerId(const std::string &token)
{
    if (token.size() < 2 || token[0] != '#')
        return std::nullopt;
    return parseInt(std::string_view(token).substr(1));
}

} // namespace

GameSession::GameSession(net::IClientSink &clients, int freq) : _clients(clients), _clock(freq) {}

void GameSession::advanceClock()
{
    int ticks = _clock.consumeTicks();
    if (ticks > 0 && _world)
        _world->update(ticks);
}

// ----- network side ---------------------------------------------------------------

void GameSession::onConnect(net::ConnectionId id)
{
    _info[id] = ClientInfo{};
    _clients.send(id, "WELCOME\n");
}

void GameSession::onLine(net::ConnectionId id, const std::string &line)
{
    auto it = _info.find(id);
    if (it == _info.end() || !_world)
        return;
    ClientInfo &info = it->second;
    switch (info.role) {
    case Role::Pending:
        handshake(id, info, line);
        break;
    case Role::Ai:
        // A full queue drops the line: that is the protocol's back-pressure.
        _world->queueCommand(info.playerId, line);
        break;
    case Role::Gui:
        guiRequest(id, line);
        break;
    }
}

void GameSession::onDisconnect(net::ConnectionId id)
{
    auto it = _info.find(id);
    if (it == _info.end())
        return;
    if (it->second.role == Role::Ai && _world) {
        _connectionOf.erase(it->second.playerId);
        _world->leave(it->second.playerId);
    }
    _info.erase(it);
}

void GameSession::handshake(net::ConnectionId id, ClientInfo &info, const std::string &line)
{
    if (line == "GRAPHIC") {
        info.role = Role::Gui;
        sendGuiBootstrap(id);
        return;
    }
    auto joined = _world->join(line);
    if (!joined) {
        _clients.send(id, "ko\n");
        _clients.closeAfterFlush(id);
        return;
    }
    info.role = Role::Ai;
    info.playerId = joined->playerId;
    _connectionOf[joined->playerId] = id;
    const Map &map = _world->map();
    _clients.send(id, std::to_string(joined->freeSlots) + "\n" + std::to_string(map.width()) + " " +
                          std::to_string(map.height()) + "\n");
}

void GameSession::sendGuiBootstrap(net::ConnectionId id)
{
    std::string out = gui::msz(_world->map());
    for (const auto &team : _world->teams())
        out += gui::tna(team);
    out += gui::mct(_world->map());
    for (const auto &p : _world->players())
        if (!p.isDead())
            out += gui::pnw(p);
    out += gui::sgt(_clock.freq());
    _clients.send(id, out);
}

void GameSession::guiRequest(net::ConnectionId id, const std::string &line)
{
    auto w = words(line);
    if (w.empty()) {
        _clients.send(id, gui::suc());
        return;
    }
    const std::string &cmd = w[0];
    auto badParameter = [&] {
        std::cerr << "[gui] bad parameters: \"" << line << "\"\n";
        _clients.send(id, gui::sbp());
    };
    auto withPlayer = [&](auto format) {
        auto pid = w.size() == 2 ? parsePlayerId(w[1]) : std::nullopt;
        const Player *p = pid ? _world->findPlayer(static_cast<int>(*pid)) : nullptr;
        if (!p || p->isDead())
            return badParameter();
        _clients.send(id, format(*p));
    };

    if (cmd == "msz" && w.size() == 1) {
        _clients.send(id, gui::msz(_world->map()));
    } else if (cmd == "mct" && w.size() == 1) {
        _clients.send(id, gui::mct(_world->map()));
    } else if (cmd == "tna" && w.size() == 1) {
        std::string out;
        for (const auto &team : _world->teams())
            out += gui::tna(team);
        _clients.send(id, out);
    } else if (cmd == "bct") {
        auto x = w.size() == 3 ? parseInt(w[1]) : std::nullopt;
        auto y = w.size() == 3 ? parseInt(w[2]) : std::nullopt;
        const Map &map = _world->map();
        if (!x || !y || *x < 0 || *y < 0 || *x >= map.width() || *y >= map.height())
            return badParameter();
        _clients.send(id, gui::bct(map, {static_cast<int>(*x), static_cast<int>(*y)}));
    } else if (cmd == "ppo") {
        withPlayer([](const Player &p) { return gui::ppo(p); });
    } else if (cmd == "plv") {
        withPlayer([](const Player &p) { return gui::plv(p); });
    } else if (cmd == "pin") {
        withPlayer([](const Player &p) { return gui::pin(p); });
    } else if (cmd == "sgt" && w.size() == 1) {
        _clients.send(id, gui::sgt(_clock.freq()));
    } else if (cmd == "sst") {
        auto freq = w.size() == 2 ? parseInt(w[1]) : std::nullopt;
        if (!freq || *freq <= 0 || *freq > 10000)
            return badParameter();
        _clock.setFreq(static_cast<int>(*freq));
        _clients.send(id, gui::sst(_clock.freq()));
        toGui(gui::smg("time unit set to " + std::to_string(_clock.freq())));
    } else if (cmd == "msz" || cmd == "mct" || cmd == "tna" || cmd == "sgt") {
        badParameter();
    } else {
        _clients.send(id, gui::suc());
    }
}

void GameSession::toGui(const std::string &text)
{
    for (const auto &[id, info] : _info)
        if (info.role == Role::Gui)
            _clients.send(id, text);
}

void GameSession::toPlayer(int playerId, const std::string &text)
{
    auto it = _connectionOf.find(playerId);
    if (it != _connectionOf.end())
        _clients.send(it->second, text);
}

// ----- game side ------------------------------------------------------------------

void GameSession::onReply(int playerId, const std::string &line) { toPlayer(playerId, line + "\n"); }

void GameSession::onPlayerDied(const Player &p)
{
    toPlayer(p.id(), "dead\n");
    auto it = _connectionOf.find(p.id());
    if (it != _connectionOf.end())
        _clients.closeAfterFlush(it->second);
    toGui(gui::pdi(p));
}

void GameSession::onPlayerJoined(const Player &p) { toGui(gui::pnw(p)); }
void GameSession::onPlayerMoved(const Player &p) { toGui(gui::ppo(p)); }
void GameSession::onPlayerLevel(const Player &p) { toGui(gui::plv(p)); }
void GameSession::onPlayerInventory(const Player &p) { toGui(gui::pin(p)); }
void GameSession::onPlayerLeft(const Player &p) { toGui(gui::pdi(p)); }
void GameSession::onResourceTaken(const Player &p, Resource r) { toGui(gui::pgt(p, r)); }
void GameSession::onResourceDropped(const Player &p, Resource r) { toGui(gui::pdr(p, r)); }
void GameSession::onTileChanged(Position pos) { toGui(gui::bct(_world->map(), pos)); }
void GameSession::onPlayerExpelled(const Player &p) { toGui(gui::pex(p)); }

void GameSession::onBroadcast(const Player &p, const std::string &message)
{
    toGui(gui::pbc(p, message));
}

void GameSession::onIncantationStart(Position pos, int level, const std::vector<int> &playerIds)
{
    toGui(gui::pic(pos, level, playerIds));
}

void GameSession::onIncantationEnd(Position pos, bool success) { toGui(gui::pie(pos, success)); }

void GameSession::onEggLaid(const Player &p, const Egg &egg)
{
    toGui(gui::pfk(p) + gui::enw(egg, p.id()));
}

void GameSession::onEggHatched(const Egg &egg) { toGui(gui::ebo(egg)); }
void GameSession::onEggDestroyed(const Egg &egg) { toGui(gui::edi(egg)); }

void GameSession::onGameWon(const std::string &team)
{
    toGui(gui::seg(team) + gui::smg("team " + team + " wins"));
}

} // namespace zappy::app
