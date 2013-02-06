/**
 * @file perf.c
 */

#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "support.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

/**************************************************************/

typedef struct
{
	/** Memory allocated for the block */
	uint8_t *block;

	/** Size of the block in bytes */
	uint32_t block_bytes;

	/** The offset of the blocks in rows */
	uint32_t row_offset;

	/** The number of rows covered by the block */
	uint32_t num_rows;

	/** The current row relative to row_offset */
	uint32_t current_relative_row;

	uint32_t current_row;
} block_t;

struct data
{
	const char *filename;
	FILE *tmp;

	enum column_datatype_t *column_datatype;
	uint32_t *column_offsets;
	uint32_t num_columns;
	uint32_t num_rows;
	uint32_t num_bytes_per_row;

	/** Size in bytes for the input block */
	uint32_t ib_bytes;

	/** Input block */
	block_t ib;
};

int data_create(data_t **out)
{
	int err = -1;
	data_t *n;

	if (!(n = (data_t*)malloc(sizeof(*n))))
		goto out;
	memset(n,0,sizeof(*n));

	n->ib_bytes = 1024 * 1024;
	n->filename = "out";
	*out = n;
	err = 0;
out:
	return err;
}

void data_free(data_t *d)
{
	if (d)
	{
		if (d->tmp)
			fclose(d->tmp);
		free(d->column_datatype);
		free(d->column_offsets);
		free(d->ib.block);
		free(d);
	}
}

/**
 * Set the name of the external file to be used when storing and
 * sorting.
 *
 * @param d
 * @param filename
 */
void data_set_external_filename(data_t *d, const char *filename)
{
	d->filename = filename;
}

int data_set_number_of_columns(data_t *d, uint32_t cols)
{
	int i;
	int err = -1;

	d->num_columns = cols;

	if (!(d->column_datatype = malloc(sizeof(d->column_datatype[0])*cols)))
		goto out;

	if (!(d->column_offsets = malloc(sizeof(d->column_offsets[0])*cols)))
		goto out;

	for (i=0;i<cols;i++)
	{
		d->column_datatype[i] = UNKNOWN;
		d->column_offsets[i] = 0;
	}
	err = 0;

out:
	return err;
}

int data_set_column_datatype(data_t *d, int col, enum column_datatype_t dt)
{
	d->column_datatype[col] = dt;
}

static size_t data_sizeof_columns_and_set_column_offsets(data_t *d)
{
	int col;
	size_t size = 0;

	for (col=0;col<d->num_columns;col++)
	{
		d->column_offsets[col] = size;
		switch (d->column_datatype[col])
		{
			case	INT32: size += sizeof(int32_t); break;
			case	DOUBLE: size += sizeof(double); break;
			default: break;
		}
	}
	return size;
}

static int data_initialize_block(block_t *b, data_t *d, uint32_t block_bytes)
{
	int col;
	int err = -1;
	if (!(b->block = (uint8_t*)malloc(block_bytes)))
		goto out;

	b->num_rows = block_bytes / d->num_bytes_per_row;
	b->row_offset = 0;
	b->current_row = 0;

	fprintf(stderr,"Initialized block %p with %d rows of storage\n",b,b->num_rows);
	err = 0;
out:
	return err;
}

static int data_write_input_block(data_t *d)
{
	int err;
	block_t *b = &d->ib;

	err = -1;

	if (!d->tmp)
	{
		if (!(d->tmp = fopen(d->filename,"w+")))
			goto out;
	}

	if (fseek(d->tmp,d->num_bytes_per_row * b->row_offset,SEEK_SET))
	{
		fprintf(stderr,"Seek failed\n");
		goto out;
	}
	fprintf(stderr,"Writing to %x (offset %d)\n",ftell(d->tmp),b->row_offset);
	if ((fwrite(b->block,d->num_bytes_per_row,b->num_rows,d->tmp) != b->num_rows ))
	{
		fprintf(stderr,"Write failed!\n");
		goto out;
	}
	err = 0;
out:
	return err;
}

int data_insert_row_v(data_t *d, ...)
{
	int err = -1;
	int col;
	uint8_t *buf;

	va_list vl;

	if (!d->ib.block)
	{
		d->num_bytes_per_row = data_sizeof_columns_and_set_column_offsets(d);
		data_initialize_block(&d->ib,d,d->ib_bytes);
	}

	if (d->ib.current_relative_row >= d->ib.num_rows)
	{
		data_write_input_block(d);
		d->ib.row_offset += d->ib.num_rows;
		d->ib.current_relative_row = 0;
	}

	va_start(vl,d);

	buf = d->ib.block + d->ib.current_relative_row * d->num_bytes_per_row;

	for (col=0;col<d->num_columns;col++)
	{
		switch (d->column_datatype[col])
		{
			case	INT32:
					{
						int32_t v = va_arg(vl,int32_t);
						*((int32_t*)buf) = v;
						buf += sizeof(v);
					}
					break;

			case	DOUBLE:
					{
						double v = va_arg(vl,double);
						*((double*)buf) = v;
						buf += sizeof(v);
					}
					break;
			default: goto out;
		}
	}
	err = 0;
	d->num_rows++;
	d->ib.current_relative_row++;
out:
	va_end(vl);
	return err;
}

/**
 * Read the block starting at row in the block.
 *
 * @param d
 * @param b
 * @param row
 * @return
 */
static int data_read_block_for_row(data_t *d, block_t *b, int row)
{
	int err = -1;

	fseek(d->tmp, row * d->num_bytes_per_row, SEEK_SET);
	fprintf(stderr,"Reading from %x (offset %d)\n", ftell(d->tmp), row);
	if (fread(b->block, d->num_bytes_per_row, b->num_rows, d->tmp) == 0)
	{
		fprintf(stderr,"Reading row %d failed!\n",row);
		goto out;
	}
	b->row_offset = row;
	err = 0;
out:
	return err;

}

static int data_read_input_block_for_row(data_t *d, int row)
{
	int err = -1;
	uint32_t new_row_offset_of_block;

	new_row_offset_of_block = (row / d->ib.num_rows) * d->ib.num_rows;
	if (new_row_offset_of_block != d->ib.row_offset)
	{
		data_write_input_block(d);
		if ((err = data_read_block_for_row(d, &d->ib, new_row_offset_of_block)))
			goto out;
	}
	err = 0;
out:
	return err;
}

static int data_get_buf_ptr(uint8_t **out, data_t *d, int i, int j)
{
	int err = -1;
	uint8_t *buf;

	if (i < d->ib.row_offset || i >= d->ib.row_offset + d->ib.num_rows)
	{
		if ((err = data_read_input_block_for_row(d,i)))
		{
			fprintf(stderr,"Couldn't read block %d\n",errno);
			goto out;
		}
	}

	buf = d->ib.block + (i - d->ib.row_offset) * d->num_bytes_per_row + d->column_offsets[j];
	*out = buf;
	err = 0;
out:
	return err;
}

int data_get_entry_as_double(double *out, data_t *d, int i, int j)
{
	uint8_t *buf;
	data_get_buf_ptr(&buf,d,i,j);
	*out = *(double*)buf;
	return 0;
}

int data_get_entry_as_int32(int32_t *out, data_t *d, int i, int j)
{
	uint8_t *buf;
	data_get_buf_ptr(&buf,d,i,j);
	*out = *(int32_t*)buf;
	return 0;
}

static int data_sort_compare_cb(const void *a, const void *b, void *data)
{
	const uint8_t *ra = (const uint8_t*)a;
	const uint8_t *rb = (const uint8_t*)b;

	ra += 4;
	rb += 4;

	double da = *(double*)ra;
	double db = *(double*)rb;

	if (da > db) return 1;
	if (da < db) return -1;
	return 0;
}

/**
 * Compare the heads of the given block.
 *
 * @param d
 * @param a
 * @param b
 * @return -1 if a is smaller, else -1.
 */
static int data_sort_compare_heads_of_blocks(data_t *d, block_t *a, block_t *b)
{
	int aoff = a->current_relative_row * d->num_bytes_per_row;
	int boff = b->current_relative_row * d->num_bytes_per_row;

	if (data_sort_compare_cb(&a->block[aoff], &b->block[boff], d) < 0)
		return -1;
	return 1;
}

/**
 * Advance the head of the given block.
 *
 * @param d
 * @param b
 * @return
 */
static int data_advance_head(data_t *d, block_t *b)
{
	int err = -1;

	b->current_relative_row++;
	b->current_row++;

	err = 0;
out:
	return err;
}

/**
 * Sorts the entire data.
 *
 * @param d
 * @param cols
 * @param to_sort_cols
 * @return
 */
static int data_sort_callback(data_t *d, int cols, int *to_sort_cols, int (*callback)(data_t *d, uint8_t *row, void *user_data), void *user_data)
{
	int i;
	int k;

	int err = -1;

	/* In place sort using the input block buffer */
	for (i=0;i<d->num_rows;)
	{
		int rows_to_sort = MIN(d->ib.num_rows,d->num_rows - i);

		if ((err = data_read_input_block_for_row(d,i)))
			goto out;
		qsort_r(d->ib.block,rows_to_sort,d->num_bytes_per_row,data_sort_compare_cb,d);
		i += rows_to_sort;
	}

	/* Now merge sort, we only support one pass for now */
	k = (d->num_rows + d->ib.num_rows - 1 ) / d->ib.num_rows;
	if (k > 1)
	{
		/* Write possible rest of the cache */
		if ((err = data_write_input_block(d)))
		{
			fprintf(stderr,"Couldn't write block\n");
			goto out;
		}

		block_t *in_blocks;
		int rows_per_in_block = (d->num_rows + k - 1)/k;

		fprintf(stderr,"Taking k=%d fixed blocks containing %d rows\n",k,rows_per_in_block);

		if (!(in_blocks = malloc(sizeof(in_blocks[0])*k)))
		{
			fprintf(stderr,"Memory allocation failed!");
			goto out;
		}

		memset(in_blocks,0,sizeof(in_blocks[0])*k);

		/* Init in buffers */
		for (i=0;i<k;i++)
		{
			if ((err = data_initialize_block(&in_blocks[i],d,32)))
			{
				fprintf(stderr,"Couldn't alloc block for input\n");
				goto out;
			}
			if ((err = data_read_block_for_row(d,&in_blocks[i],i*rows_per_in_block)))
			{
				fprintf(stderr,"Couldn't read in block\n");
				goto out;
			}
		}

		int m;

		/* Merge */
		for (m=0;m<d->num_rows;m++)
		{
			int sk;

			for (sk=0;sk<k;sk++)
				if (in_blocks[sk].current_row < rows_per_in_block)
					break;

			for (i=sk;i<k;i++)
			{
				if (in_blocks[i].current_row >= rows_per_in_block)
					continue;

				if (in_blocks[i].current_relative_row == in_blocks[i].num_rows)
				{
					if ((err = data_read_block_for_row(d,&in_blocks[i],in_blocks[i].row_offset + in_blocks[i].num_rows)))
					{
						fprintf(stderr,"Couldn't read in block for %d\n",i);
						goto out;
					}
					in_blocks[i].current_relative_row = 0;
				}

				if (i == sk)
					continue;

				if (data_sort_compare_heads_of_blocks(d,&in_blocks[sk],&in_blocks[i]) > 0)
					sk = i;
			}

			block_t *bsk = &in_blocks[sk];
			callback(d, &bsk->block[bsk->current_relative_row * d->num_bytes_per_row], user_data);
			data_advance_head(d,bsk);
		}

		for (i=0;i<k;i++)
			free(in_blocks[i].block);
		free(in_blocks);
	}
	err = 0;
out:
	return err;
}

static int data_sort_cb(data_t *d, uint8_t *buf, void *user_data)
{
	int err;
	FILE *sorted_outf;

	err = -1;

	sorted_outf = (FILE*)user_data;
	fwrite(buf,d->num_bytes_per_row,1,sorted_outf);
	err = 0;
out:
	return err;
}

static int data_sort(data_t *d, int cols, int *to_sort_cols)
{
	int err = -1;
	char *sorted_name = NULL;
	FILE *sorted_outf = NULL;

	if (!(sorted_name = malloc(strlen(d->filename) + 10)))
		goto out;

	strcpy(sorted_name,d->filename);
	strcat(sorted_name,"-sorted");

	if (!(sorted_outf = fopen(sorted_name,"wb")))
		goto out;

	if ((err = data_sort_callback(d, cols, to_sort_cols, data_sort_cb, sorted_outf)))
		goto out;

	if (d->tmp)
	{
		fclose(d->tmp);
		d->tmp = NULL;

		fclose(sorted_outf);
		sorted_outf = NULL;

		remove(d->filename);
		rename(sorted_name,d->filename);

		if (!(d->tmp = fopen(d->filename,"a+")))
		{
			fprintf(stderr,"Couldn't open file for appending\n");
			goto out;
		}
		data_read_block_for_row(d, &d->ib, 0);
	}

	err = 0;
out:
	if (sorted_outf) fclose(sorted_outf);
	if (sorted_name) free(sorted_name);
	return err;
}

static int data_sort_v(data_t *d, int cols, ...)
{
	int i;
	int err = -1;
	int to_sort_cols[cols];

	va_list vl;
	va_start(vl,cols);

	for (i=0;i<cols;i++)
		to_sort_cols[i] = va_arg(vl,int);

	data_sort(d,cols,to_sort_cols);
out:
	va_end(vl);
	return err;
}

static int data_stat(data_t *d, int label_col, int cols, int *to_sort_cols)
{
	int r;
	int err = -1;

	int32_t *tps = NULL;

	if ((err = data_sort(d,cols,to_sort_cols)))
		goto out;

	if (!(tps = malloc(sizeof(tps[0])*d->num_rows)))
		goto out;

	if ((err = data_get_entry_as_int32(&tps[0],d,0,label_col)))
		goto out;

	for (r=1; r < d->num_rows; r++)
	{
		int32_t l;

		if ((err = data_get_entry_as_int32(&l,d,r,label_col)))
			goto out;
		fprintf(stderr,"%d\n",l);
		tps[r] = tps[r - 1] + (l > 0);
	}
	err = 0;

	int positives = tps[d->num_rows - 1];
	int negatives = d->num_rows - positives;

	for (r=0; r < d->num_rows; r++)
	{
		int32_t fps = (r+1) - tps[r];

		double tpr = (double)tps[r] / positives; /* true positive rate */
		double fpr = (double)fps / negatives; /* false postive rate */
		double prec = (double)tps[r]/(r+1); /* precision = true positives / (number of all positives = (true positives + false negatives) */
		double recall = (double)tps[r]/tps[d->num_rows-1]; /* recall = number of true positives / (true positives + false negatives = all positive samples) */

		fprintf(stderr,"%d tps=%d fps=%d tpr=%lf fpr=%lf prec=%lf recall=%lf\n",r,tps[r],fps,tpr,fpr,prec,recall);
	}
out:
	fprintf(stderr,"Stats err=%d\n",err);
	free(tps);
	return err;
}


static int data_stat_v(data_t *d, int label_col, int cols, ...)
{
	int i;
	int err = -1;
	int to_sort_cols[cols];

	va_list vl;
	va_start(vl,cols);

	for (i=0;i<cols;i++)
		to_sort_cols[i] = va_arg(vl,int);

	err = data_stat(d, label_col, cols, to_sort_cols);
out:
	va_end(vl);
	return err;
}
