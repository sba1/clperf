/**
 * @file perf.c
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "support.h"

#define D(txt,...) fprintf(stderr,txt,__VA_ARGS__)

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

/**************************************************************/

#define FIO_FIRST_LINES 8

struct fio
{
	FILE *file;
	int file_was_opened;
	int current_line_nr;
	char *current_line;
	char *first_lines[FIO_FIRST_LINES];
};

int fio_init_by_file(struct fio *f, const char *filename)
{
	int err;
	int i;
	FILE *file;

	err = -1;

	memset(f,0,sizeof(*f));
	if (!(file = fopen(filename,"r")))
		goto out;

	f->file = file;
	f->file_was_opened = 1;

	for (i=0;i<FIO_FIRST_LINES;i++)
	{
		size_t len = 0;

		if (getline(&f->first_lines[i],&len,f->file) < 0)
			break;
	}
	err = 0;
out:
	return err;
}

/**
 * Reads a single line.
 *
 * @param line
 * @param f
 * @return
 */
int fio_read_next_line(const char **line, struct fio *f)
{
	char *l;
	int err;

	err = -1;

	if (f->current_line)
	{
		free(f->current_line);
		f->current_line = NULL;
	}

	if (f->current_line_nr < FIO_FIRST_LINES)
	{
		if (!(l = f->first_lines[f->current_line_nr]))
			goto out;
		f->current_line_nr++;
	} else
	{
		size_t len = 0;
		l = NULL;

		if (getline(&l,&len,f->file) < 0)
		{
			if (l) free(l);
			goto out;
		}
	}

	if (!l)
		goto out;

	*line = l;
	f->current_line = l;
	err = 0;
out:
	return err;
}

void fio_deinit(struct fio *f)
{
	if (f->current_line) free(f->current_line);
	if (f->file_was_opened)
	{
		fclose(f->file);
	}
}

/**************************************************************/

struct progress
{
	const char *task;

	uint64_t todo;
	uint64_t done;

	time_t last_time;
};

static void progress_init(struct progress *p, const char *task, uint64_t todo)
{
	p->done = 0;
	p->todo = todo;
	p->task = task;

	time(&p->last_time);
}

static void progress_done(struct progress *p, uint64_t done)
{
	p->done = done;
}

static void progress_print(struct progress *p, int force)
{
	time_t new_time;
	time(&new_time);
	if (difftime(p->last_time,new_time))
	{
		fprintf(stderr,"%s: %lld%%\n",p->task,(p->done * 100 / p->todo));
		p->last_time = new_time;
	}
}

/**************************************************************/

struct hist
{
	int num_counts;

	int *counts;
	double *y;
};

static int hist_init(struct hist *h, int counts)
{
	int err = -1;
	h->num_counts = counts;
	if (!(h->counts = (int*)calloc(counts,sizeof(h->counts[0]))))
		goto out;

	if (!(h->y = (double*)calloc(counts,sizeof(h->y[0]))))
		goto out;

	err = 0;
out:
	return err;
}

static int hist_get_slot(struct hist *h, double x)
{
	int slot;

	slot = (int)(x * (h->num_counts - 1));
	slot = MAX(0.0,MIN(slot,h->num_counts - 1));

	return slot;
}

static void hist_put(struct hist *h, double x, double y)
{
	int slot;

	slot = hist_get_slot(h,x);
	h->counts[slot]++;
	h->y[slot] += y;
}

static void hist_average(struct hist *h)
{
	int j;

	for (j=0;j<h->num_counts;j++)
	{
		int c = h->counts[j];
		if (c)
			h->y[j] /= (double)c;
	}
}

static double hist_get_y(struct hist *h, double x)
{
	int slot;
	int lslot;
	int rslot;

	slot = hist_get_slot(h,x);
	if (h->counts[slot])
		return h->y[slot];

	lslot = slot - 1;
	while (lslot >= 0)
	{
		if (h->counts[lslot])
			break;
		lslot--;
	}

	rslot = slot + 1;
	while (rslot < h->num_counts)
	{
		if (h->counts[rslot])
			break;
		rslot++;
	}

	if (lslot < 0 && rslot >= h->num_counts)
		return 0.0;

	if (lslot < 0) lslot = rslot;
	else if (rslot >= h->num_counts) rslot = lslot;

	return (h->y[lslot] + h->y[rslot])/2;
}

static void hist_free(struct hist *h)
{
	free(h->counts);
	free(h->y);
}

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

	int *to_sort_columns;
	int num_to_sort_columns;

	int label_col;
	int64_t label_sum;

	/* Histograms of various measures */
	int hist_initialized;
	struct hist roc;
	struct hist precall;
};

/**
 * Constructs an empty data frame.
 *
 * @param out where the reference is stored.
 * @return 0 on success, else an error.
 */
int data_create(data_t **out)
{
	int err = -1;
	data_t *n;

	if (!(n = (data_t*)malloc(sizeof(*n))))
		goto out;
	memset(n,0,sizeof(*n));

	n->ib_bytes = 1024 * 1024 * 10;
	n->filename = "out";
	*out = n;
	err = 0;
out:
	return err;
}

/**
 * Frees all memory associated with the given
 * data frame.
 *
 * @param d
 */
void data_free(data_t *d)
{
	if (d)
	{
		if (d->hist_initialized)
		{
			hist_free(&d->precall);
			hist_free(&d->roc);
		}

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

/**
 * Sets the number of columns of the given data frame.
 *
 * @param d
 * @param cols
 * @return 0 on success, else an error.
 */
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

/**
 * Sets the type of the column.
 *
 * @param d
 * @param col
 * @param dt
 * @return
 */
int data_set_column_datatype(data_t *d, int col, enum column_datatype_t dt)
{
	d->column_datatype[col] = dt;
	return 0;
}

/**
 * Calculates the memory occupied by one row and determines the
 * column offsets.
 *
 * @param d
 * @return the number of bytes occupied by one row.
 */
static size_t data_sizeof_row_and_set_column_offsets(data_t *d)
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

/**
 * Initializes a given block.
 *
 * @param b
 * @param d
 * @param block_bytes
 * @return 0 on success, else an error.
 */
static int data_initialize_block(block_t *b, data_t *d, uint32_t block_bytes)
{
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

/**
 * Write the contents of the input block to disk.
 *
 * @param d
 * @return 0 on success, else an error.
 */
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
	fprintf(stderr,"Writing to %x (offset %d)\n",(unsigned int)ftell(d->tmp),b->row_offset);
	if ((fwrite(b->block,d->num_bytes_per_row,b->num_rows,d->tmp) != b->num_rows ))
	{
		fprintf(stderr,"Write failed!\n");
		goto out;
	}
	err = 0;
out:
	return err;
}


/**
 * Prepare the data for next row and determine buffers
 * where it can be stored.
 *
 * @param out the pointer to a location in which the values of the next
 *  row can be stored.
 * @param d
 * @return 0 on success, else an error.
 */
int data_insert_row_prolog(uint8_t **out, data_t* d)
{
	int err = -1;

	if (!d->ib.block)
	{
		d->num_bytes_per_row = data_sizeof_row_and_set_column_offsets(d);
		if ((err = data_initialize_block(&d->ib, d, d->ib_bytes)))
			goto out;
	}

	if (d->ib.current_relative_row >= d->ib.num_rows)
	{
		data_write_input_block(d);
		d->ib.row_offset += d->ib.num_rows;
		d->ib.current_relative_row = 0;
	}
	*out = d->ib.block + d->ib.current_relative_row * d->num_bytes_per_row;
	err = 0;
out:
	return err;
}

/**
 * Insert a single row.
 *
 * @param d
 * @param row
 * @return 0 on success, else an error.
 */
int data_insert_row(data_t *d, uint8_t *row)
{
	int err = -1;
	uint8_t *buf;

	if ((err = data_insert_row_prolog(&buf,d)))
		goto out;

	memcpy(buf,row,d->num_bytes_per_row);

	d->num_rows++;
	d->ib.current_relative_row++;

	err = 0;
out:
	return err;
}

/**
 * Insert a single row using a list of variable arguments.
 *
 * @param d the data frame in which to insert the row.
 * @return 0 on success, else an error.
 */
int data_insert_row_v(data_t *d, ...)
{
	int err = -1;
	int col;
	uint8_t *buf;

	va_list vl;

	if ((err = data_insert_row_prolog(&buf,d)))
		goto out;

	va_start(vl,d);

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
	d->num_rows++;
	d->ib.current_relative_row++;

	err = 0;
out:
	va_end(vl);
	return err;
}

/**
 * Loads from the given file a data frame in to an already
 * created (vanilla) data frame.
 *
 * @param d the result as returned by data_create().
 * @param filename the file from which to read
 * @return 0 on success, else an error.
 */
int data_load_from_ascii(data_t *d, const char *filename)
{
	int i;
	int err = -1;
	struct fio fio;
	int pro_header = 0;
	int con_header = 0;
	int ncols = 1;
	enum column_datatype_t *column_types = NULL;
	uint8_t *row = NULL;

	size_t len ;
	const char *line;
	int linenr = 1; /* 1-based */
	int first_data_line = 0;

	if ((err = fio_init_by_file(&fio,filename)))
	{
		fprintf(stderr,"Couldn't open \"%s\"\n",filename);
		goto out;
	}

	line = fio.first_lines[0];
	len = strlen(line);

	/* Guess, if this is a header, also determine number of columns */
	for (i=0;i<len;i++)
	{
		if (line[i] == '\t')
			ncols++;
		else
		{
			if (line[i] == '-' || line[i] == 'e' || line[i] == 'E' || line[i] == '.' || isdigit((int)line[i]))
			{
				pro_header++;
				con_header++;
			} else
			{
				pro_header++;
			}
		}
	}

	if (pro_header > con_header)
	{
		first_data_line = 1;
		if ((err = fio_read_next_line(&line,&fio)))
			goto out;
	}
	else
		first_data_line = 0;

	/* Determine columns */
	int ln;

	if (!(column_types = (enum column_datatype_t*)calloc(ncols,sizeof(column_types[0]))))
		goto out;

	for (ln = first_data_line; ln < FIO_FIRST_LINES && ((line = fio.first_lines[ln])); ln++)
	{
		int is_double = 0;
		int col = 0;
		len = strlen(line);

		for (i=0;i<len;i++)
		{
			if (line[i] == '\t' || line[i] == '\n')
			{
				enum column_datatype_t newt = INT32;
				if (is_double) newt = DOUBLE;

				if (column_types[col] == UNKNOWN || newt == DOUBLE)
					column_types[col] = newt;

				col++;
				is_double = 0;
			} else
			{
				if (line[i] == '-' || line[i] == 'e' || line[i] == 'E' || line[i] == '.')
					is_double = 1;
			}
		}
	}

	if ((err = data_set_number_of_columns(d,ncols)))
		goto out;

	for (i=0;i<ncols;i++)
		data_set_column_datatype(d,i,column_types[i]);

	if (!(row = (uint8_t*)malloc(data_sizeof_row_and_set_column_offsets(d))))
		goto out;

	linenr = first_data_line;

	fprintf(stderr,"Identified %d columns\n",ncols);

	while (!(err = fio_read_next_line(&line,&fio)))
	{
		int pos = 0;
		int row_pos = 0;

		linenr++;

		for (i=0;i<ncols;i++)
		{
			enum column_datatype_t ct = column_types[i];

			switch (ct)
			{
				case	INT32:
						{
							long v;
							char *end;
							v = strtol(&line[pos],&end,10);
							*((int32_t*)&row[row_pos]) = v;
							row_pos += sizeof(int32_t);
							pos = end - line;
							break;
						}

				case	DOUBLE:
						{
							double v;
							char *end;
							v = strtod(&line[pos],&end);
							*((double*)&row[row_pos]) = v;
							row_pos += sizeof(double);
							pos = end - line;
							break;
						}
				default:
						fprintf(stderr,"Unknown column type at line %d in column %d\n",linenr,i);
						goto out;

			}

			while (line[pos] && line[pos] != '\t')
				pos++;
		}
		if ((err = data_insert_row(d,row)))
			goto out;
	}

	err = 0;
out:
	free(column_types);
	free(row);
	fio_deinit(&fio);
	return err;
}

/**
 * Returns the number of columns of the data frame.
 *
 * @param d the data frame in question
 * @return the number of columns
 */
uint32_t data_get_number_of_columns(data_t *d)
{
	return d->num_columns;
}

/**
 * Returns the number of rows of the data frame.
 *
 * @param d the data frame in question
 * @return the number of rows
 */
uint32_t data_get_number_of_rows(data_t *d)
{
	return d->num_rows;
}


/**
 * Read the block starting at row in the block.
 *
 * @param d the data frame associated with the block
 * @param b the block where to store the result of the read operation.
 * @param row the index of the row.
 * @return 0 on success, else an error.
 */
static int data_read_block_for_row(data_t *d, block_t *b, int row)
{
	int err = -1;

	fseek(d->tmp, row * d->num_bytes_per_row, SEEK_SET);
	fprintf(stderr,"Reading from %x (offset %d)\n", (unsigned int)ftell(d->tmp), row);
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

/**
 * Read the contents of the given row to the input block.
 *
 * @param d the associated data frame.
 * @param row
 * @return 0 on success, else an error.
 */
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

/**
 * Determine the pointer to the given column/row. May read the associated
 * block if the element is currently not in the input block.
 *
 * @param out where to store the pointer.
 * @param d the associatated data frame.
 * @param i the row
 * @param j the column
 * @return 0 on success, else an error.
 */
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
	int c;
	data_t *d = (data_t*)data;
	const uint8_t *ra = (const uint8_t*)a;
	const uint8_t *rb = (const uint8_t*)b;

	for (c=0;c<d->num_to_sort_columns;c++)
	{
		int offset = d->column_offsets[d->to_sort_columns[c]];

		double da = *(double*)(&ra[offset]);
		double db = *(double*)(&rb[offset]);

		if (da > db) return 1;
		if (da < db) return -1;

	}
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

	struct progress p;

	int label_col_offset = d->column_offsets[d->label_col];
	int64_t label_sum = 0;

	progress_init(&p,"Sorting - first pass",d->num_rows);

	/* In place sort using the input block buffer */
	for (i=0;i<d->num_rows;)
	{
		int rows_to_sort = MIN(d->ib.num_rows,d->num_rows - i);

		if ((err = data_read_input_block_for_row(d,i)))
			goto out;
		qsort_r(d->ib.block,rows_to_sort,d->num_bytes_per_row,data_sort_compare_cb,d);

		for (k=0;k<rows_to_sort;k++)
		{
			uint8_t *buf = &d->ib.block[k * d->num_bytes_per_row];
			label_sum += *(int32_t*)(&buf[label_col_offset]);

		}

		i += rows_to_sort;

		progress_done(&p,i);
		progress_print(&p,0);
	}
	d->label_sum = label_sum;

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
			if ((err = data_initialize_block(&in_blocks[i],d,MIN(rows_per_in_block*d->num_bytes_per_row,65536))))
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

		progress_init(&p,"Sorting - second pass",d->num_rows);

		int m;

		/* Merge */
		for (m=0;m<d->num_rows;m++)
		{
			int sk; /* k with smallest entry */

			progress_done(&p,m);

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
			uint8_t *bskb = &bsk->block[bsk->current_relative_row * d->num_bytes_per_row];
			callback(d, bskb, user_data);
			data_advance_head(d,bsk);

			progress_print(&p,0);
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
	if (!fwrite(buf,d->num_bytes_per_row,1,sorted_outf))
	{
		fprintf(stderr,"Failed to write some data\n!");
		goto out;
	}
	err = 0;
out:
	return err;
}

static int data_sort(data_t *d, int num_to_sort_columns, int *to_sort_columns)
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

	d->to_sort_columns = to_sort_columns;
	d->num_to_sort_columns = num_to_sort_columns;

	if ((err = data_sort_callback(d, num_to_sort_columns, to_sort_columns, data_sort_cb, sorted_outf)))
		goto out;

	if (d->tmp)
	{
		fclose(d->tmp);
		d->tmp = NULL;

		fclose(sorted_outf);
		sorted_outf = NULL;

		remove(d->filename);
		if (rename(sorted_name,d->filename) == -1)
		{
			fprintf(stderr,"Couldn't rename\n");
			goto out;
		}

		if (!(d->tmp = fopen(d->filename,"r+")))
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

int data_sort_v(data_t *d, int cols, ...)
{
	int i;
	int err = -1;
	int to_sort_cols[cols];

	va_list vl;
	va_start(vl,cols);

	for (i=0;i<cols;i++)
		to_sort_cols[i] = va_arg(vl,int);

	err = data_sort(d,cols,to_sort_cols);

	va_end(vl);
	return err;
}

static int data_stat_callback(data_t *d, int (*callback)(uint32_t ps, uint32_t ns, uint32_t tps, uint32_t fps, void *userdata), void *user_data, int label_col, int cols, int *to_sort_cols)
{
	int r;
	int err = -1;
	uint32_t tps = 0;

	d->label_col = label_col;

	if ((err = data_sort(d,cols,to_sort_cols)))
		goto out;

	uint32_t positives = d->label_sum;
	uint32_t negatives = d->num_rows - positives;

	for (r=0; r < d->num_rows; r++)
	{
		int32_t l;

		if ((err = data_get_entry_as_int32(&l,d,r,label_col)))
			goto out;

		tps += l > 0;
		uint32_t fps = (r+1) - tps;

		callback(positives,negatives,tps,fps,user_data);
	}
	err = 0;
out:
	if (err) fprintf(stderr,"Stats err=%d\n",err);
	return err;
}


/**************************************************************/

static int data_stat_with_hist_callback(uint32_t ps, uint32_t ns, uint32_t tps, uint32_t fps, void *userdata)
{
	data_t *d = (data_t*)userdata;
	double tpr = (double)tps / ps; /* true positive rate */
	double fpr = (double)fps / ns; /* false positive rate */
	double prec = (double)tps / (tps + fps); /* precision = true positives / (number of all positives = (true positives + false positives) */
	double recall = (double)tps / ps; /* recall = number of true positives / (true positives + false negatives = all positive samples) */

	hist_put(&d->roc, fpr, tpr);
	hist_put(&d->precall, recall, prec);

	return 0;
}

int data_stat_hist(data_t *d, int breaks, int label_col, int cols, int *to_sort_cols)
{
	int err = -1;

	if ((err = hist_init(&d->roc,breaks)))
		goto out;
	if ((err = hist_init(&d->precall,breaks)))
		goto out;
	d->hist_initialized = 1;
	if ((err = data_stat_callback(d, data_stat_with_hist_callback, d, label_col, cols, to_sort_cols)))
		goto out;

	/* Average */
	hist_average(&d->roc);
	hist_average(&d->precall);

	err = 0;
out:
	if (err) fprintf(stderr,"err = %d\n",err);
	return err;
}

int data_stat_hist_v(data_t *d, int breaks, int label_col, int cols, ...)
{
	int i;
	int err = -1;
	int to_sort_cols[cols];

	va_list vl;
	va_start(vl,cols);

	for (i=0;i<cols;i++)
		to_sort_cols[i] = va_arg(vl,int);

	err = data_stat_hist(d, breaks, label_col, cols, to_sort_cols);

	va_end(vl);
	return err;
}

/**
 * Returns the precision value for the given recall.
 *
 * @param precision
 * @param d
 * @param recall
 * @return
 */
int data_get_precision_by_recall(double *precision, data_t *d, double recall)
{
	int err = -1;

	if (!d->hist_initialized)
		goto out;

	*precision = hist_get_y(&d->precall,recall);
	err = 0;
out:
	return err;
}

/**
 * Returns the true positive rate by the given false positive rate.
 *
 * @param tpr
 * @param d
 * @param fpr
 * @return
 */
int data_get_tpr_by_fpr(double *tpr, data_t *d, double fpr)
{
	int err = -1;

	if (!d->hist_initialized)
		goto out;

	*tpr = hist_get_y(&d->roc,fpr);
	err = 0;
out:
	return err;

}
