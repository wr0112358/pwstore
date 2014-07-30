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

#include "pwstore.hh"

#include <chrono>
#include <libgen.h> // dirname basename
#include <list>
#include <signal.h>
#include <thread>
#include <unistd.h>

//#include "libaan/crypto_camellia.hh"
#include "libaan/crypto_file.hh"
#include "libaan/crypto_util.hh"
#include "libaan/file_util.hh"
#include "libaan/terminal_util.hh"
#include "libaan/x11_util.hh"

namespace {

struct config_type {
    enum { ADD, DUMP, INIT, INTERACTIVE_ADD, INTERACTIVE_LOOKUP, LOOKUP, REMOVE, CHANGE_PASSWD, GEN_PASSWD } mode;
    bool interactive;
    std::string lookup_key;
    std::vector<pw_store::data_type::id_type> uids;
    std::string db_file;
};

#ifndef NO_GOOD
const std::string DEFAULT_CIPHER_DB = ".pwstore.crypt";
#else
const std::string DEFAULT_CIPHER_DB = ".pwstore.crypt";
#endif


bool SIGINT_CAUGHT = false;
void sigint_handler(int signum)
{
    std::cout << "signum = " << signum << "\n";
    if (signum == SIGINT) {
        std::cout << "signum = " << signum << "\n";
        SIGINT_CAUGHT = true;
    }
}


std::list<pw_store::data_type> test_data = {
    {"ebay.de", "ebay_user1", "password1" },
    {"ebay", "ebay_user2", "password2" },
    {"amazon.de", "amazon_user", "password3" },
    {"mail.google.de", "gm_user1", "pw4" },
    {"mail.google.de", "gm_user2", "pw5" },
    {"mail.google.de", "gm_user3", "pw6" },
};

bool provide_value_to_user(const std::string &passwd)
{
#ifndef NO_GOOD
    if (!libaan::util::x11::add_to_clipboard(
            passwd, std::chrono::milliseconds(10000))) {
        std::cerr << "Error accessing x11 clipboard.\n";
        return false;
    }
    return true;
#else
    return false;
#endif

}

bool read_data_from_stdin(pw_store::data_type &date)
{
    std::cout << "Url:\n";
    std::getline(std::cin, date.url_string);
    std::cout << "User:\n";
    std::getline(std::cin, date.username);

    const libaan::crypto::util::password_from_stdin password(0);
    if(!password) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }
    date.password = password.password;

    return true;
}

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

bool add(const std::string &db_file, const std::string &db_password)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    pw_store::data_type date;
    if(!read_data_from_stdin(date)) {
        std::cerr << "Error reading data from stdin.\n";
        return false;
    }
    if(!db.get().insert(date)) {
        std::cerr << "Error: inserting in database failed.\n";
        return false;
    }

    return db.sync();
}

bool lookup(const std::string &db_file, const std::string &db_password,
            const std::string &key, const pw_store::data_type::id_type uid)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    if(uid != decltype(uid)(-1)) {
        pw_store::data_type data;
        const auto ret = db.get().get(uid, data);
        if(ret) {
            if(!provide_value_to_user(data.password))
                return false;
        }
        return ret;
    }

    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
    matches;
    db.get().lookup(key, matches);
    for(const auto &match: matches)
        std::cout << std::get<0>(match) << ": " << std::get<1>(match) << "\n";
    std::cout << "\n";
    return true;
}

bool remove(const std::string &db_file, const std::string &db_password,
            
std::vector<pw_store::data_type::id_type> &uids)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    if(uids.size() == 1) {
        if(!db.get().remove(uids[0]))
            return false;
    } else if(uids.size() > 1) {
        if(!db.get().remove(uids))
            return false;
    } else
        return false;

    return db.sync();
}

bool change_passwd(const std::string &db_file, const std::string &db_password)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    const libaan::crypto::util::password_from_stdin db_password_new(2);
    if(!db_password_new) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }

    db.change_password(db_password_new);
    return db.sync();
}

bool gen_passwd(const std::string &db_file, const std::string &db_password)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    pw_store::data_type date;
    std::cout << "Url:\n";
    std::getline(std::cin, date.url_string);
    std::cout << "User:\n";
    std::getline(std::cin, date.username);

    if(!date.url_string.length() && !date.username.length()) {
        std::cerr << "Error: creating a password for an empty"
                     "key does not make sense.\n       How would"
                     "you retrieve it? Aborting.\n";
        return false;
    }

    std::string ascii_set;
    for(int i = 0; i < 255; i++)
        if(isprint(i))
            ascii_set.push_back(char(i));
    if(!libaan::crypto::read_random_ascii_set(12, ascii_set, date.password)) {
        std::cerr << "Error creating a password from random data. Aborting.\n";
        return false;
    }

    if(!db.get().insert(date)) {
        std::cerr << "Error: inserting in database failed.\n";
        return false;
    }

    std::cout << "Created and stored key for: " << date << ".\n";
    if(!provide_value_to_user(date.password))
        return false;
    std::cout << date << " -> copied to x11 clipboard.\n";

    return db.sync();
}

bool dump(const std::string &db_file, const std::string &db_password)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    std::cout << "Database dump:\n";
    db.get().dump_db();

    return true;
}

// Test pwstore with example data.
bool init(const std::string &db_file, const std::string &db_password)
{
    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    // set content
    for(const auto &date: test_data)
        if(!db.get().insert(date)) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }

    return db.sync();
}

    bool interactive_add(const std::string &db_file, const std::string &db_password)
{
// iterate and wait for input until eof.
// read in data just like in add()
// then ask for y/n to insert
    return false;
}

const std::string ui_help = "C-x k\n"
                            "C-x C-k        clear lookup buffer.\n\n"
                            "C-x l\n"
                            "C-x C-l        dump db\n\n"
                            "C-x n\n"
                            "C-x C-n <id>   retrieve datum <id>\n\n"
                            "C-x c\n"
                            "C-x C-c        exit\n\n"
                            "C-x h\n"
                            "C-x C-h        show more help\n"
                            "C-g            cancel any action\n\n";
const std::string ui_last_normal_prefix = "  (Normal)  key = \"";
const std::string ui_last_normal_suffix = "\"\n";
const std::string ui_last_cx_prefix = "  (C-x)  key = \"";
const std::string ui_last_cx_suffix = "\"\n";

bool interactive_lookup(const std::string &db_file, const std::string &db_password)
{
    struct raii {
        raii() { libaan::util::terminal::alternate_screen_on(); }
        ~raii()
        {
            libaan::util::terminal::alternate_screen_off();
            std::cout << print_after_terminal_reset << "\n";
        }
        std::string print_after_terminal_reset;
    } raii;

    encrypted_pwstore db(db_file, db_password);
    if(!db)
        return false;

    //get user input async
    // TODO: select would be better, but 100ms cycletime works
    //       good enough for now and does not seem to have much
    //       cpu impact.
    const std::chrono::milliseconds dura(150);
    std::string input;
    std::string last_lookup;

#ifdef NO_GOOD
    #define CTRL(x) (x)
#endif

    enum state_type {
        CTRL_X,        // "command_mode": commands activated with Ctrl-x key
                       // combinations
        ACCUMULATE,    // "accumalation mode": all input is gathered in a
                       // string. for now only used for x11 clipboard population.
        NORMAL         // normal input mode
    } state;
    state = NORMAL;
    bool show_help = false;
    bool dump_db = false;
    std::string accumulate;
    bool only_once = true;

    while(true) {
        if(state == CTRL_X && SIGINT_CAUGHT) {
            raii.print_after_terminal_reset.assign("Terminating by request.\n");
            SIGINT_CAUGHT = false;
            return true;
        }

        bool change = false;
        if(only_once) {
            change = true;
            only_once = false;
        }
        {
            // TODO: move rawmode object out of the loop.
            //       -> ignoring signals necessary: see test1 in lib testsuite
            libaan::util::rawmode tty_raw;
            while (tty_raw.kbhit()) {
                bool ignore = true;
                const auto in = tty_raw.getch();

                if(state == CTRL_X) {
                    if(in == CTRL('c') || in == 'c') {
                        raii.print_after_terminal_reset.assign("Terminating by request.\n");
                        return true;
                    } else if(in == CTRL('k') || in == 'k') {
                        change = true;
                        input.clear();
                    } else if(in == CTRL('g')) {
                        change = true;
                        show_help = false;
                        dump_db = false;
                    } else if(in == CTRL('h') || in == 'h') {
                        show_help = true;
                        change = true;
                    } else if(in == CTRL('l') || in == 'l') {
                        dump_db = true;
                        change = true;
                    } else if(in == CTRL('n') || in == 'n') {
                        state = ACCUMULATE;
                        input.clear();
                    }

                    if(change)
                        state = NORMAL;
                } else if(state == ACCUMULATE) {
                    if (in == '\n') {
                        pw_store::data_type::id_type id =
                            std::strtol(accumulate.c_str(), nullptr, 10);
                        pw_store::data_type date;
                        if(db.get().get(id, date)) {
                            std::cout << id << ": " << date << "\n";
                            if(provide_value_to_user(date.password))
                                std::cout << id << ": " << date
                                          << " -> copied to x11 clipboard.\n";
                            // should be false at the moment
                            // change = false;
                        } else
                            change = true;
                        accumulate.clear();
                        state = NORMAL;
                    } else if(isgraph(in)) {
                        accumulate.push_back(static_cast<char>(in));
                        change = true;
                    } else if(in == CTRL('g')) {
                        change = true;
                        state = NORMAL;
                        accumulate.clear();
                    } 
                } else if(state == NORMAL) {
                    if(in == CTRL('x')) {
                        state = CTRL_X;
                        change = true;
                    } else if(isgraph(in)) //isalnum(in))
                        ignore = false;
                    else if(in == CTRL('g')) {
                        change = true;
                        show_help = false;
                        dump_db = false;
                    } 

                    if(!ignore) {
                        input.push_back(static_cast<char>(in));
                        change = true;
                    }
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

        // handle input
        if(input.length()) {
            last_lookup.clear();
            std::list<std::tuple<pw_store::data_type::id_type,
                                 pw_store::data_type>> matches;
            db.get().lookup(input, matches);
            for(const auto &match: matches)
                last_lookup.append(std::to_string(std::get<0>(match))
                                   + std::get<1>(match).to_string() + "\n");
        }

        struct gui
        {
            gui(bool help, const state_type &state, const std::string &key,
                const std::string &last_lookup, bool dump_db,
                const pw_store::database &db)
                : help(help),
                  state(state),
                  key(key),
                  last_lookup(last_lookup),
                  dump_db(dump_db),
                  db(db)
            {}
            ~gui()
            {
                const std::string SEP = "_________________________________________\n\n";
                libaan::util::terminal::alternate_screen_on();
                std::cout << ui_help << SEP;
                if(help)
                    std::cout << ""
                              << "C-x means: Press x-key while holding down Ctrl.\n" << SEP;
                std::cout << last_lookup << "\n";
                if(state == CTRL_X)
                    std::cout << ui_last_cx_prefix << key << ui_last_cx_suffix;
                else if(state == NORMAL)
                    std::cout << ui_last_normal_prefix << key
                              << ui_last_normal_suffix;
                std::cout << SEP;
                if(dump_db)
                    db.dump_db();
            }
            bool help;
            const state_type &state;
            const std::string &key;
            const std::string &last_lookup;
            bool dump_db;
            const pw_store::database &db;
        } gui(show_help, state, input, last_lookup, dump_db, db.get());
    }

    return db.sync();
}

bool run(config_type config)
{
    const libaan::crypto::util::password_from_stdin db_password(2);
    if(!db_password) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }
    bool ret = true;
    switch(config.mode) {
    case config_type::ADD:
        ret = add(config.db_file, db_password);
        break;
    case config_type::DUMP:
        ret = dump(config.db_file, db_password);
        break;
    case config_type::INIT:
        ret = init(config.db_file, db_password);
        break;
    case config_type::INTERACTIVE_ADD:
        ret = interactive_add(config.db_file, db_password);
        break;
    case config_type::INTERACTIVE_LOOKUP:
        ret = interactive_lookup(config.db_file, db_password);
        break;
    case config_type::LOOKUP:
        ret = lookup(config.db_file, db_password, config.lookup_key,
                     config.uids.size() ? config.uids.front()
                                        : pw_store::data_type::id_type(-1));
        break;
    case config_type::REMOVE:
        ret = remove(config.db_file, db_password, config.uids);
        break;
    case config_type::CHANGE_PASSWD:
        ret = change_passwd(config.db_file, db_password);
        break;
    case config_type::GEN_PASSWD:
        ret = gen_passwd(config.db_file, db_password);
        break;
    }

    return ret;
}

void usage(int argc, char *argv[])
{
    std::cout << "Usage: " << argv[0] << " [flags] command [optional-key]\n"
              << "  possible flags are:\n"
              << "    -f <filename>\tuse this file as database\n"
              << "    -i\tinteractive can be used with add or lookup\n"
              << "    -n <uid>  can only be used with non-interactive lookup/remove\n"
              << "              multiple uids can be specified for remove\n"
              << "  possible commands are:\n"
              << "    add\n"
              << "    dump\n"
              << "    lookup\n"
              << "    remove\n"
              << "    change_passwd    change password and reencrypt db-file\n"
              << "    gen_passwd       generate a password and store it in db-file\n"
              << "  optional-key:\n"
              << "    used only for lookup command in non-interactive mode.\n";
}


#ifndef NO_GOOD
    struct pathsep
    {
        bool operator()(char ch) const { return ch == '/'; }
    };
#else
    struct patsep
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

bool backup_db(const std::string &db_file)
{
    std::string buff;
    if(!libaan::util::file::read_file(db_file.c_str(), buff))
        return false;

    const auto now = std::chrono::high_resolution_clock::now();
    std::size_t count_backup_files = 0;

    const std::string BACKUP_FILE_PREFIX = db_file + "_backup_";
    const std::string base(basename(BACKUP_FILE_PREFIX));
    const auto dir = dirname(db_file);

    auto const lambda = [&base, &count_backup_files](const std::string &path, const struct dirent *p) {
        const std::string entry(p->d_name);
        if(!std::equal(base.begin(), base.end(), entry.begin()))
            return;
        count_backup_files++;
    };

    libaan::util::file::dir::readdir(dir, lambda);

    const auto backup_file = BACKUP_FILE_PREFIX + libaan::util::to_string(now);
    if(!libaan::util::file::write_file(backup_file.c_str(), buff))
        return false;

    std::cerr << "Creating backup(\"" << backup_file << "\") of file(\"" << db_file << "\")\n";
    std::cerr << "backup_files: " << count_backup_files << "\n";
    return true;
}

bool parse_and_check_args(int argc, char *argv[], config_type &config)
{
    config.interactive = false;
    // parse arguments
    for (int arg_index = 1; arg_index < argc; arg_index++) {
        if (argv[arg_index][0] == '-') {
            if (argv[arg_index][1] == 'i')
                config.interactive = true;
            else if (argv[arg_index][1] == 'n') {
                if (arg_index + 1 >= argc)
                    return false;
                const auto uid_arg = argv[++arg_index];
                config.uids.push_back(std::strtoul(uid_arg, nullptr, 10));
            } else if (argv[arg_index][1] == 'f') {
                if (arg_index + 1 >= argc)
                    return false;
                config.db_file = std::string(argv[++arg_index]);
            }
        } else {    // commands
            if (!std::strcmp(argv[arg_index], "add"))
                config.mode = config_type::ADD;
            else if (!std::strcmp(argv[arg_index], "dump"))
                config.mode = config_type::DUMP;
            else if (!std::strcmp(argv[arg_index], "init"))
                config.mode = config_type::INIT;
            else if (!std::strcmp(argv[arg_index], "lookup"))
                config.mode = config_type::LOOKUP;
            else if (!std::strcmp(argv[arg_index], "remove"))
                config.mode = config_type::REMOVE;
            else if (!std::strcmp(argv[arg_index], "change_passwd"))
                config.mode = config_type::CHANGE_PASSWD;
            else if (!std::strcmp(argv[arg_index], "gen_passwd"))
                config.mode = config_type::GEN_PASSWD;
            else
                config.lookup_key.assign(argv[arg_index]);
        }
    }

    // check validity of arguments
    if (config.interactive) {
        if (config.lookup_key.length()) {
            std::cerr << "Error: <optional-key> unneeded in interactive mode\n";
            return false;
        }
        if (config.mode == config_type::ADD)
            config.mode = config_type::INTERACTIVE_ADD;
        else if (config.mode == config_type::LOOKUP)
            config.mode = config_type::INTERACTIVE_LOOKUP;
        else {
            std::cerr << "Error: interactive mode only availabler for add or "
                         "lookup commands.\n";
            return false;
        }
    } else {
        if (config.mode != config_type::LOOKUP && config.lookup_key.length()) {
            std::cerr << "Error: key only needed for lookup command.\n";
            return false;
        }
    }

    if (config.uids.size()
        && config.mode != config_type::LOOKUP
        && config.mode != config_type::REMOVE) {
        std::cerr << "Error: uids only used for lookup and remove commands.\n";
        return false;
    }

    // at least one must be set for lookup
    if (config.mode == config_type::LOOKUP && !config.uids.size() &&
        !config.lookup_key.length()) {
        std::cerr << "Error: non-interactive lookup command needs either "
                     "specified uids or a an <optional-key>.\n";
        return false;
    }

    if (config.mode == config_type::REMOVE && !config.uids.size()) {
        std::cerr << "Error: specify at least one uid for remove command.\n";
        return false;
    }

    if (config.mode == config_type::CHANGE_PASSWD &&
        (config.uids.size() || config.lookup_key.length())) {
        std::cerr << "Error: change_passwd command has no need for uids/<optional-key>.\n";
        return false;
    }

    if (!config.db_file.length())
        config.db_file = DEFAULT_CIPHER_DB;

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
    //sigignore(SIGINT);
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
        break;
    case config_type::ADD:
    case config_type::INIT:
    case config_type::INTERACTIVE_ADD:
    case config_type::REMOVE:
    case config_type::CHANGE_PASSWD:
    case config_type::GEN_PASSWD:
    default:
        if(!backup_db(config.db_file))
            std::cerr << "Could not create database backup. Better be careful.\n";
    }

    if(!run(config))
        exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
