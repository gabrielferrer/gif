CC=gcc
INCLUDE=-I..\..\common\system\include -I..\include
SRCDIR=..\src
SYSDIR=..\..\common\system\src
BINDIR=bin
OBJDIR=obj
BIN=gif-test
OBJ=main_win.o buffer.o gif.o sys.o
OBJS=$(OBJDIR)\main_win.o $(OBJDIR)\buffer.o $(OBJDIR)\gif.o $(OBJDIR)\sys.o
CFLAGS=$(INCLUDE)

ifdef DEBUG
CFLAGS+=-g
endif

LDFLAGS=-mwindows

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJS) -o "$(BINDIR)\$(BIN)"

main_win.o:
	$(CC) $(CFLAGS) -c main_win.c -o "$(OBJDIR)\main_win.o"

buffer.o:
	$(CC) $(CFLAGS) -c "$(SRCDIR)\buffer.c" -o "$(OBJDIR)\buffer.o"

gif.o:
	$(CC) $(CFLAGS) -c "$(SRCDIR)\gif.c" -o "$(OBJDIR)\gif.o"
	
sys.o:
	$(CC) $(CFLAGS) -c "$(SYSDIR)\sys.c" -o "$(OBJDIR)\sys.o"

clean:
	del /F /Q "$(OBJDIR)\*.*" "$(BINDIR)\$(BIN)"
