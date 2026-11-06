# The computer's builds of the engine: the score renderer (bc_host) and the stress tests (stress).
ENGINE = ../engine/noladrift.bronzechoir
# The Opus 2 beta 4 plugin kit (core 2.30), outside the repository: make OPUS_KIT=/path/to/opus2-plugin-kit
OPUS_KIT ?= ../../opus2-plugin-kit
SDK    = $(OPUS_KIT)/sdk
CFLAGS = -O2 -Wall -Wextra -Werror -I$(ENGINE) -I$(SDK) -I.
SRC    = $(ENGINE)/bronzechoir.c $(ENGINE)/bc_dsp.c $(ENGINE)/bc_page.c $(ENGINE)/bc_fw.c os_sim.c

all: bc_host stress

bc_host: bc_host.c $(SRC) os_sim.h
	$(CC) $(CFLAGS) -o $@ bc_host.c $(SRC) -lm

stress: stress.c $(SRC) os_sim.h
	$(CC) $(CFLAGS) -o $@ stress.c $(SRC) -lm

clean:
	rm -f bc_host stress
