CC=gcc
SRCDIR1=.
SRCDIR2=../src
OBJDIR=obj
BINDIR=bin
BIN=test-gif
CFLAGS=-I../include
LDFLAGS=
SRCS:=$(notdir $(shell find $(SRCDIR1) -name '*.c'))
SRCS+=$(notdir $(shell find $(SRCDIR2) -name '*.c'))
OBJS:=$(SRCS:%.c=%.o)
VPATH=$(SRCDIR1):$(SRCDIR2)

ifdef DEBUG
CFLAGS+=-g -D DEBUG
endif

$(BIN): $(OBJS)
	$(CC) $(addprefix $(OBJDIR)/, $(OBJS)) -o $(BINDIR)/$(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(OBJDIR)/$@

.PHONY: clean
clean:
	rm $(OBJDIR)/*.o $(BINDIR)/$(BIN)

