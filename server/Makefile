CXX      = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude -MMD -MP

NAME     = zappy_server
TEST     = unit_tests

LIB_SRCS = $(wildcard src/net/*.cpp src/game/*.cpp src/app/*.cpp) src/Args.cpp
SRCS     = src/main.cpp $(LIB_SRCS)
TEST_SRCS = $(wildcard tests/*.cpp) $(LIB_SRCS)

OBJS      = $(SRCS:%.cpp=build/%.o)
TEST_OBJS = $(TEST_SRCS:%.cpp=build/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

tests_run: $(TEST)
	./$(TEST)

clean:
	rm -rf build

fclean: clean
	rm -f $(NAME) $(TEST)

re: fclean all

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all clean fclean re tests_run
