NAME    = controle
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -g

SRC = $(NAME).c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

.PHONY: all clean run distclean

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./$(NAME) ../partitions/melodie1.txt

clean:
	rm -f $(OBJ) $(DEP)

distclean: clean
	rm -f $(NAME)

-include $(DEP)