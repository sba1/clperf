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

int data_stat_callback(data_t *d, int (*callback)(uint32_t ps, uint32_t ns, uint32_t tps, uint32_t fps, void *userdata), void *user_data, int label_col, int cols, int *to_sort_cols);

int data_stat_hist(data_t *d, int breaks, int label_col, int cols, int *to_sort_cols);
int data_stat_hist_v(data_t *d, int breaks, int label_col, int cols, ...);

int data_get_precision_by_recall(double *precision, data_t *d, double recall);
int data_get_tpr_by_fpr(double *tpr, data_t *d, double fpr);

#endif
