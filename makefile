SRCS = $(wildcard *.c)

TEST_SRCS = $(wildcard tests/*.c)
TEST_EXES = $(patsubst %.c,%,$(TEST_SRCS))

VALGRIND = #valgrind --track-origins=yes

tests/%: tests/%.c $(SRCS)
	gcc -ggdb $< -o$@ -I.

all: tests

.PHONY: tests
tests: $(TEST_EXES)
	$(foreach TEST_EXE,$(TEST_EXES),$(VALGRIND) ./$(TEST_EXE)) && true

clean:
	rm -Rf $(TEST_EXES)