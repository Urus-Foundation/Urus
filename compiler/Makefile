CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Iinclude
TARGET = urusc
SRCDIR = src
SRCS = $(SRCDIR)/main.c $(SRCDIR)/lexer.c $(SRCDIR)/ast.c $(SRCDIR)/parser.c $(SRCDIR)/util.c $(SRCDIR)/sema.c $(SRCDIR)/codegen.c $(SRCDIR)/linter.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
