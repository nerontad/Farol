TARGET = farol
OBJS = src/main.o src/gfx.o src/input.o src/game.o src/save.o src/savedata.o src/ui.o src/synth.o src/audio.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
# make AUTOTEST=1 genera una version que se juega sola y guarda capturas (ver src/autotest.h)
#   AUTOTEST=1 partida completa; =2 se detiene en la primera tienda; =3 continua la partida guardada;
#   =4 gana todas las rondas para llegar a la victoria y al modo infinito
ifdef AUTOTEST
CFLAGS += -DAUTOTEST=$(AUTOTEST)
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
# psputility (datos guardados) ya la enlaza build.mak; repetirla aqui desordena los stubs
LIBS = -lpspgu -lpspaudiolib -lpspaudio -lm

BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = FAROL

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
