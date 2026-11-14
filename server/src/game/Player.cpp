#include "game/Player.hpp"
#include <utility>

namespace zappy {

Player::Player(int id, std::string team, Position pos, Orientation orientation)
    : _id(id), _team(std::move(team)), _pos(pos), _orientation(orientation)
{
    _inventory[index(Resource::Food)] = rules::START_FOOD;
}

void Player::turnRight()
{
    _orientation = static_cast<Orientation>(static_cast<int>(_orientation) % 4 + 1);
}

void Player::turnLeft()
{
    _orientation = static_cast<Orientation>((static_cast<int>(_orientation) + 2) % 4 + 1);
}

bool Player::removeItem(Resource r)
{
    int &n = _inventory[index(r)];
    if (n <= 0)
        return false;
    --n;
    return true;
}

void Player::levelUp()
{
    if (_level < rules::MAX_LEVEL)
        ++_level;
}

Player::Hunger Player::tickFood()
{
    --_foodTimer;
    if (_foodTimer > 0 || _incanting)
        return Hunger::Nothing;
    if (!removeItem(Resource::Food))
        return Hunger::Starved;
    _foodTimer = rules::FOOD_DURATION;
    return Hunger::Ate;
}

bool Player::enqueue(Action action)
{
    if (_actions.size() >= static_cast<std::size_t>(rules::MAX_PENDING_COMMANDS))
        return false;
    _actions.push_back(std::move(action));
    return true;
}

} // namespace zappy
