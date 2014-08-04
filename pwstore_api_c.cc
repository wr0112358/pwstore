/*
typedef void *pwstore_handle;

struct dataset
{
    char *username;
    size_t username_length;
    char *url;
    size_t url_length;
    char *passwd;
    size_t passwd_length;
};

// type (*function)(argtypes);
typedef void (*provide_password_callback)(pwstore_handle, const char *, size_t);

*/

#include "pwstore_api_c.h"
#include "pwstore_api_cxx.hh"

#include <algorithm>
#include <cstring>

struct pwstore_c
{
    pw_store_api_cxx::pwstore_api *db;
};

static void convert(const pw_store::data_type &from, dataset &to)
{
    // TODO: passwd > 255
    {
        to.username_used = std::min(255lu, from.username.length());
        std::strncpy(const_cast<char *>(from.username.c_str()), to.username,
                     to.username_used);
        to.username[to.username_used] = '\0';
    }
    {
        to.url_used = std::min(255lu, from.url_string.length());
        std::strncpy(const_cast<char *>(from.url_string.c_str()), to.url, to.url_used);
        to.url[to.url_used] = '\0';
    }
    {
        to.passwd_used = std::min(255lu, from.password.length());
        std::strncpy(const_cast<char *>(from.password.c_str()), to.passwd, to.passwd_used);
        to.passwd[to.passwd_used] = '\0';
    }
}

static void convert(const dataset &from, pw_store::data_type &to)
{
    to.username.assign(from.username);
    to.url_string.assign(from.url);
    to.password.assign(from.passwd);
}

/*
static void convert(const std::vector<pw_store::data_type::id_type> &from,
                    size_t *to, size_t to_length)
{}
*/

static void convert(const size_t *from, size_t from_length,
                    std::vector<pw_store::data_type::id_type> &to)
{
    to.resize(from_length);
    for(size_t i = 0; i < from_length; i++)
        to.push_back(from[i]);
}

// caller owns the ressources. all internally allocated ressources are free on
// pwstore_exit.
pwstore_handle pwstore_create(const char *password, size_t password_length,
                              const char *db_file, size_t db_file_length)
{
    pwstore_c *obj = new pwstore_c;
    obj->db = new pw_store_api_cxx::pwstore_api(std::string(db_file),
                                                std::string(password));
    if (!*obj->db)
        return nullptr;

    return reinterpret_cast<pwstore_handle>(obj);
}

void pwstore_exit(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    delete obj->db;
    delete obj;
}

bool pwstore_add(pwstore_handle handle, const struct dataset *data)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    pw_store::data_type date;
    convert(*data, date);
    return obj->db->add(date);
}

bool pwstore_lookup(pwstore_handle handle, struct dataset **matches,
                    size_t matches_length, size_t *matches_real_length,
                    const char *lookup_key, size_t lookup_key_length,
                    size_t *uids, size_t uids_length)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>> matches_cxx;

    std::vector<pw_store::data_type::id_type> uids_cxx;
    if(uids && uids_length)
        convert(uids, uids_length, uids_cxx);
    if(!obj->db->lookup(matches_cxx, std::string(lookup_key), uids_cxx)) {
        *matches_real_length = 0;
        return false;
    }

    size_t cnt = 0;
    matches_cxx.resize(std::min(matches_cxx.size(), matches_length));
    for(const auto &match: matches_cxx)
        convert(std::get<1>(match), *matches[cnt++]);
    *matches_real_length = matches_cxx.size();

    return true;
}

bool pwstore_get(pwstore_handle handle, struct dataset *match, size_t uid)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    pw_store::data_type date;
    if(!obj->db->get(uid, date))
        return false;
    convert(date, *match);

    return true;
}

bool pwstore_remove_multiple(pwstore_handle handle, size_t *uids,
                             size_t uids_length)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    std::vector<pw_store::data_type::id_type> uids_cxx;
    convert(uids, uids_length, uids_cxx);
    if(!obj->db->remove(uids_cxx))
        return false;

    return true;
}

bool pwstore_remove_one(pwstore_handle handle, size_t uid)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);

    std::vector<pw_store::data_type::id_type> uids_cxx{ uid };
    if(!obj->db->remove(uids_cxx))
        return false;

    return true;
}

bool pwstore_change_password(pwstore_handle handle, const char *new_password,
                             size_t new_password_length)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);

    return obj->db->change_password(std::string(new_password));
}

bool pwstore_gen_entry(pwstore_handle handle, const char *username,
                       const char *url_string, char *password,
                       size_t password_length, size_t *password_real_length)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);

    std::string pw_buff;
    if(!obj->db->gen_passwd(std::string(username), std::string(url_string),
                            pw_buff)) {
        password_real_length = 0;
        return false;
    }
    if(pw_buff.length() >= password_length) {
        *password_real_length = pw_buff.length();
        password[0] = '\0';
        return true;
    }

    std::strcpy(const_cast<char *>(pw_buff.c_str()), password);

    return true;
}

bool pwstore_dump(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return obj->db->dump();
}

bool pwstore_sync(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return obj->db->sync();
}

bool pwstore_state(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return *obj->db;
}
