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

#include <tuple>

#include "libaan/string_util.hh"

bool pw_store::database::parse()
{
    line_count = 0;
    if (!string_buffer.size())
        return true;

    const auto lines = libaan::util::split2(string_buffer, "\n");

    for(const auto &line_buff: lines) {
        if(!line_buff.second)
            continue;
        const auto line = std::string(line_buff.first, line_buff.second);
        const auto fields = libaan::util::split2(line, "\t");
        if(fields.size() != 4) {
            std::cerr << "Error: corrupt database file.\n";
            std::cerr << "\tfields.size() = " << fields.size() << "\n";
            std::cerr << "\tline = \"" << line << "\"\n";
            return false;
        }

        data_type date(std::string(fields[0].first, fields[0].second),
                       std::string(fields[1].first, fields[1].second),
                       std::string(fields[2].first, fields[2].second));

        insert(date);
        ++line_count;
    }
    return true;
}

bool pw_store::database::insert(const data_type &date)
{
    urluserpw.push_back(
        std::make_tuple(date.url_string, date.username, date.password));
    list_cmp cmp;
    std::sort(urluserpw.begin(), urluserpw.end(), cmp);
    dirty = true;

    return true;
}

void pw_store::database::lookup(const std::string &key,
                                std::list<std::tuple<data_type::id_type, data_type> > &matches)
{
    // Just do a linear search for now. The number of keys won't be that large.
    const auto &fail = std::string::npos;
    data_type::id_type idx = 0;
    for (const auto &k : urluserpw) {
        const std::string &url = std::get<0>(k);
        const std::string &user = std::get<1>(k);
        const bool url_match = url.find(key) != fail;
        const bool user_match = user.find(key) != fail;

        if (url_match || user_match)
            matches.push_back(std::make_tuple(idx, to_data_type(k)));

/*
        std::string debug_out = "key(\"" + key + "\") -> ";
        if (url_match)
            debug_out += "url_matched(\"" + std::get<0>(k) + "\") ";
        if (user_match)
            debug_out += "user_matched(\"" + std::get<1>(k) + "\")";
        if (url_match || user_match)
            std::cout << debug_out << "\n";
*/

        idx++;
    }
}

void pw_store::database::synchronize_buffer()
{
    if(!dirty)
        return;

    //std::fill(string_buffer.begin(), string_buffer.end(), 0);
    string_buffer.resize(0);
    for(const auto &tuple: urluserpw) {
        string_buffer.append(std::get<0>(tuple));
        string_buffer.append(DELIM);
        string_buffer.append(std::get<1>(tuple));
        string_buffer.append(DELIM);
        string_buffer.append(std::get<2>(tuple));
        string_buffer.append(DELIM);
        string_buffer.append("\n");
    }

    dirty = false;
}

void pw_store::database::clear_all_buffers()
{
    for(auto &tuple: urluserpw) {
        std::fill(std::get<0>(tuple).begin(), std::get<0>(tuple).end(), 0);
        std::fill(std::get<1>(tuple).begin(), std::get<1>(tuple).end(), 0);
        std::fill(std::get<2>(tuple).begin(), std::get<2>(tuple).end(), 0);
    }
    urluserpw.resize(0);
}

void pw_store::database::dump_db() const
{
    data_type::id_type idx = 0;
    for(const auto &key: urluserpw)
        std::cout << "\t" << idx++ << ": " << to_data_type(key) << "\n";
}
