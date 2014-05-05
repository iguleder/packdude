#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

#include "log.h"
#include "archive.h"
#include "package.h"

result_t _read_package(const char *path,
                       unsigned char **contents,
                       size_t *size) {
	/* the package attributes */
	struct stat attributes = {0};

	/* the package file descriptor */
	int fd = (-1);

	/* the return value */
	result_t result = RESULT_IO_ERROR;

	/* get the package size */
	if (-1 == stat(path, &attributes)) {
		goto end;
	}
	*size = (size_t) attributes.st_size;

	/* make sure the package size is at bigger than the header size */
	if (sizeof(package_header_t) >= *size) {
		result = RESULT_CORRUPT_DATA;
		goto end;
	}

	/* open the package */
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		goto end;
	}

	/* allocate memory for the package contents */
	*contents = malloc(*size);
	if (NULL == *contents) {
		result = RESULT_MEM_ERROR;
		goto close_package;
	}

	/* read the package contents */
	if ((ssize_t) *size != read(fd, *contents, *size)) {
		goto free_contents;
	}

	/* report success */
	result = RESULT_OK;
	goto close_package;

free_contents:
	/* free the package contents */
	free(contents);

close_package:
	/* close the package */
	(void) close(fd);

end:
	return result;
}

result_t package_verify(const char *path) {
	/* the package contents */
	unsigned char *contents = NULL;

	/* the archive contained in the package */
	unsigned char *archive = NULL;

	/* the package size */
	size_t size = 0;

	/* the return value */
	result_t result = RESULT_CORRUPT_DATA;

	log_write(LOG_INFO, "Verifying the integrity of %s\n", path);

	/* read the package */
	result = _read_package(path, &contents, &size);
	if (RESULT_OK != result) {
		goto end;
	}

	/* locate the archive and calculate its size */
	archive = contents + sizeof(package_header_t);
	size -= sizeof(package_header_t);

	/* verify the package is targeted at the running package manager version */
	if (VERSION != ((package_header_t *) contents)->version) {
		result = RESULT_INCOMPATIBLE;
		goto free_contents;
	}

	/* verify the package checksum */
	if ((mz_ulong) (((package_header_t *) contents)->checksum) != mz_crc32(
	                                                              MZ_CRC32_INIT,
	                                                              archive,
	                                                              size)) {
		log_write(LOG_ERROR,
		          "%s is corrupt; the checksum is incorrect\n",
		          path);
		result = RESULT_CORRUPT_DATA;
		goto free_contents;
	}

	/* report success */
	result = RESULT_OK;

free_contents:
	/* free the package contents */
	free(contents);

end:
	return result;
}


result_t _register_file(const char *path, file_register_params_t *params) {
	return database_register_path(params->database, path, params->package);
}

result_t package_install(const char *name,
                         const char *path,
                         database_t *database) {
	/* the callback parameters */
	file_register_params_t params = {0};

	/* the package contents */
	unsigned char *contents = NULL;

	/* the archive contained in the package */
	unsigned char *archive = NULL;

	/* the package size */
	size_t size = 0;

	/* the return value */
	result_t result = RESULT_CORRUPT_DATA;

	log_write(LOG_INFO, "Unpacking %s\n", name);

	/* read the package */
	result = _read_package(path, &contents, &size);
	if (RESULT_OK != result) {
		goto end;
	}

	/* locate the archive and calculate its size */
	archive = contents + sizeof(package_header_t);
	size -= sizeof(package_header_t);

	/* extract the archive */
	params.package = name;
	params.database = database;
	result = archive_extract(archive,
	                         size,
	                         (file_callback_t) _register_file,
	                         &params);
	if (RESULT_OK != result) {
		goto free_contents;
	}

	/* report success */
	result = RESULT_OK;

free_contents:
	/* free the package contents */
	free(contents);

end:
	return result;
}

int _remove_file(database_t *database, int count, char **values, char **names) {
	/* the file attributes */
	struct stat attributes = {0};

	/* the return value */
	int abort = 0;

	log_write(LOG_DEBUG, "Removing %s\n", values[FILE_FIELD_PATH]);

	/* determine the file type - if it doesn't exist, it's fine */
	if (-1 == lstat(values[FILE_FIELD_PATH], &attributes)) {
		if (ENOENT != errno) {
			abort = 1;
		}
		goto end;
	}

	/* delete the file */
	if (S_ISDIR(attributes.st_mode)) {
		if (-1 == rmdir(values[FILE_FIELD_PATH])) {
			if (ENOTEMPTY != errno) {
				log_write(LOG_ERROR,
				          "Failed to remove %s\n",
				          values[FILE_FIELD_PATH]);
				abort = 1;
			}
		}
	} else {
		if (-1 == unlink(values[FILE_FIELD_PATH])) {
			log_write(LOG_ERROR,
			          "Failed to remove %s\n",
			          values[FILE_FIELD_PATH]);
			abort = 1;
		}
	}

	/* unregister the file */
	if (RESULT_OK != database_unregister_path(database,
	                                          values[FILE_FIELD_PATH])) {
		abort = 1;
	}

end:
	return abort;
}

result_t package_remove(const char *name, database_t *database) {
	/* the return value */
	result_t result = RESULT_OK;

	/* delete the package files */
	result = database_for_each_file(database,
	                                name,
	                                (query_callback_t) _remove_file,
	                                database);
	if (RESULT_OK != result) {
		goto end;
	}

	/* unregister the package */
	result = database_remove_installation_data(database, name);
	if (RESULT_OK != result) {
		goto end;
	}

	/* report success */
	result = RESULT_OK;

end:
	return result;
}