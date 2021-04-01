TARGETS=shell

CC_C=gcc

all: clean $(TARGETS)

$(TARGETS):
	$(CC_C) $(CFLAGS) $@.c -o $@

clean:
	rm -f $(TARGETS)

test: all
	./$(TARGETS)