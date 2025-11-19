PKGS=libws2811
PKG_CFLAGS=$(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lm
OBJS=args.o

%.o: %.c
	$(CC) $(PKG_CFLAGS) $(CFLAGS) -c $< -o $@

main: main.c $(OBJS)
	$(CC) $(PKG_CFLAGS) $(CFLAGS) $(LIBS) $(OBJS) main.c -o main

all:	main

reinstall:
	sudo service led-alarm stop
	sudo systemctl daemon-reload
	$(MAKE) clean
	$(MAKE) all
	sudo service led-alarm start
	sudo service led-alarm status

clean:
	rm -fv main *.o
