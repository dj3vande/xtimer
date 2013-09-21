CFLAGS=-W -Wall -ansi -pedantic -O
#LDFLAGS=-L/usr/X11R6/lib -L/opt/local/lib -lX11 -lXaw3d -lXt
LDFLAGS=-L/usr/X11R6/lib -lX11 -lXaw -lXt

all: timer

timer: timer.o
