CC				= gcc
#CFLAGS			= -c -std=c99 -Wall -pedantic -g -O3 -fomit-frame-pointer -march=native
CFLAGS			= -c -std=c99 -Wall -pedantic -g -O3 -march=native -pg
SDLCFLAGS		= `sdl-config --cflags`
LDFLAGS			= -pg
SDLLDFLAGS		= `sdl-config --libs`
OBJS			= $(patsubst %.c,%.o,$(wildcard *.c))
RM				= rm -f
MAKE			= make
TARGET			= gbem

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(SDLLDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(SDLCFLAGS) -c $<

all: $(TARGET)

clean:
	$(RM) *.o core 

distclean:
	$(MAKE) clean
	$(RM) $(TARGET)
