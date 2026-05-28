# Makefile racine — délègue à chaque composant.
#
# Règles disponibles :
#   all         : compile server + gui + ai
#   server      : compile uniquement le serveur
#   gui         : compile uniquement la GUI
#   ai          : prépare uniquement l'IA (install deps)
#   clean       : nettoie les objets de tous les composants
#   fclean      : supprime les binaires de tous les composants
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
	pip3 install -r ai/requirements.txt --quiet

clean:
	$(MAKE) -C server clean
	$(MAKE) -C gui clean

fclean: clean
	$(MAKE) -C server fclean
	$(MAKE) -C gui fclean

re: fclean all

.PHONY: all server gui ai clean fclean re
