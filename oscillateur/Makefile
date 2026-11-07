NAME     = oscillateur_test
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g
INCLUDES = -I../Unity
LDLIBS   = -lm
LDFLAGS  =



SRC = $(NAME).c oscillateur.c ../Unity/unity.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

.PHONY: all clean distclean run re

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

re: distclean all

-include $(DEP)