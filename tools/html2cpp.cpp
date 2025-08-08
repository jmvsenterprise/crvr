#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

void convert_file(const std::filesystem::path& path)
{
	std::cout << "const std::string " << path.stem().string() << " = {\n";
	std::fstream file{path, std::ios::in};
	for (std::string line; std::getline(file, line); ) {
		std::string::size_type double_quote = 0;
		while ((double_quote = line.find('"', double_quote))
				!= line.npos) {
			line.insert(double_quote, 1, '\\');
			double_quote += 2;
		}
		std::cout << "\t\"" << line << "\"\n";
	}
	std::cout << "};";
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; ++i) {
		std::filesystem::path path{argv[i]};
		convert_file(path);
	}
}
