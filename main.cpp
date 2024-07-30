#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>

#include "same-inode.h"

#define YELLOW "\x1b[0;33m"
#define RESET "\x1b[0m"

using std::string;
using std::vector;

const string version = "v0.0.4";

void usage(string programName) {
	std::cout << "Usage: " << programName << " [text]\n";
	std::cout << "search " << version << '\n';
}

bool ends_with(string str, string endsWith) {
	if (str.length() < endsWith.length())
		return false;

	if (str.substr(str.length() - endsWith.length()) == endsWith)
		return true;

	return false;
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
	for (string extension : nonTextFileExtensions) {
		if (ends_with(filename, extension))
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

	string text = argv[1];
	if (text.empty()) {
		usage(argv[0]);
		return 0;
	}

	struct stat stdoutStat;
	fstat(STDOUT_FILENO, &stdoutStat);

	const std::filesystem::path cwd = ".";
	// TODO: ? recursive_directory_iterator
	for (const auto &entry : std::filesystem::recursive_directory_iterator(cwd)) {
		int entryFileDescriptor = open(entry.path().string().c_str(), O_RDONLY);
		struct stat entryStat;
		fstat(entryFileDescriptor, &entryStat);
		if (SAME_INODE(stdoutStat, entryStat)) {
			std::cerr << "search: " << entry.path().string() << ": input file is also the output\n";
			continue;
		}

		if (!should_search_file(entry.path()))
			continue;

		std::ifstream f(entry.path());

		string line;
		for (int lineNum = 1; std::getline(f, line); lineNum++) {
			int found = line.find(text);
			if (found != string::npos) {
				std::cout << std::filesystem::relative(entry.path(), "./").string() << ":" << lineNum << " ";
				std::cout << line.substr(0, found);
				std::cout << YELLOW << line.substr(found, text.length()) << RESET;
				std::cout << line.substr(found+text.length());
				std::cout << '\n';
			}
		}
	}
}
