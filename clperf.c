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

#include "support.h"

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
	int err = -1;
	data_t *d = NULL;

	const char *filename = NULL;
	int verbose = 0;

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

		if (!strcmp("--verbose",argv[i]))
		{
			verbose = 1;
		} else if (argv[i][0] == '-')
		{
			fprintf(stderr,"%s: Unknown option \"%s\"",filename,argv[i]);
			goto out;
		} else
		{
			if (!filename) filename = argv[i];
			else
			{
				fprintf(stderr,"%s: More than one filename arguments given!\n",cmd);
				goto out;
			}
		}
	}

	if (!filename)
	{
		fprintf(stderr,"%s: No input file specified!\n",cmd);
		goto out;
	}

	{
		if ((err = data_create(&d)))
			goto out;

		if ((err = data_load_from_ascii(d,filename)))
		{
			fprintf(stderr,"Couldn't load \"%s\"\n",filename);
			goto out;
		}

		if (verbose)
		{
//			fprintf(stderr,"Read data frame with %d lines and %d columns\n",);
		}
	}
	rc = EXIT_SUCCESS;
out:
	if (d) data_free(d);
	return rc;
}
