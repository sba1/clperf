#include <stdlib.h>
#include <stdio.h>

#include "minunit.h"
#include "support.c"

int tests_run;

/**
 * Helper to insert some data.
 *
 * @param d
 * @return
 */
static char *helper_insert_and_assert_data(data_t *d)
{
	int i;

	double dv;
	int32_t iv;

	mu_assert(!data_set_number_of_columns(d,6));
	mu_assert(d->num_columns == 6);
	data_set_column_datatype(d,0,INT32);
	data_set_column_datatype(d,1,DOUBLE);
	data_set_column_datatype(d,2,DOUBLE);
	data_set_column_datatype(d,3,INT32);
	data_set_column_datatype(d,4,INT32);
	data_set_column_datatype(d,5,INT32);
	mu_assert(data_sizeof_columns_and_set_column_offsets(d)==32);
	mu_assert(d->column_offsets[0] == 0);
	mu_assert(d->column_offsets[1] == 4);
	mu_assert(d->column_offsets[2] == 12);
	mu_assert(d->column_offsets[3] == 20);
	mu_assert(d->column_offsets[4] == 24);
	mu_assert(d->column_offsets[5] == 28);
	mu_assert(d->num_rows == 0);

	mu_assert(!data_insert_row_v(d, 0, 0.11, 0.12, 3, 0, 0));
	mu_assert(d->num_rows == 1);
	mu_assert(!data_insert_row_v(d, 0, 0.24, 0.11, 5, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.14, 0.43, 4, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.33, 0.56, 6, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.45, 0.44, 7, 0, 0));
	mu_assert(!data_insert_row_v(d, 1, 0.68, 0.49, 11, 0, 0));
	mu_assert(!data_insert_row_v(d, 1, 0.58, 0.59, 9, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.59, 0.68, 10, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.51, 0.42, 8, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.09, 0.09, 2, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.08, 0.08, 1, 0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.01, 0.13, 0, 0, 0));
	mu_assert(d->num_rows == 12);

	mu_assert(!data_get_entry_as_int32(&iv,d,0,0));
	mu_assert(iv == 0);
	mu_assert(!data_get_entry_as_int32(&iv,d,5,0));
	mu_assert(iv == 1);

	mu_assert(!data_get_entry_as_double(&dv,d,0,1));
	mu_assert(dv == 0.11);
	mu_assert(!data_get_entry_as_double(&dv,d,11,1));
	mu_assert(dv == 0.01);

	data_sort_v(d,1,1);
	mu_assert(!data_get_entry_as_double(&dv,d,0,1));
	mu_assert(dv == 0.01);
	mu_assert(!data_get_entry_as_double(&dv,d,1,1));
	mu_assert(dv == 0.08);
	mu_assert(!data_get_entry_as_double(&dv,d,11,1));
	mu_assert(dv == 0.68);

	for (i=0;i<12;i++)
	{
		mu_assert(!data_get_entry_as_int32(&iv,d,i,3));
		mu_assert(iv == i);
	}

	return NULL;
}

/************************************************************/

static char *test_data_simple(void)
{
	char *rc;

	data_t *d;

	mu_assert(!data_create(&d));
	mu_assert(d);

	if ((rc = helper_insert_and_assert_data(d)))
		return rc;

	data_free(d);
	return NULL;
}

/************************************************************/

static char *test_data_more_than_a_block(void)
{
	char *rc;

	data_t *d;

	mu_assert(!data_create(&d));
	mu_assert(d);

	d->ib_bytes = 64;

	if ((rc = helper_insert_and_assert_data(d)))
		return rc;

	data_free(d);
	return NULL;
}

/************************************************************/

static char *run_test_suite(void)
{
	mu_run_test(test_data_simple);
	mu_run_test(test_data_more_than_a_block);
	return NULL;
}

int main(int argc, char **argv)
{
	int rc;
	const char *errstr;

	rc = EXIT_FAILURE;
	if (!(errstr = run_test_suite()))
	{
		rc = EXIT_SUCCESS;
	} else
	{
		fprintf(stderr,"Test failed: %s\n",errstr);
	}
	return rc;
}
