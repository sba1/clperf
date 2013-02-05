/*
 * Minimal unit testing framework based on
 *  http://www.jera.com/techinfo/jtns/jtn002.html
 */
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define mu_assert(test) \
	do { if (!(test)) return #test ": "__FILE__"/" STRINGIZE(__LINE__); } while (0)

#define mu_run_test(test) \
	do { char *message = test(); tests_run++; \
	if (message) return message; } while (0)

extern int tests_run;
