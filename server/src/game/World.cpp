#include "game/World.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace zappy {

namespace {

const rules::CommandSpec *findCommand(const std::string &name)
{
    for (const auto &spec : rules::COMMANDS)
        if (spec.name == name)
            return &spec;
    return nullptr;
}

Position step(Position p, Orientation o)
{
    switch (o) {
    case Orientation::North: return {p.x, p.y - 1};
    case Orientation::South: return {p.x, p.y + 1};
    case Orientation::East:  return {p.x + 1, p.y};
    case Orientation::West:  return {p.x - 1, p.y};
    }
    return p;
}

Orientation rightOf(Orientation o)
{
    return static_cast<Orientation>(static_cast<int>(o) % 4 + 1);
}

} // namespace

int soundDirection(Position from, const Player &receiver, int width, int height)
{
    Position to = receiver.position();
    int dx = from.x - to.x;
    int dy = from.y - to.y;
    if (dx > width / 2) dx -= width;
    if (dx < -width / 2) dx += width;
    if (dy > height / 2) dy -= height;
    if (dy < -height / 2) dy += height;
    if (dx == 0 && dy == 0)
        return 0;

    double facing = 0.0;
    switch (receiver.orientation()) {
    case Orientation::North: facing = -M_PI / 2; break;
    case Orientation::East:  facing = 0.0; break;
    case Orientation::South: facing = M_PI / 2; break;
    case Orientation::West:  facing = M_PI; break;
    }
    double rel = std::atan2(dy, dx) - facing;
    while (rel > M_PI) rel -= 2 * M_PI;
    while (rel < -M_PI) rel += 2 * M_PI;
    int sector = static_cast<int>(std::round(rel / (M_PI / 4)));
    sector = ((sector % 8) + 8) % 8;
    return sector + 1;
}

World::World(const WorldConfig &config, IWorldObserver &observer)
    : _config(config), _observer(observer), _rng(config.seed),
      _map(config.width, config.height, _rng)
{
    for (const auto &team : _config.teams)
        _freeSlots[team] = _config.slotsPerTeam;
}

Player *World::find(int id)
{
    for (auto &p : _players)
        if (p.id() == id)
            return &p;
    return nullptr;
}

const Player *World::findPlayer(int id) const
{
    for (const auto &p : _players)
        if (p.id() == id)
            return &p;
    return nullptr;
}

int World::freeSlotsFor(const std::string &team) const
{
    auto slots = _freeSlots.find(team);
    int free = slots == _freeSlots.end() ? 0 : slots->second;
    for (const auto &egg : _eggs)
        if (egg.team == team)
            ++free;
    return free;
}

std::optional<World::Join> World::join(const std::string &team)
{
    auto slots = _freeSlots.find(team);
    if (slots == _freeSlots.end())
        return std::nullopt;

    Position pos{};
    if (slots->second > 0) {
        --slots->second;
        pos = _map.randomPosition();
    } else {
        auto egg = std::find_if(_eggs.begin(), _eggs.end(),
                                [&team](const Egg &e) { return e.team == team; });
        if (egg == _eggs.end())
            return std::nullopt;
        pos = egg->pos;
        Egg hatched = *egg;
        _eggs.erase(egg);
        _observer.onEggHatched(hatched);
    }
    _players.emplace_back(_nextPlayerId++, team, pos, Orientation::North);
    const Player &p = _players.back();
    _observer.onPlayerJoined(p);
    return Join{p.id(), freeSlotsFor(team)};
}

void World::leave(int playerId)
{
    auto it = std::find_if(_players.begin(), _players.end(),
                           [playerId](const Player &p) { return p.id() == playerId; });
    if (it == _players.end())
        return;
    if (!it->isDead())
        _observer.onPlayerLeft(*it);
    _players.erase(it);
}

bool World::queueCommand(int playerId, const std::string &line)
{
    Player *p = find(playerId);
    if (!p || p->isDead())
        return false;

    std::istringstream iss(line);
    std::string name;
    iss >> name;
    std::string arg;
    std::getline(iss >> std::ws, arg);

    const rules::CommandSpec *spec = findCommand(name);
    Action action;
    action.name = name;
    action.arg = arg;
    action.remainingTicks = spec ? spec->cost : 0;
    action.known = spec && (spec->takesArgument || arg.empty());
    return p->enqueue(std::move(action));
}

void World::update(int ticks)
{
    for (int t = 0; t < ticks; ++t)
        tick();
}

void World::tick()
{
    if (++_spawnTimer >= rules::RESPAWN_PERIOD) {
        _map.spawnResources();
        _spawnTimer = 0;
    }
    for (auto &p : _players)
        if (!p.isDead())
            tickPlayer(p);
}

void World::tickPlayer(Player &p)
{
    switch (p.tickFood()) {
    case Player::Hunger::Starved:
        p.kill();
        _observer.onPlayerDied(p);
        return;
    case Player::Hunger::Ate:
        _observer.onPlayerInventory(p);
        break;
    case Player::Hunger::Nothing:
        break;
    }

    if (!p.hasAction())
        return;
    // Frozen participants wait; only the player who started the incantation runs its clock.
    if (p.isIncanting() && p.currentAction().name != "Incantation")
        return;

    Action &action = p.currentAction();
    if (!action.started) {
        action.started = true;
        if (action.known && action.name == "Incantation" && !startIncantation(p)) {
            _observer.onReply(p.id(), "ko");
            p.finishAction();
            return;
        }
    }
    if (action.remainingTicks > 0)
        --action.remainingTicks;
    if (action.remainingTicks <= 0) {
        Action done = action;
        p.finishAction();
        execute(p, done);
    }
}

void World::execute(Player &p, const Action &a)
{
    std::string reply;
    if (!a.known)
        reply = "ko";
    else if (a.name == "Forward")
        reply = cmdForward(p);
    else if (a.name == "Right" || a.name == "Left")
        reply = cmdTurn(p, a.name == "Right");
    else if (a.name == "Look")
        reply = cmdLook(p);
    else if (a.name == "Inventory")
        reply = cmdInventory(p);
    else if (a.name == "Take")
        reply = cmdTake(p, a.arg);
    else if (a.name == "Set")
        reply = cmdSet(p, a.arg);
    else if (a.name == "Broadcast")
        reply = cmdBroadcast(p, a.arg);
    else if (a.name == "Connect_nbr")
        reply = cmdConnectNbr(p);
    else if (a.name == "Fork")
        reply = cmdFork(p);
    else if (a.name == "Eject")
        reply = cmdEject(p);
    else if (a.name == "Incantation")
        reply = endIncantation(p);
    _observer.onReply(p.id(), reply);
}

std::string World::cmdForward(Player &p)
{
    p.moveTo(_map.wrap(step(p.position(), p.orientation())));
    _observer.onPlayerMoved(p);
    return "ok";
}

std::string World::cmdTurn(Player &p, bool right)
{
    if (right)
        p.turnRight();
    else
        p.turnLeft();
    _observer.onPlayerMoved(p);
    return "ok";
}

std::string World::cmdLook(const Player &p) const
{
    // Row `dist` in front of the player, scanned from its left to its right.
    Position front = step({0, 0}, p.orientation());
    Position right = step({0, 0}, rightOf(p.orientation()));
    std::ostringstream oss;
    oss << "[";
    bool firstTile = true;
    for (int dist = 0; dist <= p.level(); ++dist) {
        for (int side = -dist; side <= dist; ++side) {
            if (!firstTile)
                oss << ",";
            firstTile = false;
            Position pos = _map.wrap({p.position().x + front.x * dist + right.x * side,
                                      p.position().y + front.y * dist + right.y * side});
            bool firstItem = true;
            auto item = [&](std::string_view word) {
                if (!firstItem)
                    oss << " ";
                oss << word;
                firstItem = false;
            };
            for (const auto &other : _players)
                if (!other.isDead() && other.position() == pos)
                    item("player");
            const Tile &tile = _map.tile(pos);
            for (Resource r : ALL_RESOURCES)
                for (int q = 0; q < tile.count(r); ++q)
                    item(name(r));
        }
    }
    oss << "]";
    return oss.str();
}

std::string World::cmdInventory(const Player &p) const
{
    std::ostringstream oss;
    oss << "[";
    for (Resource r : ALL_RESOURCES) {
        if (r != Resource::Food)
            oss << ", ";
        oss << name(r) << " " << p.count(r);
    }
    oss << "]";
    return oss.str();
}

std::string World::cmdTake(Player &p, const std::string &arg)
{
    auto r = parseResource(arg);
    if (!r || !_map.tile(p.position()).remove(*r))
        return "ko";
    p.addItem(*r);
    _observer.onResourceTaken(p, *r);
    _observer.onTileChanged(p.position());
    _observer.onPlayerInventory(p);
    return "ok";
}

std::string World::cmdSet(Player &p, const std::string &arg)
{
    auto r = parseResource(arg);
    if (!r || !p.removeItem(*r))
        return "ko";
    _map.tile(p.position()).add(*r);
    _observer.onResourceDropped(p, *r);
    _observer.onTileChanged(p.position());
    _observer.onPlayerInventory(p);
    return "ok";
}

std::string World::cmdBroadcast(Player &p, const std::string &message)
{
    for (const auto &other : _players) {
        if (other.id() == p.id() || other.isDead())
            continue;
        int k = soundDirection(p.position(), other, _map.width(), _map.height());
        _observer.onReply(other.id(), "message " + std::to_string(k) + ", " + message);
    }
    _observer.onBroadcast(p, message);
    return "ok";
}

std::string World::cmdConnectNbr(const Player &p) const
{
    return std::to_string(freeSlotsFor(p.team()));
}

std::string World::cmdFork(Player &p)
{
    _eggs.push_back({_nextEggId++, p.position(), p.team()});
    _observer.onEggLaid(p, _eggs.back());
    return "ok";
}

std::string World::cmdEject(Player &p)
{
    bool ejected = false;
    for (auto &other : _players) {
        if (other.id() == p.id() || other.isDead() || other.position() != p.position())
            continue;
        other.moveTo(_map.wrap(step(other.position(), p.orientation())));
        other.setIncanting(false);
        int k = soundDirection(p.position(), other, _map.width(), _map.height());
        _observer.onReply(other.id(), "eject: " + std::to_string(k));
        _observer.onPlayerExpelled(other);
        _observer.onPlayerMoved(other);
        ejected = true;
    }
    for (auto it = _eggs.begin(); it != _eggs.end();) {
        if (it->pos == p.position()) {
            _observer.onEggDestroyed(*it);
            it = _eggs.erase(it);
        } else {
            ++it;
        }
    }
    return ejected ? "ok" : "ko";
}

int World::playersReadyFor(const Player &p) const
{
    int n = 0;
    for (const auto &other : _players)
        if (!other.isDead() && other.position() == p.position() && other.level() == p.level())
            ++n;
    return n;
}

bool World::meetsElevation(const Player &p) const
{
    if (p.level() < rules::MIN_LEVEL || p.level() >= rules::MAX_LEVEL)
        return false;
    const auto &req = rules::elevation(p.level());
    if (playersReadyFor(p) < req.players)
        return false;
    const Tile &tile = _map.tile(p.position());
    for (Resource r : ALL_RESOURCES)
        if (tile.count(r) < req.stones[index(r)])
            return false;
    return true;
}

bool World::startIncantation(Player &p)
{
    if (!meetsElevation(p))
        return false;
    std::vector<int> ids;
    for (auto &other : _players) {
        if (other.isDead() || other.position() != p.position() || other.level() != p.level())
            continue;
        other.setIncanting(true);
        ids.push_back(other.id());
        _observer.onReply(other.id(), "Elevation underway");
    }
    _observer.onIncantationStart(p.position(), p.level(), ids);
    return true;
}

std::string World::endIncantation(Player &p)
{
    const Position pos = p.position();
    const int level = p.level();
    if (!meetsElevation(p)) {
        for (auto &other : _players)
            if (other.position() == pos && other.level() == level)
                other.setIncanting(false);
        _observer.onIncantationEnd(pos, false);
        return "ko";
    }

    Tile &tile = _map.tile(pos);
    const auto &req = rules::elevation(level);
    for (Resource r : ALL_RESOURCES)
        for (int q = 0; q < req.stones[index(r)]; ++q)
            tile.remove(r);

    for (auto &other : _players) {
        if (other.isDead() || other.position() != pos || other.level() != level)
            continue;
        other.levelUp();
        other.setIncanting(false);
        _observer.onPlayerLevel(other);
        if (other.id() != p.id())
            _observer.onReply(other.id(), "Current level: " + std::to_string(other.level()));
    }
    _observer.onIncantationEnd(pos, true);
    _observer.onTileChanged(pos);
    checkVictory();
    return "Current level: " + std::to_string(p.level());
}

void World::checkVictory()
{
    if (_gameOver)
        return;
    for (const auto &team : _config.teams) {
        int atMax = 0;
        for (const auto &p : _players)
            if (!p.isDead() && p.team() == team && p.level() == rules::MAX_LEVEL)
                ++atMax;
        if (atMax >= rules::WINNING_PLAYERS) {
            _gameOver = true;
            _observer.onGameWon(team);
            return;
        }
    }
}

} // namespace zappy
