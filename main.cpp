#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <filesystem>

#include "same-inode.hpp"

#define YELLOW "\x1b[0;33m"
#define RESET "\x1b[0m"

#define BUF_SIZE 256

using std::string;
using std::vector;

const string version = "v0.0.6";

void usage(string programName) {
	std::cout << "Usage: " << programName << " [text]\n";
	std::cout << "search " << version << '\n';
}

void panic(const string msg) {
	std::cerr << msg << '\n';
	exit(1);
}

const vector<string> nonTextFileExtensions{
	".png",
	".jpg",
	".jpeg",
	".jfif",
	".flif",
	".tiff",
	".gif",
	".webp",
	".bmp",

	".pack"
};

bool should_search_file(const string filename) {
	if (filename.starts_with("./.git"))
		return false;

	for (string extension : nonTextFileExtensions) {
		if (filename.ends_with(extension))
			return false;
	}

	const std::filesystem::perms permissions = std::filesystem::status(filename).permissions();

	if ((std::filesystem::perms::owner_exec & permissions) != std::filesystem::perms::none)
		return false;
	if ((std::filesystem::perms::group_exec & permissions) != std::filesystem::perms::none)
		return false;
	if ((std::filesystem::perms::others_exec & permissions) != std::filesystem::perms::none)
		return false;

	return true;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	char *searchText = argv[1];
	if (strlen(searchText) == 0) {
		usage(argv[0]);
		return 0;
	}


	struct stat stdoutStat;
	fstat(STDOUT_FILENO, &stdoutStat);

	const std::filesystem::path cwd = ".";
	// TODO: ? recursive_directory_iterator
	for (const auto &entry : std::filesystem::recursive_directory_iterator(cwd)) {
		int entryFileDescriptor = open(entry.path().string().c_str(), O_RDONLY);
		if (entryFileDescriptor == -1) {
			std::cerr << "search: " << std::filesystem::relative(entry.path(), "./") << ": Failed to open\n";
			close(entryFileDescriptor);
			continue;
		}
		struct stat entryStat;
		fstat(entryFileDescriptor, &entryStat);

		// Empty file, skip
		if (entryStat.st_size == 0) {
			close(entryFileDescriptor);
			continue;
		}

		if (SAME_INODE(stdoutStat, entryStat)) {
			std::cerr << "search: " << std::filesystem::relative(entry.path(), "./") << ": input file is also the output\n";
			close(entryFileDescriptor);
			continue;
		}

		if (!should_search_file(entry.path())) {
			close(entryFileDescriptor);
			continue;
		}

		char *map = static_cast<char*>(mmap(0, entryStat.st_size, PROT_READ, MAP_SHARED, entryFileDescriptor, 0));
		if (map == MAP_FAILED) {
			std::cerr << "map failed!\n";
			close(entryFileDescriptor);
			return 1;
		}

		int lineNumber = 1;
		int lastNewLineIndex = 0;
		int nMatched = 0;
		for (int i = 0; i < entryStat.st_size; i++) {
			const char c = map[i];

			if (c == '\n') {
				lastNewLineIndex = i;
				++lineNumber;
				nMatched = 0;
				continue;
			}

			if (c == searchText[nMatched]) {
				++nMatched;
				if (nMatched == strlen(searchText)) {
					int nextNewLineIndex = 0;
					for (int j = i;; j++) {
						if (map[j] == '\n') {
							nextNewLineIndex = j;
							break;
						}
					}

					std::cout << std::filesystem::relative(entry.path(), "./").string() << ":" << lineNumber << " ";
					int hhh = lastNewLineIndex > 0;
					std::cout << string(map + lastNewLineIndex + hhh, map + i - strlen(searchText) + 1) << RESET;
					std::cout << YELLOW << string(map + i - strlen(searchText) + 1, map + i + 1) << RESET;
					std::cout << string(map + i + 1, map + nextNewLineIndex);
					std::cout << '\n';

					nMatched = 0;
					lastNewLineIndex = nextNewLineIndex;
					++lineNumber;
					i = nextNewLineIndex + 1;
				}
			} else {
				nMatched = 0;
			}
		}

		if (munmap(map, entryStat.st_size) == -1) {
			std::cerr << "Failed to munmap\n";
			close(entryFileDescriptor);
			return 1;
		}
		close(entryFileDescriptor);
	}
}
