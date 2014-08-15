#include "pwstore_api_c.h"

#include <string.h>

int main()
{
	const char *pw = "aa";
	const size_t pw_length = strlen(pw);
	const char *db = ".c_api_test.crypt";
	const size_t db_length = strlen(db);
	pwstore_handle handle = pwstore_create(pw, pw_length, db, db_length);
	if(!handle)
		puts("pwstore_create failed");


	struct dataset data[] = {
		{ "user1", strlen("user1"), "url1", strlen("url1"), "pw1", strlen("pw1") },
		{ "user2", strlen("user2"), "url2", strlen("url2"), "pw2", strlen("pw2") },
		{ "user3", strlen("user3"), "url3", strlen("url3"), "pw3", strlen("pw3") }
	};
	if(!pwstore_add(handle, data))
		puts("pw_store_add failed");

	if(!pwstore_dump(handle))
		puts("pwstore_dump failed");

	pwstore_exit(handle);

	return 0;
}
