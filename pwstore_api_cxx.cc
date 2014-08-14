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

#include "pwstore_api_cxx.hh"

bool pw_store_api_cxx::encrypted_pwstore::sync_and_write_db()
{
    if(!db || !crypto_file)
        return false;
    using namespace libaan::crypto::file;
    db->synchronize_buffer();
    const auto err = crypto_file->write(password);
    if(err != crypto_file::NO_ERROR) {
        std::cerr << "Writing database to disk failed. Error: "
                  << crypto_file::error_string(err) << "\n";
        return false;
    }

    return true;
}

std::unique_ptr<pw_store::database>
pw_store_api_cxx::encrypted_pwstore::load_db()
{
    if(!crypto_file)
        return nullptr;

    using namespace libaan::crypto::file;

    // Read encrypted database with provided password.
    auto err = crypto_file->read(password);
    if(err != crypto_file::NO_ERROR) {
        std::cerr << "Error deciphering database. Wrong key? ("
                  << crypto_file::error_string(err) << ")\n";
        return nullptr;
    }

    // The only reason for not using an automatic variable for db per function
    // is to avoid duplicate code..
    // Better to be replaced with a class.
    std::unique_ptr<pw_store::database> db(
        new pw_store::database(crypto_file->get_decrypted_buffer()));
    if(!db->parse()) {
        std::cerr << "Error: corrupt database file.\n";
        return nullptr;
    }

    locked = false;
    return db;
}

void pw_store_api_cxx::encrypted_pwstore::lock()
{
    std::fill(std::begin(password), std::end(password), 0);
    db.reset(nullptr);
    crypto_file->clear_buffers();
    locked = true;
}

bool pw_store_api_cxx::encrypted_pwstore::unlock(const std::string &passwd)
{
    password.assign(passwd);
    db = load_db();
    if(db == nullptr)
        return false;

    return true;
}

bool pw_store_api_cxx::pwstore_api::add(const pw_store::data_type &date)
{
    if(!state)
        return false;

    if(!db.get().insert(date)) {
        std::cerr << "Error: inserting in database failed.\n";
        return false;
    }

    return true;
}

bool pw_store_api_cxx::pwstore_api::lookup(
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>> &
        matches,
    const std::string &lookup_key,
    const std::vector<pw_store::data_type::id_type> &uids)
{
    if(!state)
        return false;

    if(lookup_key.length())
        db.get().lookup(lookup_key, matches);

    for(const auto &id : uids) {
        pw_store::data_type data;
        if(db.get().get(id, data))
            matches.push_back(std::make_tuple(id, data));
    }

    return true;
}

bool pw_store_api_cxx::pwstore_api::get(const pw_store::data_type::id_type &uid,
                                        pw_store::data_type &date)
{
    if(!state)
        return false;

    return db.get().get(uid, date);
}

bool pw_store_api_cxx::pwstore_api::remove(
    std::vector<pw_store::data_type::id_type> &uids)
{
    if(!state)
        return false;

    if(uids.size() == 1) {
        if(!db.get().remove(uids.front()))
            return false;
    } else if(uids.size() > 1) {
        if(!db.get().remove(uids))
            return false;
    } else
        return false;

    return true;
}

bool
pw_store_api_cxx::pwstore_api::change_password(const std::string &new_password)
{
    if(!state)
        return false;
    db.change_password(new_password);
    return true;
}

namespace
{
bool generate_pw(std::string &password, const std::string &ascii_set)
{
    std::string new_ascii_set;
    const std::string &set = (ascii_set.empty() ? new_ascii_set : ascii_set);
    if(ascii_set.empty()) {
        for(int i = 0; i < 255; i++)
            if(isprint(i))
                new_ascii_set.push_back(char(i));
    }

    if(!libaan::crypto::read_random_ascii_set(12, set, password)) {
        std::cerr << "Error creating a password from random data. Aborting.\n";
        return false;
    }
    return true;
}
}

bool pw_store_api_cxx::pwstore_api::gen_passwd(const std::string &username,
                                               const std::string &url_string,
                                               std::string &password,
                                               bool insert_generated,
                                               const std::string &ascii_set)
{
    if(!state)
        return false;

    // only generate a password
    if(!insert_generated)
        return generate_pw(password, ascii_set);

    if(!url_string.length() && !username.length()) {
        std::cerr << "Error: creating a password for an empty"
                     "key does not make sense.\n       How would"
                     "you retrieve it? Aborting.\n";
        return false;
    }

    pw_store::data_type date;
    date.username = username;
    date.url_string = url_string;
    if(!generate_pw(date.password, ascii_set))
        return false;

    if(!db.get().insert(date)) {
        std::cerr << "Error: inserting in database failed.\n";
        return false;
    }

    std::cout << "Created and stored key for: " << date << ".\n";
    password = date.password;

    return true;
}

bool pw_store_api_cxx::pwstore_api::dump(
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>> &
        content) const
{
    if(!state)
        return false;

    db.get().dump_db(content);

    return true;
}

bool pw_store_api_cxx::pwstore_api::sync()
{
    if(!state)
        return false;

    return db.sync();
}

void pw_store_api_cxx::pwstore_api::lock() { db.lock(); }

bool pw_store_api_cxx::pwstore_api::unlock(const std::string &password)
{
    return db.unlock(password);
}
