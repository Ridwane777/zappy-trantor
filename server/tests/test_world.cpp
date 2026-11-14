#include "app/GameSession.hpp"
#include "game/World.hpp"
#include "test_framework.hpp"
#include <map>
#include <set>

using namespace zappy;

namespace {

// Records what the world reports, for assertions.
class RecordingObserver : public IWorldObserver {
public:
    std::map<int, std::vector<std::string>> replies;
    std::vector<std::string> events;

    void onReply(int id, const std::string &line) override { replies[id].push_back(line); }
    void onPlayerDied(const Player &) override { events.push_back("died"); }
    void onPlayerJoined(const Player &) override { events.push_back("joined"); }
    void onPlayerMoved(const Player &) override { events.push_back("moved"); }
    void onPlayerLevel(const Player &) override { events.push_back("level"); }
    void onPlayerInventory(const Player &) override {}
    void onPlayerLeft(const Player &) override { events.push_back("left"); }
    void onResourceTaken(const Player &, Resource) override { events.push_back("taken"); }
    void onResourceDropped(const Player &, Resource) override { events.push_back("dropped"); }
    void onTileChanged(Position) override {}
    void onPlayerExpelled(const Player &) override { events.push_back("expelled"); }
    void onBroadcast(const Player &, const std::string &) override { events.push_back("broadcast"); }
    void onIncantationStart(Position, int, const std::vector<int> &) override { events.push_back("pic"); }
    void onIncantationEnd(Position, bool ok) override { events.push_back(ok ? "pie1" : "pie0"); }
    void onEggLaid(const Player &, const Egg &) override { events.push_back("egg"); }
    void onEggHatched(const Egg &) override { events.push_back("hatched"); }
    void onEggDestroyed(const Egg &) override { events.push_back("eggdied"); }
    void onGameWon(const std::string &team) override { events.push_back("won " + team); }

    std::string last(int id) const
    {
        auto it = replies.find(id);
        return it == replies.end() || it->second.empty() ? "" : it->second.back();
    }
};

WorldConfig config(unsigned seed = 1, int slots = 2)
{
    return {10, 10, {"red", "blue"}, slots, seed};
}

} // namespace

TEST(world_join_uses_slots_then_refuses)
{
    RecordingObserver obs;
    World w(config(1, 2), obs);
    CHECK(!w.join("nobody"));
    auto a = w.join("red");
    CHECK(a && a->freeSlots == 1);
    auto b = w.join("red");
    CHECK(b && b->freeSlots == 0);
    CHECK(!w.join("red"));
}

TEST(world_replies_in_order_and_unknown_command_is_ko)
{
    RecordingObserver obs;
    World w(config(), obs);
    int id = w.join("red")->playerId;
    CHECK(w.queueCommand(id, "Inventory"));
    CHECK(w.queueCommand(id, "Dance"));
    CHECK(w.queueCommand(id, "Forward extra-argument"));
    w.update(20);
    CHECK_EQ(obs.replies[id].size(), 3u);
    CHECK_EQ(obs.replies[id][0].substr(0, 7), std::string("[food 1"));
    CHECK_EQ(obs.replies[id][1], std::string("ko"));
    CHECK_EQ(obs.replies[id][2], std::string("ko"));
}

TEST(world_queue_holds_at_most_ten_commands)
{
    RecordingObserver obs;
    World w(config(), obs);
    int id = w.join("red")->playerId;
    for (int i = 0; i < 10; ++i)
        CHECK(w.queueCommand(id, "Look"));
    CHECK(!w.queueCommand(id, "Look"));
}

TEST(world_forward_wraps_around_the_map)
{
    RecordingObserver obs;
    World w(config(), obs);
    int id = w.join("red")->playerId;
    Position start = w.findPlayer(id)->position();
    for (int i = 0; i < 10; ++i)
        w.queueCommand(id, "Forward");
    w.update(7 * 10);
    CHECK(w.findPlayer(id)->position() == start);   // 10 steps north on a 10-high torus
}

TEST(world_set_then_take_moves_one_item)
{
    RecordingObserver obs;
    World w(config(), obs);
    int id = w.join("red")->playerId;
    w.queueCommand(id, "Set food");
    w.queueCommand(id, "Take food");
    w.queueCommand(id, "Take unobtainium");
    w.update(21);
    CHECK_EQ(obs.replies[id][0], std::string("ok"));
    CHECK_EQ(obs.replies[id][1], std::string("ok"));
    CHECK_EQ(obs.replies[id][2], std::string("ko"));
    CHECK_EQ(w.findPlayer(id)->count(Resource::Food), 10);
}

TEST(world_player_starves_after_ten_food_units)
{
    RecordingObserver obs;
    World w(config(), obs);
    int id = w.join("red")->playerId;
    w.update(rules::FOOD_DURATION * 11 - 1);
    CHECK(!w.findPlayer(id)->isDead());
    w.update(1);
    CHECK(w.findPlayer(id)->isDead());
}

TEST(world_fork_adds_a_slot_and_the_egg_hatches_in_place)
{
    RecordingObserver obs;
    World w(config(1, 1), obs);
    int id = w.join("red")->playerId;
    w.queueCommand(id, "Fork");
    w.queueCommand(id, "Connect_nbr");
    w.update(43);   // Fork ends at unit 42, Connect_nbr (cost 0) runs on the next unit
    CHECK_EQ(obs.replies[id][0], std::string("ok"));
    CHECK_EQ(obs.replies[id][1], std::string("1"));
    Position egg = w.eggs().at(0).pos;
    auto child = w.join("red");
    CHECK(child.has_value());
    CHECK(w.findPlayer(child->playerId)->position() == egg);
    CHECK(w.eggs().empty());
}

TEST(world_solo_incantation_reaches_level_two)
{
    // Find a seed where the first player spawns on a linemate.
    for (unsigned seed = 1; seed < 2000; ++seed) {
        RecordingObserver obs;
        World w(config(seed), obs);
        int id = w.join("red")->playerId;
        const Player *p = w.findPlayer(id);
        if (w.map().tile(p->position()).count(Resource::Linemate) == 0)
            continue;
        w.queueCommand(id, "Incantation");
        w.update(1);
        CHECK_EQ(obs.last(id), std::string("Elevation underway"));
        w.update(rules::COMMANDS.back().cost);
        CHECK_EQ(obs.last(id), std::string("Current level: 2"));
        CHECK_EQ(w.findPlayer(id)->level(), 2);
        return;
    }
    CHECK(!"no seed puts a linemate under the first player");
}

TEST(world_incantation_without_stones_is_ko)
{
    for (unsigned seed = 1; seed < 2000; ++seed) {
        RecordingObserver obs;
        World w(config(seed), obs);
        int id = w.join("red")->playerId;
        if (w.map().tile(w.findPlayer(id)->position()).count(Resource::Linemate) > 0)
            continue;
        w.queueCommand(id, "Incantation");
        w.update(1);
        CHECK_EQ(obs.last(id), std::string("ko"));
        return;
    }
}

TEST(sound_direction_is_relative_to_the_receiver)
{
    Player north(1, "t", {5, 5}, Orientation::North);
    CHECK_EQ(soundDirection({5, 5}, north, 10, 10), 0);
    CHECK_EQ(soundDirection({5, 3}, north, 10, 10), 1);   // ahead
    CHECK_EQ(soundDirection({7, 5}, north, 10, 10), 3);   // right
    CHECK_EQ(soundDirection({5, 7}, north, 10, 10), 5);   // behind
    CHECK_EQ(soundDirection({3, 5}, north, 10, 10), 7);   // left
    Player edge(2, "t", {0, 5}, Orientation::North);
    CHECK_EQ(soundDirection({9, 5}, edge, 10, 10), 7);    // shortest way crosses the edge
    Player east(3, "t", {5, 5}, Orientation::East);
    CHECK_EQ(soundDirection({7, 5}, east, 10, 10), 1);
}

// ----- application layer, with a fake network -------------------------------------

namespace {

class FakeSink : public net::IClientSink {
public:
    std::map<net::ConnectionId, std::string> sent;
    std::set<net::ConnectionId> closed;
    void send(net::ConnectionId id, const std::string &text) override { sent[id] += text; }
    void closeAfterFlush(net::ConnectionId id) override { closed.insert(id); }
};

struct Session {
    FakeSink sink;
    app::GameSession session{sink, 100};
    World world{config(1, 2), session};
    Session() { session.attach(world); }
};

} // namespace

TEST(session_handshake_for_ai_and_unknown_team)
{
    Session s;
    s.session.onConnect(1);
    CHECK_EQ(s.sink.sent[1], std::string("WELCOME\n"));
    s.session.onLine(1, "red");
    CHECK_EQ(s.sink.sent[1], std::string("WELCOME\n1\n10 10\n"));
    s.session.onConnect(2);
    s.session.onLine(2, "purple");
    CHECK_EQ(s.sink.sent[2], std::string("WELCOME\nko\n"));
    CHECK(s.sink.closed.count(2) == 1);
}

TEST(session_gui_errors_use_sbp_and_suc)
{
    Session s;
    s.session.onConnect(1);
    s.session.onLine(1, "GRAPHIC");
    CHECK(s.sink.sent[1].find("msz 10 10\n") != std::string::npos);
    s.sink.sent[1].clear();
    s.session.onLine(1, "ppo #999");
    s.session.onLine(1, "ppo abc");
    s.session.onLine(1, "bct 99 0");
    s.session.onLine(1, "sst 0");
    s.session.onLine(1, "dance");
    CHECK_EQ(s.sink.sent[1], std::string("sbp\nsbp\nsbp\nsbp\nsuc\n"));
    s.sink.sent[1].clear();
    s.session.onLine(1, "sst 50");
    CHECK_EQ(s.sink.sent[1], std::string("sst 50\nsmg time unit set to 50\n"));
}

TEST(session_reports_new_players_to_gui)
{
    Session s;
    s.session.onConnect(1);
    s.session.onLine(1, "GRAPHIC");
    s.sink.sent[1].clear();
    s.session.onConnect(2);
    s.session.onLine(2, "red");
    CHECK(s.sink.sent[1].rfind("pnw #1 ", 0) == 0);
}
