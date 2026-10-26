# Root Makefile - delegates to each component.
#
# Available rules:
#   all         : build server + gui
#   server      : build the server only
#   gui         : build the GUI only
#   ai          : check that zappy_ai is present (Python script, nothing to build)
#   clean       : remove object files of all components
#   fclean      : remove binaries of all components
#   re          : fclean + all

NAME_SERVER = zappy_server
NAME_GUI    = zappy_gui
NAME_AI     = zappy_ai

all: server gui ai

server:
	$(MAKE) -C server

gui:
	$(MAKE) -C gui

ai:
	@test -f ai/$(NAME_AI) || (echo "ERROR: ai/$(NAME_AI) not found" && exit 1)
	@echo "ai/$(NAME_AI) OK"

clean:
	$(MAKE) -C server clean
	$(MAKE) -C gui clean

fclean: clean
	$(MAKE) -C server fclean
	$(MAKE) -C gui fclean

re: fclean all

.PHONY: all server gui ai clean fclean re
