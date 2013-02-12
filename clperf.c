/**
 * The clperf is a simple tool to analyze the performance
 * of classifiers using various measures. It is able to
 * deal with very large data files.
 *
 * @file clperf.c
 *
 * @author Sebastian Bauer <mail@sebastianbauer.info>
 */

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support.h"
#include "version.h"

/**
 * Check if the arg of the given position matches the given arg and
 * return the value in *value on existence.
 *
 * @param argc same as in main()
 * @param argv same as in main()
 * @param argpos the current position. May be incremented.
 * @param arg name of the arg
 * @param value where to store the value
 * @return 0, if no match, 1 otherwise.
 */
static int getarg(int argc, char **argv, int *argpos, char *arg, const char **value)
{
	int arglen;

	if (strncmp(argv[*argpos],arg,strlen(arg)))
		return 0;

	arglen = strlen(arg);
	if (argv[*argpos][arglen]=='=')
	{
		*value = &argv[*argpos][arglen+1];
	} else
	{
		if (*argpos + 1 == argc)
			return 0;
		*argpos = *argpos + 1;
		*value = argv[*argpos];
	}

	return 1;
}

/**
 * Displays usage.
 *
 * @param cmd
 */
static void usage(const char *cmd)
{
	printf(
			"Usage: %s [OPTION] INPUT LABELCOL PREDCOL\n"
			"Determines the performance of a classification result that\n"
			"was stored in a tabular ASCII file.\n"
			"Available options are:\n"
			"--help            show this help\n"
			"--output-format   how the output should look like. Supported\n"
			"                  values: Rscript (default)\n"
			"--no-sampling     disable sampling\n"
			"--verbose         verbose output during progress\n"
			"--version         shows the version number\n"
			"", cmd);
}

static int clperf_write_data_for_R(FILE *f, data_t *d, const char *var_prefix, const int breaks, int (*function_of_x)(double *, data_t *, double))
{
	int j;
	int err = -1;
	fprintf(f, "%sx<-c(",var_prefix);
	for (j = 0; j < breaks; j++) {
		double x = ((double) j) / (breaks + 1);
		fprintf(f, "%g%s", x, (j == breaks - 1) ? "" : ",");
	}
	fprintf(f, ")\n%sy<-c(",var_prefix);
	for (j = 0; j < breaks; j++) {
		double x = ((double) j) / (breaks + 1);
		double y;

		if ((err = function_of_x(&y, d, x)))
			goto out;
		fprintf(f, "%g%s", y, (j == breaks - 1) ? "" : ",");
	}
	fprintf(f, ")\n");
	err = 0;
out:
	return err;
}

static int clperf_stat_print_callback(uint32_t ps, uint32_t ns, uint32_t tps, uint32_t fps, void *userdata)
{
	double tpr = (double)tps / ps; /* true positive rate */
	double fpr = (double)fps / ns; /* false positive rate */
	double prec = (double)tps / (tps + fps); /* precision = true positives / (number of all positives = (true positives + false positives) */
	double recall = (double)tps / ps; /* recall = number of true positives / (true positives + false negatives = all positive samples) */

	fprintf(stdout,"%lf %lf %lf %lf\n",tpr,fpr,prec,recall);
	return 0;
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
	const char *output_format = NULL;
	int label_col = INT_MIN;
	int pred_col = INT_MIN;
	int verbose = 0;
	int sampling = 1;

	const char *cmd;

	if (!(cmd = strrchr(argv[0], '/'))) cmd = argv[0];
	else cmd++;

	rc = EXIT_FAILURE;

	for (i=1;i<argc;i++)
	{
		if (getarg(argc,argv,&i,"--output-format",&output_format)) continue;

		if (!strcmp("--help",argv[i]) || !strcmp("-h",argv[i]))
		{
			usage(cmd);
			rc = EXIT_SUCCESS;
			goto out;
		}

		if (!strcmp("--version",argv[i]))
		{
			printf("clperf %s\n",CLPERF_VERSION);
			goto out;
		} else if (!strcmp("--verbose",argv[i]))
		{
			verbose = 1;
		} else if (!strcmp("--no-sampling",argv[i]))
		{
			sampling = 0;
		} else if (argv[i][0] == '-' && !isdigit((unsigned char)argv[i][1]))
		{
			fprintf(stderr,"%s: Unknown option \"%s\"",filename,argv[i]);
			goto out;
		} else
		{
			if (!filename) filename = argv[i];
			else if (label_col == INT_MIN) label_col = atoi((argv[i]));
			else if (pred_col == INT_MIN) pred_col = atoi(argv[i]);
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

	if (!output_format)
		output_format = "Rscript";

	if (strcmp(output_format,"Rscript"))
	{
		fprintf(stderr,"%s: Unknown output format \"%s\"\n",cmd,output_format);
		goto out;
	}

	if (label_col == INT_MIN)
	{
		fprintf(stderr,"%s: No label column specified\n",cmd);
		goto out;
	}

	if (pred_col == INT_MIN)
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

	if (abs(pred_col) >= ncols)
	{
		fprintf(stderr,"Specified prediction column out of bounds.\n");
		goto out;
	}

	if (sampling)
	{
		const int breaks = 1001;
		if ((err = data_stat_hist(d,breaks,label_col,1,&pred_col)))
		{
			fprintf(stderr,"Couldn't determine stat\n");
			goto out;
		}

		if (!strcmp("Rscript",output_format))
		{
			fprintf(stdout,"#/usr/bin/Rscript --vanilla\n");
			clperf_write_data_for_R(stdout, d, "roc.", breaks, data_get_tpr_by_fpr);
			clperf_write_data_for_R(stdout, d, "precall.", breaks, data_get_precision_by_recall);
			fprintf(stdout,"pdf(width=10,height=5)\n");
			fprintf(stdout,"par(mfrow=c(1,2))\n");
			fprintf(stdout,"plot(main=\"ROC\",roc.x,roc.y,xlab=\"False positive rate\",ylab=\"True positive rate\",xlim=c(0,1),ylim=c(0,1))\n");
			fprintf(stdout,"plot(main=\"Precision/Recall\",precall.x,precall.y,xlab=\"Recall\",ylab=\"Precision\",xlim=c(0,1),ylim=c(0,1))\n");
			fprintf(stdout,"dev.off()\n");
		}
	} else
	{
		if ((err = data_stat_callback(d, clperf_stat_print_callback, NULL, label_col, 1, &pred_col)))
			goto out;
	}

	rc = EXIT_SUCCESS;
out:
	if (d) data_free(d);
	return rc;
}
