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



#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <list>
#include <signal.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "libaan/file_util.hh"
#include "libaan/terminal_util.hh"
#ifndef NO_GOOD
#include "libaan/x11_util.hh"
#endif

#include "pwstore.hh"
#include "pwstore_api_cxx.hh"

#include "libaan/crypto_util.hh"

#ifdef NO_GOOD
#define CTRL(x)(x&037)
#endif

namespace
{

#ifndef NO_GOOD
const std::string DEFAULT_CIPHER_DB = ".pwstore.crypt";
#else
const std::string DEFAULT_CIPHER_DB = ".pwstore.crypt";
#endif

bool SIGINT_CAUGHT = false;
bool exit_on_sigint = false;
void sigint_handler(int signum)
{
    if(signum == SIGINT) {
        if(exit_on_sigint)
            exit(EXIT_FAILURE);
        SIGINT_CAUGHT = true;
    }
}

bool ssh_running()
{
    return getenv("SSH_CLIENT") && getenv("SSH_CONNECTION");
}

struct config_type
{
    enum {
        ADD,
        DUMP,
        INIT,
        INTERACTIVE_LOOKUP,
        LOOKUP,
        MERGE,
        REMOVE,
        CHANGE_PASSWD,
        GEN_PASSWD,
        GET
    } mode;
    bool interactive;
    bool force;
    std::string lookup_key;
    std::vector<pw_store::data_type::id_type> uids;
    std::string db_file;
    std::string merge_input_files[2];
    std::function<bool(const std::string &)> provide_value_to_user;
};

std::list<pw_store::data_type> test_data = {
    {"ebay.de", "ebay_user1", "password1"},
    {"ebay", "ebay_user2", "password2"},
    {"amazon.de", "amazon_user", "password3"},
    {"mail.google.de", "gm_user1", "pw4"},
    {"mail.google.de", "gm_user2", "pw5"},
    {"mail.google.de", "gm_user3", "pw6"},
};

bool read_until(const std::list<int> &end_char, std::string &accumulate)
{
    libaan::util::rawmode tty_raw;
    while(true) {
        const auto in = tty_raw.getch();
        if((in == CTRL('x')) || (in == CTRL('g'))) {
            accumulate.clear();
            std::cerr << "\n..exiting.\n";
            return false;
        } else if(std::find(std::begin(end_char), std::end(end_char), in) !=
                  std::end(end_char))
            return true;
        else if(isalnum(in))
            accumulate.push_back(static_cast<char>(in));

        std::cout << char(in) << std::flush;
    }
}

// small cli-layer over pw_store_api_cxx

bool add(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    // read from stdin
    if(config.merge_input_files[0].empty()) {
        pw_store::data_type date;

        std::cout << "Url:\n";
        if(!read_until({'\n', '\r'}, date.url_string))
            return false;
        std::cout << "User:\n";
        if(!read_until({'\n', '\r'}, date.username))
            return false;

        const libaan::crypto::util::password_from_stdin password(0);
        if(!password) {
            std::cerr << "Password Error. Too short?\n";
            return false;
        }
        date.password = password.password;

        if(!db.add(date)) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }

        std::cout << "added: " << date << "\n";
    } else { // read from input file
        std::ifstream fp(config.merge_input_files[0]);
        if(!fp) {
            std::cerr << "Invalid input file \"" << config.merge_input_files[0]
                      << "\"\n";
            return false;
        }

        std::list<pw_store::data_type> insert;
        while(true) {
            pw_store::data_type date;
            if(!std::getline(fp, date.url_string))
                return false;
            if(!std::getline(fp, date.username))
                return false;
            if(!std::getline(fp, date.password))
                return false;
            if(date.url_string.empty() && date.username.empty() &&
               date.password.empty())
                return false;
            insert.push_back(date);
            std::string line;
            if(!std::getline(fp, line))
                break;
            if(line != "###")
                return false;
            if(!std::getline(fp, line))
                break;
            if(line != "###")
                return false;
        }

        for(const auto &date: insert) {
            if(!db.add(date)) {
                std::cerr << "Error: inserting in database failed.\n";
                return false;
            }
            std::cerr << "Debug: inserted " << date.to_string() << ".\n";
        }
    }

    return db.sync();
}

bool lookup(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
    matches;
    db.lookup(matches, config.lookup_key, config.uids);
    for(const auto &match : matches)
        std::cout << std::get<0>(match) << ": " << std::get<1>(match) << "\n";
    std::cout << "\n";
    return true;
}

std::unique_ptr<pw_store_api_cxx::pwstore_api> open_db(const std::string &file)
{
    const libaan::crypto::util::password_from_stdin db_password(2);
    if(!db_password) {
        std::cerr << "Password Error. Too short?\n";
        return nullptr;
    }
    std::unique_ptr<pw_store_api_cxx::pwstore_api> db(
        new pw_store_api_cxx::pwstore_api(file, db_password));
    if(!*db)
        return nullptr;

    if(!db->empty()) {
        const auto mod_time = db->time_of_last_write();
        std::cout << "Authenticity verified. Date of last modification: "
                  << mod_time << ".\nVerify integrity by comparing dates.(Y/n)\n";
        {
            libaan::util::rawmode tty_raw;
            const auto in = tty_raw.getch();
            if(in != 'Y')
                return nullptr;
        }
    }

    return db;
}

// quicker debugging
std::unique_ptr<pw_store_api_cxx::pwstore_api>
open_db(const std::string &file, const std::string &password)
{
    std::unique_ptr<pw_store_api_cxx::pwstore_api> db(
        new pw_store_api_cxx::pwstore_api(file, password));
    if(!*db)
        return nullptr;

    return db;
}

bool merge(config_type &config)
{
    struct stat s;
    if(stat(config.merge_input_files[0].c_str(), &s)) {
        std::cerr << "Invalid input database specified \""
                  << config.merge_input_files[0] << "\"\n";
        return false;
    }
    if(stat(config.merge_input_files[1].c_str(), &s)) {
        std::cerr << "Invalid input database specified \""
                  << config.merge_input_files[1] << "\"\n";
        return false;
    }

    if(!stat(config.db_file.c_str(), &s)) {
        std::cerr << "Output database must not exist. If you want to merge two dbs into an already existing one,\n"
                  << "use two merge steps.\n";
        return false;
    }

    // compare config.merge_input_files
    const auto in1 = open_db(config.merge_input_files[0], "hallo");
    if(!in1) {
        std::cerr << "Could not open database file \""
                  << config.merge_input_files[0] << "\".\n";
        return false;
    }
    const auto in2 = open_db(config.merge_input_files[1], "hallo");
    if(!in2) {
        std::cerr << "Could not open database file \""
                  << config.merge_input_files[1] << "\".\n";
        return false;
    }

    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        input1;
    in1->dump(input1);
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        input2;
    in2->dump(input2);
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        intersection;
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        complement_of_in1;
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        complement_of_in2;

    struct {
        bool operator()(const std::tuple<pw_store::data_type::id_type,
                                         pw_store::data_type> &a,
                        const std::tuple<pw_store::data_type::id_type,
                                         pw_store::data_type> &b) const
        {
            pw_store::data_type_cmp_enhanced cmp_inner;
            return cmp_inner(std::get<1>(a), std::get<1>(b));
        }
    } cmp;
/*
TODO:
This algorithm has a problem with identical entries. If input1 has 2 identical entries D
and input2 contains another copy of D, intersection will contain one D and complement A,B
the other D. The result of the merge is consistent, but the shown output is incorrect.
Solutions could be, find duplicates in pw_store::database::insert before doing the insert.
Or provide db_check function which checks for things like:
  is_sorted -> if no warn and ask yes/no for fix
  all_unique -> adjacent_find for duplicates, only after sorting, ask yes/no if duplicates
                should be deleted. out.erase(unique(out.begin, out.end), out.end)
  integrity_check -> check if date stored in crypto file is sensible.. e.g. 2013->now
                     should be a good timeframe
*/
    std::set_intersection(std::begin(input1), std::end(input1),
                          std::begin(input2), std::end(input2),
                          std::inserter(intersection, std::begin(intersection)),
                          cmp);

    std::cout << "Calculating Intersection.\n";
    std::cout << "A:\n";
    for(const auto &k : input1)
        std::cout << "\t" << std::get<0>(k) << ": "
                  << std::get<1>(k).to_string(true) << "\n";
    std::cout << "\n";

    std::cout << "B:\n";
    for(const auto &k : input2)
        std::cout << "\t" << std::get<0>(k) << ": "
                  << std::get<1>(k).to_string(true) << "\n";
    std::cout << "\n";

    std::cout << "Intersection of A and B:\n";
    for(const auto &k : intersection)
        std::cout << "\t" << std::get<0>(k) << ": "
                  << std::get<1>(k).to_string(true) << "\n";
    std::cout << "\n";

    std::set_difference(
        std::begin(input1), std::end(input1), std::begin(input2),
        std::end(input2),
        std::inserter(complement_of_in1, std::begin(complement_of_in1)), cmp);
    std::set_difference(
        std::begin(input2), std::end(input2), std::begin(input1),
        std::end(input1),
        std::inserter(complement_of_in2, std::begin(complement_of_in2)), cmp);

    std::cout << "Complement of A in B:\n";
    for(const auto &k : complement_of_in1)
        std::cout << "\t" << std::get<0>(k) << ": "
                  << std::get<1>(k).to_string(true) << "\n";
    std::cout << "\n";

    std::cout << "Complement of B in A:\n";
    for(const auto &k : complement_of_in2)
        std::cout << "\t" << std::get<0>(k) << ": "
                  << std::get<1>(k).to_string(true) << "\n";
    std::cout << "\n";

    bool use_ab = false;
    std::cout << "Use complement of A in B?(Y/n)\n";
    {
        libaan::util::rawmode tty_raw;
        const auto in = tty_raw.getch();
        if(in == 'Y')
            use_ab = true;
    }

    bool use_ba = false;
    std::cout << "Use complement of B in A?(Y/n)\n";
    {
        libaan::util::rawmode tty_raw;
        const auto in = tty_raw.getch();
        if(in == 'Y')
            use_ba = true;
    }

    auto out = open_db(config.db_file);
    if(!out) {
        std::cerr << "Could not create database file \"" << config.db_file
                  << "\".\n";
        return false;
    }

    // insert intersecting elements
    for(const auto &date: intersection) {
        if(!out->add(std::get<1>(date))) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }
        std::cerr << "Debug: inserted " << std::get<1>(date).to_string() << ".\n";
    }

    if(use_ab) {
        for(const auto &date: complement_of_in1) {
            if(!out->add(std::get<1>(date))) {
                std::cerr << "Error: inserting in database failed.\n";
                return false;
            }
            std::cerr << "Debug: inserted " << std::get<1>(date).to_string() << ".\n";
        }
    }

    if(use_ba) {
        for(const auto &date: complement_of_in2) {
            if(!out->add(std::get<1>(date))) {
                std::cerr << "Error: inserting in database failed.\n";
                return false;
            }
            std::cerr << "Debug: inserted " << std::get<1>(date).to_string() << ".\n";
        }
    }

    return out->sync();
}

bool get(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    pw_store::data_type date;
    if(!db.get(config.uids.front(), date)) {
        std::cerr << "Could not retrieve datum with id: " << config.uids.front()
                  << ".\n";
        return false;
    }
    exit_on_sigint = true;
    if(!config.provide_value_to_user(date.password)) {
        std::cerr << "Could not retrieve datum with id: " << config.uids.front()
                  << ".\n";
        exit_on_sigint = false;
        return false;
    }
    exit_on_sigint = false;
    std::cout << "Retrieved value for <id> " << config.uids.front() << ".\n";
    return true;
}

// needs non-const config_type since remove sorts the uid vector
bool remove(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    if(!config.force) {
        libaan::util::rawmode tty_raw;

        // if sth is available from stdin, discard it.
        while(tty_raw.kbhit())
            tty_raw.getch();

        std::cout << "Remove the following entries from database(Y/n)?\n";

        // lookup all entries specified for removal
        lookup(db, config);

        // read y/n from user
        const auto in = tty_raw.getch();
        // abort if anything except Y was entered
        if(in != 'Y') {
            std::cout << "Aborted. Nothing removed.\n";
            return false;
        }
    }

    if(!db.remove(config.uids))
        return false;
    std::cout << "Removed the specified entries.\n";
    return db.sync();
}

bool change_passwd(pw_store_api_cxx::pwstore_api &db)
{
    const libaan::crypto::util::password_from_stdin db_password_new(2);
    if(!db_password_new) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }

    db.change_password(db_password_new);
    return db.sync();
}

bool gen_passwd(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    std::string url_string, username;
    std::cout << "Url:\n";
    if(!read_until({'\n', '\r'}, url_string))
        return false;
    std::cout << "User:\n";
    if(!read_until({'\n', '\r'}, username))
        return false;

    std::string password;
    if(!db.gen_passwd(username, url_string, password))
        return false;

    std::cout << "Generated and stored password. Retrieving password..\n";
    exit_on_sigint = true;
    config.provide_value_to_user(password);
    exit_on_sigint = false;
    return db.sync();
}

bool dump(const pw_store_api_cxx::pwstore_api &db)
{
    std::cout << "Database dump:\n";
    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
    content;
    db.dump(content);
    for(const auto &k : content)
        std::cout << "\t" << std::get<0>(k) << ": " << std::get<1>(k) << "\n";

    return true;
}

// Test pwstore with example data.
bool init(pw_store_api_cxx::pwstore_api &db)
{
    // set content
    for(const auto &date : test_data)
        if(!db.add(date)) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }

    return db.sync();
}

const std::string ui_help = "<Enter>        start command mode.\n"
                            "k              kill input.\n"
                            "l              dump db\n"
                            "<id><Enter>    retrieve password for key <id>. "
                            "where id can be any digits\n"
                            "q              exit\n"
                            "h              show more help\n"
                            "Ctrl+g       cancel any action\n";
const std::string ui_last_normal_prefix = "  (Normal)      key = \"";
const std::string ui_last_normal_suffix = "\"\n";
const std::string ui_last_command_prefix = "  (Command)      key = \"";
const std::string ui_last_command_suffix = "\"\n";
const std::string ui_last_accumulate_prefix = "  (Command)       id = \"";
const std::string ui_last_accumulate_suffix = "\"\n";

bool interactive_lookup(pw_store_api_cxx::pwstore_api &db, config_type &config)
{
    struct raii
    {
        raii() { libaan::util::terminal::alternate_screen_on(); }
        ~raii()
        {
            libaan::util::terminal::alternate_screen_off();
            std::cout << print_after_terminal_reset << "\n";
        }
        std::string print_after_terminal_reset;
    } raii;

    const std::chrono::milliseconds dura(150);
    const long DEFAULT_TIMEOUT_SECS = 120;
    auto time_of_last_change = std::chrono::high_resolution_clock::now();

    std::string input;
    std::string last_lookup;

    enum state_type {
        COMMAND,       // "command_mode"
        ACCUMULATE,    // "accumalation mode"
        NORMAL         // normal input mode
    } state = NORMAL;
    bool show_help = false;
    bool dump_db = false;
    std::string accumulate;
    bool only_once = true;
    bool terminate = false;

    while(true) {

        bool change = false;
        if(only_once) {
            change = true;
            only_once = false;
        }

        if(terminate || (state == COMMAND && SIGINT_CAUGHT)) {
            raii.print_after_terminal_reset.assign("Terminating by request.\n");
            SIGINT_CAUGHT = false;
            return true;
        }

        // lock db after a fixed time
        if(std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::high_resolution_clock::now() - time_of_last_change)
               .count() > DEFAULT_TIMEOUT_SECS) {
            db.lock();
            libaan::util::terminal::alternate_screen_on();
            std::cout << "Database locked after " << DEFAULT_TIMEOUT_SECS
                      << " seconds of inactivity.\n";
            const libaan::crypto::util::password_from_stdin db_password(2);
            if(!db_password) {
                std::cerr << "Password Error. Too short?\n";
                return false;
            }
            if(!db.unlock(db_password))
                return false;
            time_of_last_change = std::chrono::high_resolution_clock::now();
            change = true;
        }

        {
            libaan::util::rawmode tty_raw;
            while(tty_raw.kbhit() && !terminate) {
                bool ignore = true;
                const auto in = tty_raw.getch();
                if(state == COMMAND) {
                    if(in == 'q') {
                        terminate = true;
                        break;
                    } else if(in == 'k') {
                        change = true;
                        input.clear();
                    } else if(in == CTRL('g')) {
                        change = true;
                        show_help = false;
                        dump_db = false;
                    } else if(in == 'h') {
                        show_help = true;
                        change = true;
                    } else if(in == 'l') {
                        dump_db = true;
                        change = true;
                    } else if(isdigit(in)) {
                        state = ACCUMULATE;
                        input.clear();
                        // don't forget this number
                        accumulate.push_back(static_cast<char>(in));
                        change = true;
                    } else if(in == CTRL('x'))
                        terminate = true;

                    if(change && state != ACCUMULATE)
                        state = NORMAL;
                } else if(state == ACCUMULATE) {
                    if(in == '\n' || in == '\r') {
                        pw_store::data_type::id_type id =
                            std::strtol(accumulate.c_str(), nullptr, 10);
                        pw_store::data_type date;
                        if(db.get(id, date)) {
                            exit_on_sigint = true;
                            if(config.provide_value_to_user(date.password))
                                std::cout << "Retrieved value for <id> " << id
                                          << ".\n";
                            exit_on_sigint = false;
                        } else
                            change = true;
                        accumulate.clear();
                        state = NORMAL;
                    } else if(isdigit(in)) {
                        accumulate.push_back(static_cast<char>(in));
                        change = true;
                    } else if(in == CTRL('g')) {
                        change = true;
                        state = NORMAL;
                        accumulate.clear();
                    } else if(in == CTRL('x'))
                        terminate = true;
                } else if(state == NORMAL) {
                    if(in == 127) {
                        if(input.length()) {
                            input.resize(input.length() - 1);
                            change = true;
                        }
                    } else if(in == '\n' || in == '\r') {
                        state = COMMAND;
                        change = true;
                    } else if(isgraph(in))
                        ignore = false;
                    else if(in == CTRL('g')) {
                        change = true;
                        show_help = false;
                        dump_db = false;
                    } else if(in == CTRL('x'))
                        terminate = true;
                    if(!ignore) {
                        input.push_back(static_cast<char>(in));
                        change = true;
                    }
                }
            }

            if(!change) {
#ifdef NO_GOOD
                usleep(dura.count() * 1000);
#else
                std::this_thread::sleep_for(dura);
#endif
                continue;
            }
        }

        time_of_last_change = std::chrono::high_resolution_clock::now();

        // handle input
        if(input.length()) {
            last_lookup.clear();
            std::list<std::tuple<pw_store::data_type::id_type,
                                 pw_store::data_type>> matches;
            db.lookup(matches, input, config.uids);
            for(const auto &match : matches)
                last_lookup.append(std::to_string(std::get<0>(match)) +
                                   std::get<1>(match).to_string() + "\n");
        }

        struct gui
        {
            gui(bool help, const state_type &state, const std::string &key,
                const std::string &last_lookup, const std::string &accumulate,
                bool dump_db, const pw_store_api_cxx::pwstore_api &db)
                : help(help),
                  state(state),
                  key(key),
                  last_lookup(last_lookup),
                  accumulate(accumulate),
                  dump_db(dump_db),
                  db(db)
            {
            }

            ~gui()
            {
                const std::string SEP =
                    "_________________________________________\n\n";
                libaan::util::terminal::alternate_screen_on();
                std::cout << ui_help << SEP;
                if(help)
                    std::cout
                        << ""
                        << "C-x means: Press x-key while holding down Ctrl.\n"
                        << SEP;
                if(state == COMMAND)
                    std::cout << ui_last_command_prefix << key
                              << ui_last_command_suffix;
                else if(state == NORMAL)
                    std::cout << ui_last_normal_prefix << key
                              << ui_last_normal_suffix;
                else if(state == ACCUMULATE)
                    std::cout << ui_last_accumulate_prefix << accumulate
                              << ui_last_accumulate_suffix;
                std::cout << SEP;
                std::cout << last_lookup << (last_lookup.length() ? SEP : "");
                if(dump_db)
                    dump(db);
            }
            bool help;
            const state_type &state;
            const std::string &key;
            const std::string &last_lookup;
            const std::string &accumulate;
            bool dump_db;
            const pw_store_api_cxx::pwstore_api &db;
        } gui(show_help, state, input, last_lookup, accumulate, dump_db, db);
    }

    return db.sync();
}

bool run(config_type config)
{
    if(config.mode == config_type::MERGE)
        return merge(config);

    const libaan::crypto::util::password_from_stdin db_password(2);
    if(!db_password) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }
    pw_store_api_cxx::pwstore_api db(config.db_file, db_password);
    if(!db)
        return false;

    if(!db.empty()) {
        const auto mod_time = db.time_of_last_write();
        std::cout << "Authenticity verified. Date of last modification: "
                  << mod_time << ".\nVerify integrity by comparing dates.(Y/n)\n";
        {
            libaan::util::rawmode tty_raw;
            const auto in = tty_raw.getch();
            if(in != 'Y')
                return false;
        }
    }

    bool ret = true;
    switch(config.mode) {
    case config_type::ADD:
        ret = add(db, config);
        break;
    case config_type::DUMP:
        ret = dump(db);
        break;
    case config_type::INIT:
        ret = init(db);
        break;
    case config_type::INTERACTIVE_LOOKUP:
        ret = interactive_lookup(db, config);
        break;
    case config_type::LOOKUP:
        ret = lookup(db, config);
        break;
    case config_type::REMOVE:
        ret = remove(db, config);
        break;
    case config_type::CHANGE_PASSWD:
        ret = change_passwd(db);
        break;
    case config_type::GEN_PASSWD:
        ret = gen_passwd(db, config);
        break;
    case config_type::GET:
        ret = get(db, config);
        break;
    case config_type::MERGE:
        ret = false;
        break;
    }

    return ret;
}

void usage(int, char *argv[])
{
    std::cout
        << "Usage: " << argv[0] << " [flags] command [optional-key]\n\n"
        << "  possible flags are:\n"
        << "    -f <db-file>  use this file as database\n"
        << "    -n <uid>      can only be used with non-interactive "
           "lookup/remove\n"
        << "                  multiple uids can be specified for remove\n"
        << "    -o            dump retrieved password to stdout\n"
        << "    -i            interactive\n\n"
        << "  possible commands are:\n"
        << "    add <optional_input_file>\n"
        << "      Interactively add one datum to database if no input file was specified.\n"
        << "      Input file must have format: <entry>###\\n###\\n<entry>..., where <entry> is:\n"
        << "      url\\nuser\\npassword\\n"
        << "    dump\n"
        << "      Dump database content.\n"
        << "    lookup <optional-key> [-i] [-o] [-n <uid>]\n"
        << "      Print all entries that match the specified uids or the specified key.\n"
        << "    get                   [-o] -n <uid>\n"
        << "      Retrieve password for entry with speciefied uid.\n"
        << "    merge dbfile1 dbfile2 result\n"
        << "      Compare dbfile1 and dbfile2 and try to merge them in a new dbfile result.\n"
        << "    remove                (-n <uid>)+ [--force]\n"
        << "      Remove specified entry.\n"
        << "    change_passwd         change password and reencrypt db-file\n"
        << "      Change encryption key of database.\n"
        << "    gen_passwd            generate a password and store it in "
           "db-file\n"
        << "      Same as add, but password is created from pseudo random pool.\n"
        << "  database name to be used is taken from:\n"
        << "    environment variable PWSTORE_DB_FILE\n"
        << "    -f <db-file> flag\n"
        << "    default filename: " << DEFAULT_CIPHER_DB << "\n"
        << "    -f flag overrides all, $PWSTORE_DB_FILE overrides default "
           "name.\n";
}

#ifndef NO_GOOD
struct pathsep
{
    bool operator()(char ch) const { return ch == '/'; }
};
#else
struct pathsep
{
    bool operator()(char ch) const { return ch == '\\' || ch == '/'; }
};
#endif
std::string dirname(std::string const &pathname)
{
    return std::string(
        pathname.begin(),
        std::find_if(pathname.rbegin(), pathname.rend(), pathsep()).base());
}

std::string basename(std::string const &pathname)
{
    return std::string(
        std::find_if(pathname.rbegin(), pathname.rend(), pathsep()).base(),
        pathname.end());
}

#ifdef NO_GOOD
bool backup_db(const std::string &)
{
    return false;
}
#else
bool backup_db(const std::string &db_file)
{
    { // if file does not exist, don't do anything
        struct stat s;
        if(stat(db_file.c_str(), &s))
            return true;
    }
    std::string buff;
    if(!libaan::util::file::read_file(db_file.c_str(), buff))
        return false;

    const auto now = std::chrono::high_resolution_clock::now();
    std::size_t count_backup_files = 0;

    const std::string BACKUP_FILE_PREFIX = db_file + "_backup_";
    const std::string base(basename(BACKUP_FILE_PREFIX));
    const auto dir = dirname(db_file);

    auto const lambda = [&base, &count_backup_files](const std::string &,
                                                     const struct dirent *p) {
        const std::string entry(p->d_name);
        if(!std::equal(base.begin(), base.end(), entry.begin()))
            return;
        count_backup_files++;
    };

    libaan::util::file::dir::readdir(dir, lambda);

    const auto backup_file = BACKUP_FILE_PREFIX + libaan::util::to_string(now);
    if(!libaan::util::file::write_file(backup_file.c_str(), buff))
        return false;

    std::cerr << "Creating backup(\"" << backup_file << "\") of file(\""
              << db_file << "\")\n"
              << "backup_files: " << count_backup_files << "\n";
    return true;
}
#endif

bool parse_and_check_args(int argc, char *argv[], config_type &config)
{
    config.interactive = false;
    config.force = false;
    enum output_type { TO_X11, TO_STDOUT } output;
    output = TO_X11;

    // parse arguments
    for(int arg_index = 1; arg_index < argc; arg_index++) {
        if((argv[arg_index][0] == '-') && (argv[arg_index][1] == '-')) {
            // arguments starting with --
            if(!std::strcmp(argv[arg_index], "--force"))
                config.force = true;
        } else if(argv[arg_index][0] == '-') {
            // flags starting with a single '-'
            if(argv[arg_index][1] == 'i')
                config.interactive = true;
            else if(argv[arg_index][1] == 'n') {
                if(arg_index + 1 >= argc)
                    return false;
                const auto uid_arg = argv[++arg_index];
                config.uids.push_back(std::strtoul(uid_arg, nullptr, 10));
            } else if(argv[arg_index][1] == 'f') {
                if(arg_index + 1 >= argc)
                    return false;
                config.db_file = std::string(argv[++arg_index]);
            } else if(argv[arg_index][1] == 'o')
                output = TO_STDOUT;
        } else {
            // commands
            if(!std::strcmp(argv[arg_index], "add"))
                config.mode = config_type::ADD;
            else if(!std::strcmp(argv[arg_index], "dump"))
                config.mode = config_type::DUMP;
            else if(!std::strcmp(argv[arg_index], "init"))
                config.mode = config_type::INIT;
            else if(!std::strcmp(argv[arg_index], "lookup"))
                config.mode = config_type::LOOKUP;
            else if(!std::strcmp(argv[arg_index], "remove"))
                config.mode = config_type::REMOVE;
            else if(!std::strcmp(argv[arg_index], "change_passwd"))
                config.mode = config_type::CHANGE_PASSWD;
            else if(!std::strcmp(argv[arg_index], "gen_passwd"))
                config.mode = config_type::GEN_PASSWD;
            else if(!std::strcmp(argv[arg_index], "get"))
                config.mode = config_type::GET;
            else if(!std::strcmp(argv[arg_index], "merge"))
                config.mode = config_type::MERGE;
            else {
                if(config.mode == config_type::LOOKUP) {
                    config.lookup_key.assign(argv[arg_index]);
                } else if(config.mode == config_type::MERGE) {
                    if(!config.merge_input_files[0].length())
                        config.merge_input_files[0].assign(argv[arg_index]);
                    else if(config.merge_input_files[0].length()
                            && !config.merge_input_files[1].length())
                        config.merge_input_files[1].assign(argv[arg_index]);
                    else if(config.merge_input_files[0].length()
                            && config.merge_input_files[1].length()
                            && !config.db_file.length())
                        config.db_file.assign(argv[arg_index]);
                } else if(config.mode == config_type::ADD) {
                    if(!config.merge_input_files[0].length())
                        config.merge_input_files[0].assign(argv[arg_index]);
                }
            }
        }
    }

    // check validity of arguments
    if(config.interactive) {
        if(config.lookup_key.length()) {
            std::cerr << "Error: <optional-key> unneeded in interactive mode\n";
            return false;
        }
        if(config.mode == config_type::LOOKUP)
            config.mode = config_type::INTERACTIVE_LOOKUP;
        else {
            std::cerr << "Error: interactive mode only available for lookup.\n";
            return false;
        }
    } else {
        if(config.mode != config_type::LOOKUP && config.lookup_key.length()) {
            std::cerr << "Error: key only needed for lookup command.\n";
            return false;
        }
    }

    if(config.uids.size() && config.mode != config_type::LOOKUP &&
       config.mode != config_type::REMOVE && config.mode != config_type::GET) {
        std::cerr << "Error: uids only used for lookup and remove commands.\n";
        return false;
    }

    // at least one must be set for lookup
    if(config.mode == config_type::LOOKUP && !config.uids.size() &&
       !config.lookup_key.length()) {
        std::cerr << "Error: non-interactive lookup command needs either "
                     "specified uids or a an <optional-key>.\n";
        return false;
    }

    if(config.mode == config_type::REMOVE && !config.uids.size()) {
        std::cerr << "Error: specify at least one uid for remove command.\n";
        return false;
    }

    if(config.mode == config_type::CHANGE_PASSWD &&
       (config.uids.size() || config.lookup_key.length())) {
        std::cerr << "Error: change_passwd command has no need for "
                     "uids/<optional-key>.\n";
        return false;
    }

    if(config.mode == config_type::GET &&
       (config.uids.size() != 1 || config.lookup_key.length() || config.force ||
        config.interactive)) {
        std::cerr << "Error: get command has no need for multiple "
                     "uids/<optional-key>.\n"
                     "Read the usage instructions.\n";
        return false;
    }
    std::cout << config.merge_input_files[0] << "\n"
              << config.merge_input_files[1] << "\n"
              << config.db_file << "\n";

    if(config.mode == config_type::MERGE)
        if(!config.merge_input_files[0].length()
           || !config.merge_input_files[1].length()
           || !config.db_file.length()) {
            std::cout << "Invalid parameters for merge command.\n";
            return false;
        }

    if(!config.db_file.length()) {
        const auto env_db = getenv("PWSTORE_DB_FILE");
        if(!env_db || !env_db[0])
            config.db_file = DEFAULT_CIPHER_DB;
        else
            config.db_file = std::string(env_db);
    }
    if(config.db_file.length()
       && (config.mode == config_type::DUMP
           || config.mode == config_type::INTERACTIVE_LOOKUP
           || config.mode == config_type::LOOKUP
           || config.mode == config_type::GET)){
        struct stat s;
        if(stat(config.db_file.c_str(), &s)) {
            std::cerr << "Invalid database specified.\n";
            return false;
        }
    }

    if(output == TO_X11 && ssh_running()) {
        std::cerr << "When running over ssh, pwstore only supports output to stdout.\n";
        return false;
    }

    if(output == TO_X11) {
#ifndef NO_GOOD
        config.provide_value_to_user = [](const std::string &passwd) {
            if(!libaan::util::x11::add_to_clipboard(
                   passwd, std::chrono::milliseconds(10000))) {
                std::cerr << "Error accessing x11 clipboard.\n";
                return false;
            }
            return true;
        };
#else
        return false;
#endif
    } else if(output == TO_STDOUT) {
        config.provide_value_to_user = [](const std::string &passwd) {
            std::cout << passwd << "\n";
            return true;
        };
    }

    return true;
}
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }

#ifndef NO_GOOD
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, nullptr);
// sigignore(SIGINT);
#endif

    config_type config;
    if(!parse_and_check_args(argc, argv, config)) {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    // create a backup file before applying any modifying commands.
    switch(config.mode) {
    case config_type::DUMP:
    case config_type::INTERACTIVE_LOOKUP:
    case config_type::LOOKUP:
    case config_type::GET:
    case config_type::MERGE:
        break;
    case config_type::ADD:
    case config_type::INIT:
    case config_type::REMOVE:
    case config_type::CHANGE_PASSWD:
    case config_type::GEN_PASSWD:
    default:
        if(!backup_db(config.db_file))
            std::cerr
                << "Could not create database backup. Better be careful.\n";
    }

    if(!run(config))    // TODO: remove backup?
        exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
