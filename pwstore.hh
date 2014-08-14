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

#ifndef _PWSTORE_HH_
#define _PWSTORE_HH_

#include <algorithm>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <tuple>

namespace pw_store
{

// stores key value tuples in a string
// - where keys are url-strings and usernames. values are usernames and
// passwords.
// - per entry 3 delim characters must exist
// format is:
//   entry = URL DELIM USERNAME DELIM PASSWORD DELIM
//   file = entry*
//   URL/USERNAME/PASSWORD can be any readable string or EMPTY
//   - a line consists at least of: DELIM DELIM DELIM NEWLINE

struct data_type
{
    using id_type = std::size_t;
    data_type() : data_type("", "", "") {}
    data_type(const std::string &url, const std::string &user,
              const std::string &pass)
        : url_string(url), username(user), password(pass)
    {
    }

    friend std::ostream &operator<<(std::ostream &os, const data_type &date);
    std::string to_string() const;

    std::string url_string;
    std::string username;
    std::string password;
};

inline std::string data_type::to_string() const
{
    return std::string("(\"") + url_string + std::string("\", \"") + username +
           std::string("\", \"") + "***" + std::string("\")");
}

inline std::ostream &operator<<(std::ostream &os, const data_type &date)
{
    return os << date.to_string();
}

class database
{
public:
    const std::string DELIM = {'\t'};

public:
    // Create database object from string buffer. No copying involved.
    explicit database(std::string &buffer) : dirty(false), string_buffer(buffer)
    {
    }

    ~database() { clear_all_buffers(); }
    // Parse the provided buffer.
    bool parse();
    bool insert(const data_type &date);
    // lookup performs an iterative search over all keys and returns matches
    // together with an unique id. This id is invalidated after add or delete
    // operations.
    void lookup(const std::string &key,
                std::list<std::tuple<data_type::id_type, data_type>> &matches);
    void synchronize_buffer();
    void clear_all_buffers();

    bool get(const data_type::id_type &id, data_type &date)
    {
        if(id >= urluserpw.size())
            return false;
        date = to_data_type(urluserpw[id]);
        return true;
    }

    bool remove(std::vector<data_type::id_type> &ids)
    {
        if(!ids.size())
            return true;
        if(!urluserpw.size())
            return false;
        std::sort(std::begin(ids), std::end(ids));
        if(ids.back() >= urluserpw.size())
            return false;

        // delete multiple elements, starting with highest index,
        // so the iterators stay valid.
        for(std::size_t i = ids.size() - 1;;) {
            urluserpw.erase(std::begin(urluserpw) + ids[i]);
            if(i == 0)
                break;
            i--;
        }
        return true;
    }

    bool remove(const data_type::id_type &id)
    {
        if(id >= urluserpw.size())
            return false;
        urluserpw.erase(std::begin(urluserpw) + id);
        return true;
    }

    void dump_db(
        std::list<std::tuple<data_type::id_type, data_type>> &content) const;

    bool is_dirty() const { return dirty; }

private:
    using tuple_type = std::tuple<std::string, std::string, std::string>;
    struct list_cmp
    {
        // returns true if a < b
        bool operator()(const tuple_type &a, const tuple_type &b)
        {
            // true if a.url < b.url
            if(std::lexicographical_compare(
                   std::begin(std::get<0>(a)), std::end(std::get<0>(a)),
                   std::begin(std::get<0>(b)), std::end(std::get<0>(b))))
                return true;

            // post condition: a.url >= b.url
            // true if a.url > b.url
            if(std::lexicographical_compare(
                   std::begin(std::get<0>(b)), std::end(std::get<0>(b)),
                   std::begin(std::get<0>(a)), std::end(std::get<0>(a))))
                return false;

            // post condition: a.url = b.url
            // true if a.user < b.user
            if(std::lexicographical_compare(
                   std::begin(std::get<1>(a)), std::end(std::get<1>(a)),
                   std::begin(std::get<1>(b)), std::end(std::get<1>(b))))
                return true;

            return false;
        }
    };

    // TODO: why not just save it as data_type?
    data_type to_data_type(const tuple_type &t) const
    {
        return data_type(std::get<0>(t), std::get<1>(t), std::get<2>(t));
    }

    tuple_type to_tuple_type(const data_type &date) const
    {
        return std::make_tuple(date.url_string, date.username, date.password);
    }

private:
    bool dirty;
    std::string &string_buffer;
    size_t line_count;

    // use dc3 suffix-array from libaan for readonly databases in case of
    // interactive lookup
    std::vector<tuple_type> urluserpw;
};
}

#endif
