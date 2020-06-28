/*

Copyright (c) 2012, Arvid Norberg, Magnus Jonsson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include <boost/filesystem.hpp>
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/file.hpp"
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <cinttypes>

int const metadata_size_quantization = 1024;
int const torrent_size_quantization = 5;
int const piece_count_quantization = 100;

bool load_file(std::string const& filename, std::vector<char>& v)
{
	std::fstream f(filename, std::ios_base::in | std::ios_base::binary);
	f.seekg(0, std::ios_base::end);
	auto const s = f.tellg();
	f.seekg(0, std::ios_base::beg);
	v.resize(static_cast<std::size_t>(s));
	if (s == std::fstream::pos_type(0)) return !f.fail();
	f.read(v.data(), v.size());
	return !f.fail();
}

int main(int argc, char const* argv[])
{
	using libtorrent::torrent_info;
	using libtorrent::error_code;
	using libtorrent::announce_entry;

	std::map<int, int> piece_sizes;
	std::map<int, int> piece_counts;
	std::map<boost::uint64_t, int> torrent_sizes;
	std::map<int, int> metadata_sizes;
	std::map<std::string, int> creators;
	std::map<std::string, int> trackers;

	if (argc < 2)
	{
		fprintf(stderr, "usage: torrent_analyzer <directory-with-torrent-files> ...\n");
		return 1;
	}

	int num_torrents = 0;

	int spin_cnt = 0;
	char const* spinner = "/-\\|";

	namespace fs = boost::filesystem;

	std::vector<char> buffer;

	error_code ec;
	for (fs::recursive_directory_iterator it(argv[1]), endit;
		it != endit; ++it)
	{
		if (!fs::is_directory(it->path()))
		{
			fprintf(stderr, "\r%c", spinner[spin_cnt++]);
			spin_cnt %= 4;
			auto filename = it->path();
			if (extension(filename) != ".torrent") continue;

			if (!load_file(filename.native(), buffer))
			{
				fprintf(stderr, "failed to load %s\n", filename.c_str());
				continue;
			}

			torrent_info ti(buffer, ec, lt::from_span);
			if (ec)
			{
				fprintf(stderr, "failed %s: %s\n", filename.c_str(), ec.message().c_str());
				continue;
			}

			metadata_sizes[buffer.size() / metadata_size_quantization] += 1;

			// piece size
			int const piece_size = ti.piece_length();
			piece_sizes[piece_size] += 1;

			int const num_pieces = ti.num_pieces();
			piece_counts[num_pieces / piece_count_quantization] += 1;

			// creator
			std::string creator = ti.creator();
			if (creator.substr(0, 4) != "http") creator = creator.substr(0, creator.find_first_of('/'));
			creators[creator] += 1;

			// torrent size
			boost::uint64_t torrent_size = ti.total_size();
			torrent_sizes[torrent_size / (torrent_size_quantization*1024*1024)] += 1;

			// tracker
			std::vector<announce_entry> const& tr = ti.trackers();
			for (std::vector<announce_entry>::const_iterator i = tr.begin()
				, end(tr.end()); i != end; ++i)
			{
				std::string t = i->url;
				if (t.size() > 5 && t.substr(0, 6) == "dht://") t = "dht://xxxxx";
				trackers[t] += 1;
			}

			++num_torrents;
		}
	}

	printf("writing piece_size.dat\n");
	FILE* f = fopen("piece_size.dat", "w+");
	double sum = 0.0;
	for (std::map<int, int>::iterator i = piece_sizes.begin();
		i != piece_sizes.end(); ++i)
	{
		sum += i->second;
		fprintf(f, "%5d\t%-2.1f\n", i->first / 1024, sum * 100. / num_torrents);
	}
	fclose(f);

	printf("writing piece_count.dat\n");
	f = fopen("piece_count.dat", "w+");
	sum = 0.0;
	for (auto const e : piece_counts)
	{
		sum += e.second;
		fprintf(f, "%5d\t%-4.4f\n"
			, e.first * piece_count_quantization + (piece_count_quantization / 2)
			, sum * 100. / num_torrents);
	}
	fclose(f);

	printf("writing size.dat\n");
	f = fopen("size.dat", "w+");
	sum = 0.0;
	for (std::map<boost::uint64_t, int>::iterator i = torrent_sizes.begin();
		i != torrent_sizes.end(); ++i)
	{
		sum += i->second;
		fprintf(f, "%" PRId64 "\t%-4.4f\n"
			, i->first * torrent_size_quantization + (torrent_size_quantization / 2)
			, sum * 100. / num_torrents);
	}
	fclose(f);

	printf("writing metadata_size.dat\n");
	f = fopen("metadata_size.dat", "w+");
	sum = 0.0;
	for (std::map<int, int>::iterator i = metadata_sizes.begin();
		i != metadata_sizes.end(); ++i)
	{
		sum += i->second;
		fprintf(f, "%d\t%-4.4f\n", i->first, sum * 100. / num_torrents);
	}
	fclose(f);

	printf("writing creator.txt\n");
	f = fopen("creator.txt", "w+");
	std::vector<std::pair<int, std::string> > sorted_list;
	for (std::map<std::string, int>::iterator i = creators.begin();
		i != creators.end(); ++i)
	{
		sorted_list.push_back(std::make_pair(i->second, i->first));
	}

	std::sort(sorted_list.rbegin(), sorted_list.rend());

	for (std::vector<std::pair<int, std::string> >::iterator i = sorted_list.begin();
		i != sorted_list.end(); ++i)
	{
		fprintf(f, "%2.1f %%: %s\n", float(i->first) * 100.f / num_torrents, i->second.c_str());
	}
	fclose(f);


	printf("writing tracker.txt\n");
	f = fopen("tracker.txt", "w+");
	sorted_list.clear();

	for (std::map<std::string, int>::iterator i = trackers.begin();
		i != trackers.end(); ++i)
	{
		sorted_list.push_back(std::make_pair(i->second, i->first));
	}

	std::sort(sorted_list.rbegin(), sorted_list.rend());

	for (std::vector<std::pair<int, std::string> >::iterator i = sorted_list.begin();
		i != sorted_list.end(); ++i)
	{
		fprintf(f, "%-4.4f %%: %s\n", float(i->first) * 100.f / num_torrents, i->second.c_str());
	}
	fclose(f);
}


