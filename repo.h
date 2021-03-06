#ifndef _REPO_H_INCLUDED
#	define _REPO_H_INCLUDED

#	include <stdio.h>
#	include "result.h"
#	include "database.h"
#	include "fetch.h"

/*!
 * @defgroup repo Repository
 * @brief Repository handling
 * @{ */

/*!
 * @def REPO_DATABASE_FILE_NAME
 * @brief The file name of a repository metadata database */
#	define REPO_DATABASE_FILE_NAME "repo.sqlite3"

/*!
 * @def METADATA_DATABASE_PATH_FORMAT
 * @brief The path the repository metadata database gets downloaded to
 * @see REPO_DATABASE_FILE_NAME */
#	define METADATA_DATABASE_PATH_FORMAT "."VAR_DIR"/packdude/repo-%lu.sqlite3"

/*!
 * @def MAX_METADATA_CACHE_AGE
 * @brief The maximum age of the repository metadata database cache, in
 *        seconds */
#	define MAX_METADATA_CACHE_AGE (3600)

/*!
 * @struct repo_t
 * @brief A repository */
typedef struct {
	const char *url; /*!< The repository base URL */
	fetcher_t fetcher; /*!< A fetcher used to fetch files from the repository */
} repo_t;

/*!
 * @fn result_t repo_open(repo_t *repo, const char *url)
 * @brief Connects to a repository
 * @param repo A repository
 * @param url The repository base URL
 * @see repo_close */
result_t repo_open(repo_t *repo, const char *url);

/*!
 * @fn void repo_close(repo_t *repo);
 * @brief Disconnects from a repository
 * @param repo A repository
 * @see repo_open */
void repo_close(repo_t *repo);

/*!
 * @fn result_t repo_get_database(repo_t *repo, database_t *database)
 * @brief Fetches the metadata database of a repository
 * @param repo A repository
 * @param database A database */
result_t repo_get_database(repo_t *repo, database_t *database);

/*!
 * @fn result_t repo_get_package(repo_t *repo,
 *                               const package_info_t *info,
 *                               fetcher_buffer_t *buffer)
 * @brief Fetches the a package from a repository
 * @param repo A repository
 * @param info The package metadata
 * @param buffer The output buffer */
result_t repo_get_package(repo_t *repo,
                          const package_info_t *info,
                          fetcher_buffer_t *buffer);

/*!
 * @} */

#endif
