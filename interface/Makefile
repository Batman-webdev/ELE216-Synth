NAME    = test_interface
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
INCLUDES = -I ../Unity/unity -I/usr/include/SDL2 -D_REENTRANT
LDLIBS  = -lm -lSDL2
LDFLAGS =

# Définition des liste de fichiers
SRC = $(NAME).c interface.c SDL2\SDL2.c \enveloppe\enveloppe.c \filtre\filtre.c \oscillateur\oscillateur.c \Unity\unity.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

.PHONY: all clean run distclean

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) $^ ../Unity/unity.c -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

run: all
	./$(NAME)

clean:
	rm -f $(OBJ) $(DEP)

distclean: clean
	rm -f $(NAME)

-include $(DEP)
