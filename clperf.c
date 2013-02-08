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
	printf("%s INPUT LABELCOL PREDCOL\n",cmd);
}

int main(int argc, char **argv)
{
	int rc;
	int i;
	int err = -1;
	data_t *d = NULL;
	int nrows;
	int ncols;

	const char *filename = NULL;
	int label_col = -1;
	int pred_col = -1;
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
			else if (label_col == -1) label_col = atoi((argv[i]));
			else if (pred_col == -1) pred_col = atoi(argv[i]);
			else
			{
				fprintf(stderr,"%s: Too many arguments!\n",cmd);
				goto out;
			}
		}
	}

	if (!filename)
	{
		fprintf(stderr,"%s: No input file specified!\n",cmd);
		goto out;
	}

	if (label_col == -1)
	{
		fprintf(stderr,"%s: No label column specified\n",cmd);
		goto out;
	}

	if (pred_col == -1)
	{
		fprintf(stderr,"%s: No prediction column specified\n",cmd);
		goto out;
	}

	if ((err = data_create(&d)))
		goto out;

	if ((err = data_load_from_ascii(d,filename)))
	{
		fprintf(stderr,"Couldn't load \"%s\"\n",filename);
		goto out;
	}

	nrows = data_get_number_of_rows(d);
	ncols = data_get_number_of_columns(d);

	if (verbose)
		fprintf(stderr,"Read data frame with %d lines and %d columns\n",nrows,ncols);

	if (label_col < 0 || label_col >= ncols)
	{
		fprintf(stderr,"Specified label column out of bounds.\n");
		goto out;
	}

	if (pred_col < 0 || pred_col >= ncols)
	{
		fprintf(stderr,"Specified prediction column out of bounds.\n");
		goto out;
	}

	if ((err = data_stat(d,label_col,1,&pred_col)))
	{
		fprintf(stderr,"Couldn't determine stat\n");
		goto out;
	}
	rc = EXIT_SUCCESS;
out:
	if (d) data_free(d);
	return rc;
}
