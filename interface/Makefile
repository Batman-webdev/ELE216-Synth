NAME    = demo_interface
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
INCLUDES = -I ../Unity/unity $(shell pkg-config --cflags sdl2 SDL2_ttf)
LDLIBS  = -lm $(shell pkg-config --libs sdl2 SDL2_ttf)
LDFLAGS =

# Définition des liste de fichiers
SRC = $(NAME).c interface.c ../enveloppe/enveloppe.c ../filtre/filtre.c ../oscillateur/oscillateur.c 
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

.PHONY: all clean run distclean

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

run: all
	./$(NAME)

clean:
	rm -f $(OBJ) $(DEP)

distclean: clean
	rm -f $(NAME)

-include $(DEP)
