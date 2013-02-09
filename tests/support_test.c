#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "minunit.h"
#include "support.c"

int tests_run;

struct test_callback_data
{
	uint32_t ps[12];
	uint32_t ns[12];
	uint32_t tps[12];
	uint32_t fps[12];
	uint32_t current;
};

static int test_data_roc_precall_callback(uint32_t ps, uint32_t ns, uint32_t tps, uint32_t fps, void *userdata)
{
	struct test_callback_data *tcd = (struct test_callback_data*)userdata;
	if (tcd->current < 12)
	{
		tcd->ps[tcd->current] = ps;
		tcd->ns[tcd->current] = ns;
		tcd->tps[tcd->current] = tps;
		tcd->fps[tcd->current] = fps;
		tcd->current++;
		return 0;
	}
	tcd->current++;
	return 0;
}

static char *helper_assert_data(data_t *d)
{
	double dv;
	int32_t iv;
	int i;

	mu_assert(!data_get_entry_as_int32(&iv,d,0,0));
	mu_assert(iv == 0);
	mu_assert(!data_get_entry_as_int32(&iv,d,5,0));
	mu_assert(iv == 1);

	mu_assert(!data_get_entry_as_double(&dv,d,0,1));
	mu_assert(dv == 0.11);
	mu_assert(!data_get_entry_as_double(&dv,d,11,1));
	mu_assert(dv == 0.01);

	mu_assert(!data_sort_v(d,1,1));
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

	mu_assert(!data_sort_v(d,1,2));
	for (i=0;i<12;i++)
	{
		mu_assert(!data_get_entry_as_int32(&iv,d,i,4));
		mu_assert(iv == i);
	}


	int col = 1;
	struct test_callback_data tcb;
	memset(&tcb,0,sizeof(tcb));
	mu_assert(!data_stat_callback(d,test_data_roc_precall_callback,&tcb,0,1,&col));
	mu_assert(tcb.current == 12);
	static const char expected_tps[12] = {0,0,0,0,0,0,0,0,0,1,1,2};
	static const char expected_fps[12] = {1,2,3,4,5,6,7,8,9,9,10,10};
	for (i=0;i<12;i++)
	{
		mu_assert(2 == tcb.ps[i]);
		mu_assert(10 == tcb.ns[i]);
		mu_assert(expected_tps[i] == tcb.tps[i]);
		mu_assert(expected_fps[i] == tcb.fps[i]);
	}

	return NULL;
}

/**
 * Helper to insert and check some data.
 *
 * @param d
 * @return
 */
static char *helper_insert_and_assert_data(data_t *d)
{
	char *rc;

	mu_assert(!data_set_number_of_columns(d,6));
	mu_assert(d->num_columns == 6);
	data_set_column_datatype(d,0,INT32);
	data_set_column_datatype(d,1,DOUBLE);
	data_set_column_datatype(d,2,DOUBLE);
	data_set_column_datatype(d,3,INT32);
	data_set_column_datatype(d,4,INT32);
	data_set_column_datatype(d,5,INT32);
	mu_assert(data_sizeof_row_and_set_column_offsets(d)==32);
	mu_assert(d->column_offsets[0] == 0);
	mu_assert(d->column_offsets[1] == 4);
	mu_assert(d->column_offsets[2] == 12);
	mu_assert(d->column_offsets[3] == 20);
	mu_assert(d->column_offsets[4] == 24);
	mu_assert(d->column_offsets[5] == 28);
	mu_assert(d->num_rows == 0);

	mu_assert(!data_insert_row_v(d, 0, 0.11, 0.12,  3,  3, 0));
	mu_assert(d->num_rows == 1);
	mu_assert(!data_insert_row_v(d, 0, 0.24, 0.11,  5,  2, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.14, 0.43,  4,  6, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.33, 0.56,  6,  9, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.45, 0.44,  7,  7, 0));
	mu_assert(!data_insert_row_v(d, 1, 0.68, 0.49, 11,  8, 0));
	mu_assert(!data_insert_row_v(d, 1, 0.58, 0.59,  9, 10, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.59, 0.68, 10, 11, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.51, 0.42,  8,  5, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.09, 0.09,  2,  1, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.08, 0.08,  1,  0, 0));
	mu_assert(!data_insert_row_v(d, 0, 0.01, 0.13,  0,  4, 0));
	mu_assert(d->num_rows == 12);

	if ((rc = helper_assert_data(d)))
		return rc;

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

static char *test_data_load_from_ascii(void)
{
	char *rc;
	data_t *d;
	mu_assert(!data_create(&d));

	mu_assert(!data_load_from_ascii(d,"tests/resources/test.dat"));
	if ((rc = helper_assert_data(d)))
		return rc;

	data_free(d);
	return NULL;
}

/************************************************************/

static char *test_fio(void)
{
	struct fio fio;
	const char *l;

	mu_assert(!fio_init_by_file(&fio,"tests/resources/test.dat"));
	mu_assert(fio.first_lines[0]);
	mu_assert(!strcmp("label\tpred1\tpred2\to1\to2\to3\n",fio.first_lines[0]));

	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("label\tpred1\tpred2\to1\to2\to3\n",l));

	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.11	0.12	3	3	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.24	0.11	5	2	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.14	0.43	4	6	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.33	0.56	6	9	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.45	0.44	7	7	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("1	0.68	0.49	11	8	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("1	0.58	0.59	9	10	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.59	0.68	10	11	0\n",l));
	mu_assert(!fio_read_next_line(&l,&fio));
	mu_assert(!strcmp("0	0.51	0.42	8	5	0\n",l));

	fio_deinit(&fio);

	return NULL;
}

/************************************************************/

static char *run_test_suite(void)
{
	mu_run_test(test_fio);
	mu_run_test(test_data_simple);
	mu_run_test(test_data_more_than_a_block);
	mu_run_test(test_data_load_from_ascii);
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
