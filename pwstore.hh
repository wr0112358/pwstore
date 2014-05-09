#ifndef _PWSTORE_HH_
#define _PWSTORE_HH_

#include <algorithm>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <tuple>

namespace pw_store {

// stores key value tuples in a string
// - where keys are url-strings and usernames. values are usernames and passwords.
// - per entry 3 delim characters must exist
// format is:
//   entry = URL DELIM USERNAME DELIM PASSWORD DELIM
//   file = entry*
//   URL/USERNAME/PASSWORD can be any readable string or EMPTY
//   - a line consists at least of: DELIM DELIM DELIM NEWLINE

struct data_type
{
/*
Possibly provide hash as another key to ease lookup:
    libaan::crypto::hash h;
    h.sha1(data.url_string + data.username + data.password, data.hash);
Is non-interactive lookup even worthwile?
*/
    data_type()
        : data_type("", "", "") {}
    data_type(const std::string & url, const std::string & user,
              const std::string & pass)
        : url_string(url), username(user), password(pass) {}

    friend std::ostream &operator<<(std::ostream &os, const data_type &date);
    std::string to_string() const;

    std::string url_string;
    std::string username;
    std::string password;
};

inline std::string data_type::to_string() const
{
    return std::string("(\"") + url_string + std::string("\", \"") + username +
           std::string("\", \"") + "***"    // date.password
           + std::string("\")");
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
    database(std::string &buffer)
        : dirty(false), string_buffer(buffer) {}

    ~database() { clear_all_buffers(); }
    // Parse the provided buffer.
    bool parse();
    bool insert(const data_type &date);
    void lookup(const std::string &key, std::list<data_type> &matches);
    void synchronize_buffer();
    void clear_all_buffers();

//TODO:
// lookup should return id_type == index in vector for every found match
//modify(id_type id, const data_type &date);
//delete(id_type id);

    void dump_db() const;
private:

    using tuple_type = std::tuple<std::string, std::string, std::string>;
    struct list_cmp {
        // returns true if a < b 
        bool operator()(const tuple_type &a, const tuple_type &b)
        {
            // true if a.url < b.url
            if(std::lexicographical_compare(std::begin(std::get<0>(a)),
                                            std::end(std::get<0>(a)),
                                            std::begin(std::get<0>(b)),
                                            std::end(std::get<0>(b))))
                return true;

            // post condition: a.url >= b.url
            // true if a.url > b.url
            if(std::lexicographical_compare(std::begin(std::get<0>(b)),
                                            std::end(std::get<0>(b)),
                                            std::begin(std::get<0>(a)),
                                            std::end(std::get<0>(a))))
                return false;

            // post condition: a.url = b.url
            // true if a.user < b.user
            if(std::lexicographical_compare(std::begin(std::get<1>(a)),
                                            std::end(std::get<1>(a)),
                                            std::begin(std::get<1>(b)),
                                            std::end(std::get<1>(b))))
                return true;

            return false;
        }
    };

    data_type to_data_type(const tuple_type & t) const
    {
        return data_type(std::get<0>(t), std::get<1>(t), std::get<2>(t));
    }

private:
    bool dirty;
    std::string &string_buffer;
    size_t line_count;
    std::vector<tuple_type> urluserpw;
};

}

#endif
