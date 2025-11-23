PKGS=libws2811 libconfig
PKG_CFLAGS=$(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lm
OBJS=args.o config.o

%.o: %.c
	$(CC) $(PKG_CFLAGS) $(CFLAGS) -c $< -o $@

main: main.c $(OBJS)
	$(CC) $(PKG_CFLAGS) $(CFLAGS) $(LIBS) $(OBJS) main.c -o main

all:	main

clean:
	rm -fv main *.o
