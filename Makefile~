CC = gcc
CFLAGS = -Iinclude -std=c11 -Wall -Wextra -g
LIBS = -pthread -lm

SRC = $(wildcard src/*.c) 
LIBSRC = $(filter-out src/main.c,$(wildcard src/*.c))
# object files go into build/obj so they don't pollute root
OBJDIR = build/obj
OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SRC)))
LIBOBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(LIBSRC)))

BIN = bin/color_reducer
# TESTS = tests/test_person.c

.PHONY: all app test clean

all: app

# build application: link all object files
app: $(BIN)

# build static lib from library object files
libcolor.a: $(LIBOBJS)
	@echo "ar rcs $@ $^"
	@ar rcs $@ $^

# compile objects into build/obj
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

# app linking: link object files (including the glad object)
$(BIN): $(OBJS)
	@mkdir -p bin
	$(CC) $(OBJS) -o $@ $(LIBS)

#test: libfamily.a tests/test_person.c
#	@mkdir -p build
#	$(CC) $(CFLAGS) $(TESTS) libfamily.a -o build/test_person $(LIBS)
#	./build/test_person

clean:
	rm -rf $(OBJDIR) libcolor.a bin build
