/**
 * The clperf is a simple tool to analyze the performance
 * of classifiers using various measures. It is able to
 * deal with very large data files.
 *
 * @file clperf.c
 *
 * @author Sebastian Bauer <mail@sebastianbauer.info>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Displays usage.
 *
 * @param cmd
 */
static void usage(const char *cmd)
{
	printf("%s INPUT\n",cmd);
}

int main(int argc, char **argv)
{
	int rc;
	int i;
	const char *filename = NULL;

	const char *cmd;

	if (!(cmd = strrchr(argv[0], '/'))) cmd = argv[0];
	else cmd++;

	rc = EXIT_FAILURE;

	for (i=1;i<argc;i++)
	{
		if (!strcmp("--help",argv[i]) || !strcmp("-h",argv[i]))
		{
			usage(cmd);
			rc = EXIT_SUCCESS;
			goto out;
		}
	}

	if (!filename)
	{
		fprintf(stderr,"%s: No input file specified!\n",cmd);
		goto out;
	}

	rc = EXIT_SUCCESS;
out:
	return rc;
}
