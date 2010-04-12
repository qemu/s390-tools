/*
 * s390-tools/zipl/src/misc.c
 *   Miscellaneous helper functions.
 *
 * Copyright IBM Corp. 2001, 2006.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#include "misc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "error.h"


/* Allocate SIZE bytes of memory. Upon success, return pointer to memory.
 * Return NULL otherwise. */
void *
misc_malloc(size_t size)
{
	void* result;

	result = malloc(size);
	if (result == NULL) {
		error_reason("Could not allocate %lld bytes of memory",
			     (unsigned long long) size);
	}
	return result;
}


/* Allocate N * SIZE bytes of memory. Upon success, return pointer to memory.
 * Return NULL otherwise. */
void *
misc_calloc(size_t n, size_t size)
{
	void* result;

	result = calloc(n, size);
	if (result == NULL) {
		error_reason("Could not allocate %lld bytes of memory",
			     (unsigned long long) n *
			     (unsigned long long) size);
	}
	return result;
}


/* Duplicate the given string S. Upon success, return pointer to new string.
 * Return NULL otherwise. */
char *
misc_strdup(const char* s)
{
	char* result;

	result = strdup(s);
	if (result == NULL) {
		error_reason("Could not allocate %lld bytes of memory",
			     (unsigned long long) strlen(s) + 1);
	}
	return result;
}


/* Read COUNT bytes of data from file identified by file descriptor FD to
 * memory at location BUFFER. Return 0 when all bytes were successfully read,
 * non-zero otherwise. */
int
misc_read(int fd, void* buffer, size_t count)
{
	size_t done;
	ssize_t rc;

	for (done=0; done < count; done += rc) {
		rc = read(fd, VOID_ADD(buffer, done), count - done);
		if (rc == -1) {
			error_reason(strerror(errno));
			return -1;
		}
		if(rc == 0) {
			error_reason("Reached unexpected end of file");
			return -1;
		}
	}
	return 0;
}


/* Read all of file FILENAME to memory. Upon success, return 0 and set BUFFER
 * to point to the data and SIZE (if non-NULL) to contain the file size.
 * If NIL_TERMINATE is non-zero, a nil-char will be added to the buffer string
 * Return non-zero otherwise. */
int
misc_read_file(const char* filename, char** buffer, size_t* size,
	       int nil_terminate)
{
	struct stat stats;
	void* data;
	int fd;
	int rc;

	if (stat(filename, &stats)) {
		error_reason(strerror(errno));
		return -1;
	}
	if (!S_ISREG(stats.st_mode)) {
		error_reason("Not a regular file");
		return -1;
	}
	data = misc_malloc(stats.st_size + (nil_terminate ? 1 : 0));
	if (data == NULL)
		return -1;
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		error_reason(strerror(errno));
		free(data);
		return -1;
	}
	rc = misc_read(fd, data, stats.st_size);
	close(fd);
	if (rc) {
		free(data);
		return rc;
	}
	*buffer = data;
	if (size != NULL)
		*size = stats.st_size;
	if (nil_terminate) {
		if (size != NULL)
			(*size)++;
		(*buffer)[stats.st_size] = 0;
	}
	return 0;
}


#define INITIAL_FILE_BUFFER_SIZE	1024

/* Read file into buffer without querying its size (necessary for reading files
 * from /proc or /sys). Upon success, return zero and set BUFFER to point to
 * the file buffer and SIZE (if non-null) to contain the file size. Return
 * non-zero otherwise. Add a null-byte at the end of the buffer if
 * NIL_TERMINATE is non-zero.  */
int
misc_read_special_file(const char* filename, char** buffer, size_t* size,
		       int nil_terminate)
{
	FILE* file;
	char* data;
	char* new_data;
	size_t count;
	size_t current_size;
	int current;

	file = fopen(filename, "r");
	if (file == NULL) {
		error_reason(strerror(errno));
		return -1;
	}
	current_size = INITIAL_FILE_BUFFER_SIZE;
	count = 0;
	data = (char *) misc_malloc(current_size);
	if (data == NULL) {
		fclose(file);
		return -1;
	}
	current = fgetc(file);
	while (current != EOF || nil_terminate) {
		if (current == EOF) {
			current = 0;
			nil_terminate = 0;
		}
		data[count++] = (char) current;
		if (count >= current_size) {
			new_data = (char *) misc_malloc(current_size * 2);
			if (new_data == NULL) {
				free(data);
				fclose(file);
				return -1;
			}
			memcpy(new_data, data, current_size);
			free(data);
			data = new_data;
			current_size *= 2;
		}
		current = fgetc(file);
	}
	fclose(file);
	*buffer = data;
	if (size)
		*size = count;
	return 0;
}


/* Get contents of file identified by FILENAME and fill in the respective
 * fields of FILE. Return 0 on success, non-zero otherwise. */
int
misc_get_file_buffer(const char* filename, struct misc_file_buffer* file)
{
	int rc;

	rc = misc_read_file(filename, &file->buffer, &file->length, 0);
	file->pos = 0;
	return rc;
}


/* Free resources allocated for file buffer FILE. */
void
misc_free_file_buffer(struct misc_file_buffer* file)
{
	if (file->buffer != NULL) {
		free(file->buffer);
		file->buffer = NULL;
		file->pos = 0;
		file->length = 0;
	}
}


/* Return character at current FILE buffer position plus READAHEAD or EOF if
 * at end of file. */
int
misc_get_char(struct misc_file_buffer* file, off_t readahead)
{
	if (file->buffer != NULL)
		if ((size_t) (file->pos + readahead) < file->length)
			return file->buffer[file->pos + readahead];
	return EOF;
}


char*
misc_make_path(char* dirname, char* filename)
{
	char* result;
	size_t len;

	len = strlen(dirname) + strlen(filename) + 2;
	result = (char *) misc_malloc(len);
	if (result == NULL)
		return NULL;
	sprintf(result, "%s/%s", dirname, filename);
	return result;
}


#define TEMP_DEV_MAX_RETRIES	1000

int
misc_temp_dev(dev_t dev, int blockdev, char** devno)
{
	char* result;
	char* pathname[] = { "/dev", getenv("TMPDIR"), "/tmp",
			     getenv("HOME"), "." , "/"};
	char filename[] = "zipl0000";
	mode_t mode;
	unsigned int path;
	int retry;
	int rc;
	int fd;

	if (blockdev)
		mode = S_IFBLK | S_IRWXU;
	else
		mode = S_IFCHR | S_IRWXU;
	/* Try several locations as directory for the temporary device
	 * node. */
	for (path=0; path < sizeof(pathname) / sizeof(pathname[0]); path++) {
		if (pathname[path] == NULL)
			continue;
		for (retry=0; retry < TEMP_DEV_MAX_RETRIES; retry++) {
			sprintf(filename, "zipl%04d", retry);
			result = misc_make_path(pathname[path], filename);
			if (result == NULL)
				return -1;
			rc = mknod(result, mode, dev);
			if (rc == 0) {
				/* Need this test to cover 'nodev'-mounted
				 * filesystems. */
				fd = open(result, O_RDWR);
				if (fd != -1) {
					close(fd);
					*devno = result;
					return 0;
				}
				remove(result);
				retry = TEMP_DEV_MAX_RETRIES;
			} else if (errno != EEXIST)
				retry = TEMP_DEV_MAX_RETRIES;
			free(result);
		}
	}
	error_text("Unable to create temporary device node");
	error_reason(strerror(errno));
	return -1;
}


/* Create a temporary device node for the device containing FILE. Upon
 * success, return zero and store a pointer to the name of the device node
 * file into DEVNO. Return non-zero otherwise. */
int
misc_temp_dev_from_file(char* file, char** devno)
{
	struct stat stats;

	if (stat(file, &stats)) {
		error_reason(strerror(errno));
		return -1;
	}
	return misc_temp_dev(stats.st_dev, 1, devno);
}


/* Delete temporary device node DEVICE and free memory allocated for device
 * name. */
void
misc_free_temp_dev(char* device)
{
	if (remove(device)) {
		fprintf(stderr, "Warning: Could not remove "
				"temporary file %s: %s",
				device, strerror(errno));
	}
	free(device);
}


/* Write COUNT bytes from memory at location DATA to the file identified by
 * file descriptor FD. Return 0 when all bytes were successfully written,
 * non-zero otherwise. */
int
misc_write(int fd, const void* data, size_t count)
{
	size_t written;
	ssize_t rc;

	for (written=0; written < count; written += rc) {
		rc = write(fd, VOID_ADD(data, written), count - written);
		if (rc == -1) {
			error_reason(strerror(errno));
			return -1;
		}
		if (rc == 0) {
			error_reason("Write failed");
			return -1;
		}
	}
	return 0;
}


int
misc_check_writable_directory(const char* directory)
{
	struct stat stats;

	if (stat(directory, &stats)) {
		error_reason(strerror(errno));
		return -1;
	}
	if (!S_ISDIR(stats.st_mode)) {
		error_reason("Not a directory");
		return -1;
	}
	if (access(directory, W_OK)) {
		error_reason(strerror(errno));
		return -1;
	}
	return 0;
}


int
misc_check_readable_file(const char* filename)
{
	struct stat stats;

	if (stat(filename, &stats)) {
		error_reason(strerror(errno));
		return -1;
	}
	if (!S_ISREG(stats.st_mode)) {
		error_reason("Not a regular file");
		return -1;
	}
	if (access(filename, R_OK)) {
		error_reason(strerror(errno));
		return -1;
	}
	return 0;
}


int
misc_check_writable_device(const char* devno, int blockdev, int chardev)
{
	struct stat stats;

	if (stat(devno, &stats)) {
		error_reason(strerror(errno));
		return -1;
	}
	if (blockdev && chardev) {
		if (!(S_ISCHR(stats.st_mode) || S_ISBLK(stats.st_mode))) {
			error_reason("Not a device");
			return -1;
		}
	} else if (blockdev) {
		if (!S_ISBLK(stats.st_mode)) {
			error_reason("Not a block device");
			return -1;
		}
	} else if (chardev) {
		if (!S_ISCHR(stats.st_mode)) {
			error_reason("Not a character device");
			return -1;
		}
	}
	if (access(devno, W_OK)) {
		error_reason(strerror(errno));
		return -1;
	}
	return 0;
}


