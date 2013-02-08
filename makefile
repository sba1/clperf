SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

TEST_SRCS = $(wildcard tests/*.c)
TEST_EXES = $(patsubst %.c,%,$(TEST_SRCS))

CFLAGS = -c -ggdb -I.
VALGRIND = valgrind --track-origins=yes --leak-check=full --show-reachable=yes

tests/%: tests/%.c $(SRCS)
	gcc $(CFLAGS) $< -o $@

%.o: %.c
	gcc $(CFLAGS) $< -o $@

all: clperf tests

clperf: $(OBJS)
	gcc $(OBJS) -o $@

.PHONY: tests
tests: $(TEST_EXES)
	$(foreach TEST_EXE,$(TEST_EXES),$(VALGRIND) ./$(TEST_EXE)) && true

clean:
	rm -Rf clperf $(OBJS) $(TEST_EXES)
