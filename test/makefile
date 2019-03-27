CC=gcc
INCLUDE=-I..\include
SRCDIR=..\src
BINDIR=bin
OBJDIR=obj
BIN=gif-test
OBJS="$(OBJ)\main_win.o" "$(OBJ)\buffer.o" "$(OBJ)\gif.o"
CFLAGS=$(INCLUDE)

ifdef DEBUG
CFLAGS+=-g
endif

LDFLAGS=-mwindows

$(BIN): main_win.o buffer.o gif.o
	$(CC) $(LDFLAGS) $(OBJS) -o "$(BINDIR)\$(BIN)"

main_win.o:
	$(CC) $(CFLAGS) -c "main_win.c" -o "$(OBJ)\main_win.o"

buffer.o:
	$(CC) $(CFLAGS) -c "$(SRCDIR)\buffer.c" -o "$(OBJ)\buffer.o"

gif.o:
	$(CC) $(CFLAGS) -c "$(SRCDIR)\gif.c" -o "$(OBJ)\gif.o"

clean:
	del /F /Q "$(OBJ)\*.*" "$(BINDIR)\$(BIN)"
