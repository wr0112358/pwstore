#ifndef _PWSTORE_API_CXX_
#define _PWSTORE_API_CXX_

#include "libaan/crypto_file.hh"

#include "pwstore.hh"

namespace pw_store_api_cxx {
class encrypted_pwstore
{
public:
    // open/create database in file db_file.
    encrypted_pwstore(const std::string &db_file, const std::string &password)
        : crypto_file(new libaan::crypto::file::crypto_file(db_file)),
          password(password)
    {
        db = load_db();
    }

    ~encrypted_pwstore()
    {
        std::fill(std::begin(password), std::end(password), 0);
    }

    // better check this before using get() method
    operator bool() const { return db != 0; }

    // next call to sync will reencrypt the database with the new password
    void change_password(const std::string &pw) { password.assign(pw); }

    bool sync() { return sync_and_write_db(); }

    pw_store::database &get() { return *db; }
    const pw_store::database &get() const { return *db; }

private:
    bool sync_and_write_db()
    {
        if(!db || !crypto_file)
            return false;
        using namespace libaan::crypto::file;
        db->synchronize_buffer();
        const auto err = crypto_file->write(password);
        if (err != crypto_file::NO_ERROR) {
            std::cerr << "Writing database to disk failed. Error: "
                      << crypto_file::error_string(err) << "\n";
            return false;
        }

        return true;
    }

    std::unique_ptr<pw_store::database> load_db()
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

        return db;
    }

private:
    std::unique_ptr<libaan::crypto::file::crypto_file> crypto_file;
    std::unique_ptr<pw_store::database> db;
    std::string password;
};

class pwstore_api
{
public:
    pwstore_api(const std::string &db_file, const std::string &db_password)
        : state(false), db{db_file, db_password}
    {
        state = db;
    }

    bool add(const pw_store::data_type &date);
    bool lookup(std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>> &matches,
                std::function<bool(const std::string &)> &provide_value_to_user,
                const std::string &lookup_key,
                const std::vector<pw_store::data_type::id_type> &uids = {}); // TODO: uid lookup -> get(uid)
    bool get(const pw_store::data_type::id_type &uid, pw_store::data_type &date);
    bool remove(std::vector<pw_store::data_type::id_type> &uids);

    bool change_password(const std::string &new_password);
    bool gen_passwd(const std::string &username, const std::string &url_string,
                    std::function<bool(const std::string &)> &provide_value_to_user);
    // TODO: dump should just do an empty lookup and return all
    bool dump() const;

    bool sync();

    operator bool() const { return state; }

private:
    bool state;
    encrypted_pwstore db;
};

}

#endif

