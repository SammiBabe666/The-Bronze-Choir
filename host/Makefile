# The computer's builds of the engine: the score renderer (bc_host) and the stress tests (stress).
ENGINE = ../engine/noladrift.bronzechoir
SDK    = ../third_party/opus-plugin-kit/sdk
CFLAGS = -O2 -Wall -Wextra -Werror -I$(ENGINE) -I$(SDK) -I.
SRC    = $(ENGINE)/bronzechoir.c $(ENGINE)/bc_dsp.c $(ENGINE)/bc_page.c $(ENGINE)/bc_fw.c os_sim.c

all: bc_host stress

bc_host: bc_host.c $(SRC) os_sim.h
	$(CC) $(CFLAGS) -o $@ bc_host.c $(SRC) -lm

stress: stress.c $(SRC) os_sim.h
	$(CC) $(CFLAGS) -o $@ stress.c $(SRC) -lm

clean:
	rm -f bc_host stress
