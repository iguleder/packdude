#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "log.h"
#include "fetch.h"

/* the number of fetchers */
unsigned int g_fetcher_count = 0;

result_t fetcher_new(fetcher_t *fetcher) {
	/* the return value */
	result_t result = RESULT_MEM_ERROR;

	assert(NULL != fetcher);

	/* if this is the first fetcher, initialize libcurl */
	if (0 == g_fetcher_count) {
		if (0 != curl_global_init(CURL_GLOBAL_NOTHING)) {
			goto end;
		}
	}

	/* initialize an "easy" libcurl session */
	fetcher->handle = curl_easy_init();
	if (NULL == fetcher->handle) {
		goto cleanup;
	}

	/* set common options */
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_USERAGENT,
	                                 FETCHER_USER_AGENT)) {
		goto cleanup;
	}
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_TCP_NODELAY,
	                                 1)) {
		goto cleanup;
	}

	/* increment the fetcher counter */
	++g_fetcher_count;

	/* report success */
	result = RESULT_OK;
	goto end;

cleanup:
	/* free memory used by libcurl */
	curl_global_cleanup();

end:
	return result;
}

size_t _append_to_file(const void *ptr, size_t size, size_t nmemb, FILE *file) {
	assert(NULL != ptr);
	assert(NULL != file);

	log_write(LOG_DEBUG, "Received a chunk of %u bytes\n", (size * nmemb));
	return fwrite(ptr, size, nmemb, file);
}

result_t fetcher_fetch_to_file(fetcher_t *fetcher,
                               const char *url,
                               FILE *destination) {
	/* the return value */
	result_t result = RESULT_MEM_ERROR;

	assert(NULL != fetcher);
	assert(NULL != fetcher->handle);
	assert(NULL != url);
	assert(NULL != destination);

	log_write(LOG_DEBUG, "Fetching %s\n", url);

	/* set the input URL and the output file */
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_WRITEFUNCTION,
	                                 _append_to_file)) {
		goto end;
	}
	if (CURLE_OK != curl_easy_setopt(fetcher->handle, CURLOPT_URL, url)) {
		goto end;
	}
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_WRITEDATA,
	                                 destination)) {
		goto end;
	}

	/* fetch the URL */
	if (CURLE_OK != curl_easy_perform(fetcher->handle)) {
		result = RESULT_NETWORK_ERROR;
		goto end;
	}

	/* flush the output file */
	if (0 != fflush(destination)) {
		result = RESULT_IO_ERROR;
		goto end;
	}

	/* report success */
	result = RESULT_OK;

end:
	return result;
}

size_t _append_to_buffer(const void *ptr,
                         size_t size,
                         size_t nmemb,
                         void *buffer) {
	/* the enlarged buffer */
	fetcher_buffer_t new_buffer = {0};

	/* the number of available bytes */
	size_t bytes_available = 0;

	/* the number of bytes handled */
	size_t bytes_handled = 0;

	assert(NULL != ptr);
	assert(NULL != buffer);

	/* calculate the number of available bytes */
	bytes_available = size * nmemb;

	/* enlarge the buffer */
	new_buffer.size = ((fetcher_buffer_t *) buffer)->size + bytes_available;
	new_buffer.buffer = realloc(((fetcher_buffer_t *) buffer)->buffer,
	                            new_buffer.size);
	if (NULL == new_buffer.buffer) {
		free(((fetcher_buffer_t *) buffer)->buffer);
		goto end;
	}

	/* copy the data to the buffer */
	(void) memcpy(&new_buffer.buffer[((fetcher_buffer_t *) buffer)->size],
	              ptr,
	              bytes_available);
	(void) memcpy(buffer, &new_buffer, sizeof(fetcher_buffer_t));

	/* report success */
	bytes_handled = bytes_available;

end:
	return bytes_handled;
}

result_t fetcher_fetch_to_memory(fetcher_t *fetcher,
                                 const char *url,
                                 fetcher_buffer_t *buffer) {
	/* the return value */
	result_t result = RESULT_MEM_ERROR;

	assert(NULL != fetcher);
	assert(NULL != fetcher->handle);
	assert(NULL != url);
	assert(NULL != buffer);

	/* set the input URL and the output file */
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_WRITEFUNCTION,
	                                 _append_to_buffer)) {
		goto end;
	}
	if (CURLE_OK != curl_easy_setopt(fetcher->handle, CURLOPT_URL, url)) {
		goto end;
	}
	if (CURLE_OK != curl_easy_setopt(fetcher->handle,
	                                 CURLOPT_WRITEDATA,
	                                 buffer)) {
		goto end;
	}

	/* fetch the URL */
	buffer->buffer = NULL;
	buffer->size = 0;
	if (CURLE_OK != curl_easy_perform(fetcher->handle)) {
		result = RESULT_NETWORK_ERROR;
		goto end;
	}

	/* report success */
	result = RESULT_OK;

end:
	return result;
}

void fetcher_free(fetcher_t *fetcher) {
	assert(NULL != fetcher);
	assert(NULL != fetcher->handle);
	assert(0 < g_fetcher_count);

	/* end the libcurl session */
	curl_easy_cleanup(fetcher->handle);

	/* decrement the fetcher count */
	--g_fetcher_count;

	/* if this is the last fetcher, free all memory used by libcurl globally */
	if (0 == g_fetcher_count) {
		curl_global_cleanup();
	}
}
