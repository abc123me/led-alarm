OBJS=
CC=gcc

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main: main.c
	$(CC) $(CFLAGS)  $(LIBS) $(OBJS) -I rpi_ws281x/ main.c -o main rpi_ws281x/build/libws2811.a -lm

all:	main

clean:
	rm -fv main *.o
