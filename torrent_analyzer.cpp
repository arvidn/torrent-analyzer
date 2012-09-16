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

#include "libtorrent/torrent_info.hpp"
#include "libtorrent/file.hpp"
#include <map>
#include <list>
#include <string>

enum { torrent_size_quantization = 200 };

int main(int argc, char const* argv[])
{
	using libtorrent::directory;
	using libtorrent::torrent_info;
	using libtorrent::error_code;
	using libtorrent::combine_path;
	using libtorrent::announce_entry;

	std::map<int, int> piece_sizes;
	std::map<boost::uint64_t, int> torrent_sizes;
	std::map<std::string, int> creators;
	std::map<std::string, int> trackers;

	if (argc != 2)
	{
		fprintf(stderr, "usage: torrent_analyzer <directory-with-torrent-files>\n");
		return 1;
	}

	error_code ec;
	directory dir(argv[1], ec);
	int num_torrents = 0;

	int spin_cnt = 0;
	char const* spinner = "/-\\|";
	for (; !ec && !dir.done(); dir.next(ec))
	{
		fprintf(stderr, "\r%c", spinner[spin_cnt++]);
		spin_cnt %= 4;
		std::string filename = dir.file();
		if (filename.size() < 9) continue;
		if (filename.substr(filename.size() - 8) != ".torrent") continue;

		filename = combine_path(argv[1], filename);
		torrent_info ti(filename, ec);
		if (ec)
		{
			fprintf(stderr, "failed %s: %s\n", filename.c_str(), ec.message().c_str());
			continue;
		}

		// piece size
		int piece_size = ti.piece_length();
		piece_sizes[piece_size] += 1;

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

	printf("\npiece sizes:\n");

	for (std::map<int, int>::iterator i = piece_sizes.begin();
		i != piece_sizes.end(); ++i)
	{
		printf("%5d kiB: %-2.1f %%\n", i->first / 1024, float(i->second) * 100.f / num_torrents);
	}

	printf("\ncreator:\n");
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
		printf("%2.1f %%: %s\n", float(i->first) * 100.f / num_torrents, i->second.c_str());
	}


	printf("\ntrackers:\n");
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
		printf("%-4.4f %%: %s\n", float(i->first) * 100.f / num_torrents, i->second.c_str());
	}

	printf("\ntotal size:\n");
	for (std::map<boost::uint64_t, int>::iterator i = torrent_sizes.begin();
		i != torrent_sizes.end(); ++i)
	{
		printf("%-4.4f %%: %5" PRId64 " MiB\n"
			, float(i->second) * 100.f / num_torrents
			, i->first * torrent_size_quantization
				+ (torrent_size_quantization / 2));
	}

	printf("\ntotal size (CDF):\n");
	float total = 0.f;
	for (std::map<boost::uint64_t, int>::iterator i = torrent_sizes.begin();
		i != torrent_sizes.end(); ++i)
	{
		total += float(i->second) * 100.f / num_torrents;
		printf("%-4.4f %%: %5" PRId64 " MiB\n", total
			, i->first * torrent_size_quantization
			+ (torrent_size_quantization / 2));
	}
}


