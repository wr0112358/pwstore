/*
Copyright (C) 2014 Reiter Wolfgang wr0112358@gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
    const decltype(from.username.length()) max_length{255};
    {
        to.username_used = std::min(max_length, from.username.length());
        std::strncpy(const_cast<char *>(from.username.c_str()), to.username,
                     to.username_used);
        to.username[to.username_used] = '\0';
    }
    {
        to.url_used = std::min(max_length, from.url_string.length());
        std::strncpy(const_cast<char *>(from.url_string.c_str()), to.url,
                     to.url_used);
        to.url[to.url_used] = '\0';
    }
    {
        to.passwd_used = std::min(max_length, from.password.length());
        std::strncpy(const_cast<char *>(from.password.c_str()), to.passwd,
                     to.passwd_used);
        to.passwd[to.passwd_used] = '\0';
    }
}

static void convert(const dataset &from, pw_store::data_type &to)
{
    to.username.assign(from.username);
    to.url_string.assign(from.url);
    to.password.assign(from.passwd);
}

static void convert(const size_t *from, size_t from_length,
                    std::vector<pw_store::data_type::id_type> &to)
{
    to.resize(from_length);
    for(size_t i = 0; i < from_length; i++)
        to.push_back(from[i]);
}

pwstore_handle pwstore_create(const char *password, size_t password_length,
                              const char *db_file, size_t db_file_length)
{
    (void)password_length;
    (void)db_file_length;
    pwstore_c *obj = new pwstore_c;
    obj->db = new pw_store_api_cxx::pwstore_api(std::string(db_file),
                                                std::string(password));
    if(!*obj->db)
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
    (void)lookup_key_length;
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
    matches_cxx;

    std::vector<pw_store::data_type::id_type> uids_cxx;
    if(uids && uids_length)
        convert(uids, uids_length, uids_cxx);
    if(!obj->db->lookup(matches_cxx, std::string(lookup_key), uids_cxx)) {
        *matches_real_length = 0;
        return false;
    }

    size_t cnt = 0;
    matches_cxx.resize(std::min(matches_cxx.size(), matches_length));
    for(const auto &match : matches_cxx)
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

    std::vector<pw_store::data_type::id_type> uids_cxx{uid};
    if(!obj->db->remove(uids_cxx))
        return false;

    return true;
}

bool pwstore_change_password(pwstore_handle handle, const char *new_password,
                             size_t new_password_length)
{
    (void)new_password_length;
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

/*
bool pwstore_dump(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return obj->db->dump();
}
*/

bool pwstore_sync(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return obj->db->sync();
}

void pwstore_lock(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    obj->db->lock();
}

bool pwstore_unlock(pwstore_handle handle, const char *password)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return obj->db->unlock(std::string(password));
}

bool pwstore_state(pwstore_handle handle)
{
    pwstore_c *obj = reinterpret_cast<pwstore_c *>(handle);
    return *obj->db;
}
