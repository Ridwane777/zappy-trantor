#include "net/RingBuffer.hpp"
#include "test_framework.hpp"
#include "util/Parse.hpp"
#include "Args.hpp"

using zappy::net::RingBuffer;

TEST(ringbuffer_extracts_lines_across_the_wrap)
{
    RingBuffer rb(8);
    CHECK_EQ(rb.write("abc\n", 4), 4u);
    std::string line;
    CHECK(rb.extractLine(line));
    CHECK_EQ(line, std::string("abc"));
    // head is now at 4: "defg\n" wraps around the end of the storage
    CHECK_EQ(rb.write("defg\n", 5), 5u);
    CHECK(rb.extractLine(line));
    CHECK_EQ(line, std::string("defg"));
    CHECK(rb.empty());
}

TEST(ringbuffer_never_grows_past_its_capacity)
{
    RingBuffer rb(4);
    CHECK_EQ(rb.write("123456", 6), 4u);
    CHECK(rb.full());
    CHECK(!rb.writeAll("x"));
    std::string line;
    CHECK(!rb.extractLine(line));   // no '\n' yet: nothing to extract
}

TEST(ringbuffer_keeps_partial_lines_and_strips_cr)
{
    RingBuffer rb(16);
    rb.write("tea", 3);
    std::string line;
    CHECK(!rb.extractLine(line));
    rb.write("m1\r\n", 4);
    CHECK(rb.extractLine(line));
    CHECK_EQ(line, std::string("team1"));
}

TEST(ringbuffer_contiguous_views_cover_all_data)
{
    RingBuffer rb(8);
    rb.write("abcdef", 6);
    rb.consume(5);
    rb.write("ghijk", 5);   // wraps
    std::string out;
    while (!rb.empty()) {
        out.append(rb.readPtr(), rb.contiguousReadable());
        rb.consume(rb.contiguousReadable());
    }
    CHECK_EQ(out, std::string("fghijk"));
}

TEST(parse_int_rejects_partial_and_empty_input)
{
    CHECK(zappy::parseInt("42") == 42);
    CHECK(zappy::parseInt("-3") == -3);
    CHECK(!zappy::parseInt(""));
    CHECK(!zappy::parseInt("abc"));
    CHECK(!zappy::parseInt("42x"));
}

namespace {

bool argsFail(std::vector<std::string> args)
{
    args.insert(args.begin(), "zappy_server");
    try {
        zappy::parseArgs(args);
    } catch (const zappy::ArgsError &) {
        return true;
    }
    return false;
}

} // namespace

TEST(args_have_defaults)
{
    auto c = zappy::parseArgs({"zappy_server"});
    CHECK_EQ(c.port, 4242);
    CHECK_EQ(c.width, 10);
    CHECK_EQ(c.height, 10);
    CHECK_EQ(c.clients, 3);
    CHECK_EQ(c.freq, 100);
    CHECK_EQ(c.teams.size(), 4u);
}

TEST(args_reject_invalid_values_and_teams)
{
    CHECK(argsFail({"-p", "abc"}));
    CHECK(argsFail({"-p", "70000"}));
    CHECK(argsFail({"-x", "0"}));
    CHECK(argsFail({"-c"}));
    CHECK(argsFail({"-n"}));
    CHECK(argsFail({"-n", "a", "a"}));
    CHECK(argsFail({"-n", ""}));
    CHECK(argsFail({"-n", "GRAPHIC"}));
    CHECK(argsFail({"-z"}));
    CHECK(!argsFail({"-p", "5000", "-n", "red", "blue", "-c", "2", "-f", "50"}));
}

TEST(args_clamp_oversized_world)
{
    auto c = zappy::parseArgs({"zappy_server", "-x", "100", "-y", "50"});
    CHECK_EQ(c.width, 42);
    CHECK_EQ(c.height, 42);
}
