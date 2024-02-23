src = $(wildcard src/*.c)
obj = $(src:.c=.o)

CCFLAGS = -pthread -Wall -O2

uk101re: $(obj)
	$(CC) $(CCFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(obj) uk101re
