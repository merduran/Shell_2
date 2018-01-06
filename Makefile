CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -D_GNU_SOURCE -std=gnu99 -Werror
PROMPT = -DPROMPT
EXECS = 33sh 33noprompt
.PHONY = all clean
all: $(EXECS)
33sh: 33sh.c jobs.c jobs.h
	$(CC) $(CFLAGS) $(PROMPT) $< -o $@
33noprompt: 33sh.c jobs.c jobs.h
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(EXECS)
