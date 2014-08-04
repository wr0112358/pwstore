#ifndef _PWSTORE_API_C_
#define _PWSTORE_API_C_

#include <stdbool.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef void *pwstore_handle;

/*
struct dataset
{
    char *username;
    size_t username_length;
    char *url;
    size_t url_length;
    char *passwd;
    size_t passwd_length;
};
*/

struct dataset
{
	/* Making my life easier: gui won't display longer lines anyway..
	   if a password should really be longer, return (0,0,0) for now. */
    char username[256];
    size_t username_used;
    char url[256];
    size_t url_used;
    char passwd[256];
    size_t passwd_used;
};

typedef void (*provide_password_callback)(pwstore_handle, const char *, size_t);

/* caller owns the ressources. all internally allocated ressources are freed on
   pwstore_exit. */
pwstore_handle pwstore_create(const char *password, size_t password_length,
                              const char *db_file, size_t db_file_length);
void pwstore_exit(pwstore_handle handle);

bool pwstore_add(pwstore_handle handle, const struct dataset *data);
/* call with preallocated array of struct dataset with size matches_length
 the number of found matches are stored in matches_real_length
 either lookup_key or uids can be NULL at the moment. */
bool pwstore_lookup(pwstore_handle handle, struct dataset **matches,
                    size_t matches_length, size_t *matches_real_length,
                    provide_password_callback callback, const char *lookup_key,
                    size_t lookup_key_length, size_t *uids, size_t uids_length);
bool pwstore_get(pwstore_handle handle, struct dataset *match, size_t uid);
bool pwstore_remove_multiple(pwstore_handle handle, size_t *uids,
                             size_t uids_length);
bool pwstore_remove_one(pwstore_handle handle, size_t uid);

bool pwstore_change_password(pwstore_handle handle, const char *new_password,
                             size_t new_password_length);
bool pwstore_gen_entry(pwstore_handle handle, const char *username,
                       const char *url_string,
                       provide_password_callback callback);
bool pwstore_dump(pwstore_handle handle);

bool pwstore_sync(pwstore_handle handle);
bool pwstore_state(pwstore_handle handle);

#ifdef __cplusplus
}
#endif

#endif
