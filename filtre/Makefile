
NAME    = filtre_test
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
INCLUDES = -I ../Unity/unity
LDLIBS  = -lm
LDFLAGS =

# Définition des liste de fichiers
SRC = filtre_test.c filtre.c ../Unity/unity.c
OBJ = filtre_test.o filtre.o
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
