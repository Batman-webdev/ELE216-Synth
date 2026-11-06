
NAME    = enveloppe_test
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
INCLUDES = -I ../Unity/unity
LDLIBS  = -lm
LDFLAGS =

# Définition des liste de fichiers
SRC = enveloppe_test.c enveloppe.c ../Unity/unity.c
OBJ = enveloppe_test.o enveloppe.o
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
