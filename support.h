#ifndef CLPERF_SUPPORT_H
#define CLPERF_SUPPORT_H

#include <stdint.h>

typedef struct data data_t;
typedef struct perf perf_t;

enum column_datatype_t
{
	UNKNOWN,
	INT32,
	DOUBLE
};

int data_create(data_t **out);
void data_free(data_t *d);
void data_set_external_filename(data_t *d, const char *filename);
int data_load_from_ascii(data_t *d, const char *filename);

uint32_t data_get_number_of_columns(data_t *d);
uint32_t data_get_number_of_rows(data_t *d);

int data_stat(data_t *d, int label_col, int cols, int *to_sort_cols);
int data_stat_v(data_t *d, int label_col, int cols, ...);

#endif
