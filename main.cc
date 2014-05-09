// run example with:
// rm test_file.crypt test_file6decrypted; ./crypto_util_test_camellia password test_file6 test_file.crypt

#include "pwstore.hh"

#include <chrono>
#include <list>
#include <signal.h>
#include <thread>
#include <unistd.h>

//#include "libaan/crypto_camellia.hh"
#include "libaan/crypto_file.hh"
#include "libaan/crypto_util.hh"
#include "libaan/file_util.hh"
#include "libaan/terminal_util.hh"

namespace {

struct config_type {
    enum { ADD, DUMP, INIT, INTERACTIVE_ADD, INTERACTIVE_LOOKUP, LOOKUP } mode;
    bool interactive;
    std::string lookup_key;
};

const auto CIPHER_DB = "/tmp/.pwstore.crypt";

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

bool read_data_from_stdin(pw_store::data_type &date)
{
    std::cout << "Url:\n";
    std::cin >> date.url_string;
    std::cout << "User:\n";
    std::cin >> date.username;

    const libaan::crypto::util::password_from_stdin password(2);
    if(!password) {
        std::cerr << "Password Error. Too short?\n";
        return false;
    }
    date.password = password.password;

    return true;
}

std::unique_ptr<pw_store::database>
load_db(libaan::crypto::file::crypto_file &file_io,
        const std::string &db_password)
{
    using namespace libaan::crypto::file;

    // Read encrypted database with provided password.
    crypto_file::error_type err = crypto_file::NO_ERROR;
    err = file_io.read(db_password);
    if(err != crypto_file::NO_ERROR) {
        std::cerr << "Error deciphering database. Wrong key? ("
                  << crypto_file::error_string(err) << ")\n";
        return nullptr;
    }

    // The only reason for not using an automatic variable for db per function
    // is to avoid duplicate code..
    // Better to be replaced with a class.
    std::unique_ptr<pw_store::database> db(
        new pw_store::database(file_io.get_decrypted_buffer()));
    if(!db->parse()) {
        std::cerr << "Error: corrupt database file.\n";
        return nullptr;
    }

    return db;
}

bool sync_and_write_db(const std::string &db_password,
                       libaan::crypto::file::crypto_file &file_io,
                       pw_store::database &db)
{
    using namespace libaan::crypto::file;
    db.synchronize_buffer();
    const auto err = file_io.write(db_password);
    if (err != crypto_file::NO_ERROR) {
        std::cerr << "Writing database to disk failed. Error: "
                  << crypto_file::error_string(err) << "\n";
        return false;
    }

    return true;
}

bool add(const std::string &db_password)
{
    using namespace libaan::crypto::file;
    crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    pw_store::data_type date;
    if(!read_data_from_stdin(date)) {
        std::cerr << "Error reading data from stdin.\n";
        return false;
    }
    if(!db->insert(date)) {
        std::cerr << "Error: inserting in database failed.\n";
        return false;
    }
    if(!sync_and_write_db(db_password, file_io, *db))
        return false;

    { // DEBUG: test crypto file by reading/decrypting the just written file
        // make a copy of plaintext buffer
        auto plain = file_io.get_decrypted_buffer();

        // encryption and write successfull.
        // Reading encrypted file again.

        crypto_file file_io2(CIPHER_DB);
        const auto err = file_io2.read(db_password);
        if(err != crypto_file::NO_ERROR) {
            std::cerr << "Error deciphering database, wrong key? ("
                      << crypto_file::error_string(err) << ")\n";
            return false;
        }
        if(file_io2.get_decrypted_buffer() != plain) {
            std::cerr << "Error: plain != Decrypt(Encrypt(plain))\n";
            return false;
        }
    } // Debug Success: plain == Decrypt(Encrypt(plain))

    // Overwrite memory.
    file_io.clear_buffers();

    return true;
}

bool lookup(const std::string &db_password, const std::string &key)
{
    libaan::crypto::file::crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    std::list<pw_store::data_type> matches;
    db->lookup(key, matches);
    for(const auto &match: matches)
        std::cout << match << "\n";
    std::cout << "\n";
    return true;
}

bool dump(const std::string &db_password)
{
    libaan::crypto::file::crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    std::cout << "Database dump:\n";
    db->dump_db();

    return true;
}

// Test pwstore with example data.
bool init(const std::string &db_password)
{
    using namespace libaan::crypto::file;
    crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    // set content
    for(const auto &date: test_data)
        if(!db->insert(date)) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }
    if(!sync_and_write_db(db_password, file_io, *db))
        return false;

    return true;
}

/*
// Add saved pws from firefox profiles.
bool firefox_add(const std::string &db_password)
{

// A: Extract it by reading the encrypted db
    // 1. read "Path=.." lines from:
    //    "~" + getpwuid(getuid)->pw_name + "/.mozilla/firefox/profiles.ini
    //
    // Relevant files:  “signons.sqlite” and "key3.db" for master-pw
    // Analog to the following. But might need some updating:
    // http://spiros-antonatos.blogspot.de/2011/07/cc-way-for-decrypting-firefox-passwords.html
    // http://spiros-antonatos.blogspot.de/2011/07/cc-way-for-decrypting-firefox-passwords_07.html

// B: Use cam_app ocr extractor to get ocr dump of firefox window..

    using namespace libaan::crypto::file;
    crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    // set content
    for(const auto &date: test_data)
        if(!db->insert(date)) {
            std::cerr << "Error: inserting in database failed.\n";
            return false;
        }
    if(!sync_and_write_db(db_password, file_io, *db))
        return false;

    return true;
}
*/

bool interactive_add(const std::string &db_password)
{
// iterate and wait for input until eof.
// read in data just like in add()
// then ask for y/n to insert
    return true;
}

const std::string ui_help = "C-x k\n"
                            "C-x C-k    clear lookup buffer.\n\n"
                            "C-x l\n"
                            "C-x C-l    dump db\n\n"
                            "C-x c\n"
                            "C-x C-c    exit\n\n"
                            "C-x h\n"
                            "C-x C-h    show more help\n"
                            "C-g        cancel any action\n\n";
const std::string ui_last_normal_prefix = "  (Normal)  key = \"";
const std::string ui_last_normal_suffix = "\"\n";
const std::string ui_last_cx_prefix = "  (C-x)  key = \"";
const std::string ui_last_cx_suffix = "\"\n";

bool interactive_lookup(const std::string &db_password)
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

    libaan::crypto::file::crypto_file file_io(CIPHER_DB);
    auto db = load_db(file_io, db_password);
    if(!db)
        return false;

    // TODO:
    // iterate and wait for input until eof.
    // perform partial lookup for entered characters.
    // when lookup result is a single datum notify user and copy key(password)
    // in x11 buffer

    //get user input async
    // TODO: select would be better, but 100ms cycletime works
    //       good enough for now and does not seem to have much
    //       cpu impact.
    const std::chrono::milliseconds dura(150);
    std::string input;
    std::string last_lookup;

    enum state_type { CTRL_X, NORMAL } state;
    state = NORMAL;
    bool show_help = false;
    bool dump_db = false;

    while(true) {
        if(state == CTRL_X && SIGINT_CAUGHT) {
            raii.print_after_terminal_reset.assign("Terminating by request.\n");
            SIGINT_CAUGHT = false;
            return true;
        }

        bool change = false;
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
                    }

                    if(change)
                        state = NORMAL;
                } else {
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
                /*
                    char c = static_cast<char>(tty_raw.getch());
                    std::cout << "\tyes: " << c << "\n";
                    if(c == 'q')
                        return;
                */
        }
        if(!change) {
            std::this_thread::sleep_for(dura);
            continue;
        }

        // handle input

        if(input.length()) {
            last_lookup.clear();
            std::list<pw_store::data_type> matches;
            db->lookup(input, matches);
            for(const auto &match: matches)
                last_lookup.append(match.to_string() + "\n");
            // std::cout << "\t" << match << "\n";
            // std::cout << "\n";
        }

        struct gui
        {
            gui(bool help, const state_type &state, const std::string &key,
                const std::string &last_lookup, bool dump_db, const pw_store::database &db)
                : help(help), state(state), key(key), last_lookup(last_lookup),
                  dump_db(dump_db), db(db)
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
        } gui(show_help, state, input, last_lookup, dump_db, *db);
    }

    return true;
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
        ret = add(db_password);
        break;
    case config_type::DUMP:
        ret = dump(db_password);
        break;
    case config_type::INIT:
        ret = init(db_password);
        break;
    case config_type::INTERACTIVE_ADD:
        ret = interactive_add(db_password);
        break;
    case config_type::INTERACTIVE_LOOKUP:
        ret = interactive_lookup(db_password);
        break;
    case config_type::LOOKUP:
        ret = lookup(db_password, config.lookup_key);
        break;
    }

    return ret;
}

void usage(int argc, char *argv[])
{
    std::cout << "Usage: " << argv[0] << " [flags] command [optional-key]\n"
              << "  possible flags are:\n"
              << "    -i\tinteractive can be used with add or lookup\n"
              << "  possible commands are:\n"
              << "    add\n"
              << "    dump\n"
              << "    lookup\n"
              << "  optional-key:\n"
              << "    used only for lookup command in non-interactive mode.\n";
}
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, nullptr);
    //sigignore(SIGINT);

    // arguments
    config_type config;
    config.interactive = false;
    // parse arguments
    for(int arg_index = 1; arg_index < argc; arg_index++) {
        if(argv[arg_index][0] == '-') {
            if(argv[arg_index][1] == 'i')
                config.interactive = true;
        } else { // commands
            if(!std::strcmp(argv[arg_index], "add"))
                config.mode = config_type::ADD;
            else if(!std::strcmp(argv[arg_index], "dump"))
                config.mode = config_type::DUMP;
            else if(!std::strcmp(argv[arg_index], "init"))
                config.mode = config_type::INIT;
            else if(!std::strcmp(argv[arg_index], "lookup"))
                config.mode = config_type::LOOKUP;
            else
                config.lookup_key.assign(argv[arg_index]);
        }
    }

    // check validity of arguments
    if(config.interactive) {
        if(config.lookup_key.length()) {
            usage(argc, argv);
            exit(EXIT_FAILURE);
        }
        if(config.mode == config_type::ADD)
            config.mode = config_type::INTERACTIVE_ADD;
        else if(config.mode == config_type::LOOKUP)
            config.mode = config_type::INTERACTIVE_LOOKUP;
        else {
            usage(argc, argv);
            exit(EXIT_FAILURE);
        }
    } else {
        if (config.mode != config_type::LOOKUP
            && config.lookup_key.length()) {
            usage(argc, argv);
            exit(EXIT_FAILURE);
        }
    }

    if(!run(config))
        exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
