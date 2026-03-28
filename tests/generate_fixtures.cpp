// Standalone fixture generator for .wpress test files.
// Build: g++ -std=c++11 -o generate_fixtures generate_fixtures.cpp
// Run: ./generate_fixtures

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

static const int HEADER_SIZE = 4377;

struct WpressHeader {
    char filename[255];
    char filesize[14];
    char mtime[12];
    char filepath[4096];
    // total: 255 + 14 + 12 + 4096 = 4377
};

static void writeHeader(std::ofstream &out, const std::string &name, const std::string &content,
                        const std::string &path = ".")
{
    char header[HEADER_SIZE];
    std::memset(header, 0, HEADER_SIZE);

    // Filename (0-254)
    std::strncpy(header, name.c_str(), 254);

    // File size (255-268), right-padded with spaces
    std::string sizeStr = std::to_string(content.size());
    while (sizeStr.size() < 13)
        sizeStr += ' ';
    std::memcpy(header + 255, sizeStr.c_str(), 13);

    // mtime (269-280) - skip, leave as zeros

    // File path (281-4376)
    std::strncpy(header + 281, path.c_str(), 4095);

    out.write(header, HEADER_SIZE);
    out.write(content.c_str(), content.size());
}

static void writeEof(std::ofstream &out)
{
    char eof[HEADER_SIZE];
    std::memset(eof, 0, HEADER_SIZE);
    out.write(eof, HEADER_SIZE);
}

int main()
{
    // 1. Plain .wpress with a config file and one data file
    {
        std::ofstream f("fixtures/plain.wpress", std::ios::binary);
        std::string config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        writeHeader(f, "package.json", config);
        writeHeader(f, "hello.txt", "Hello, World!\n", "wp-content");
        writeEof(f);
        std::cout << "Created: fixtures/plain.wpress" << std::endl;
    }

    // 2. Empty .wpress (valid, zero-byte)
    {
        std::ofstream f("fixtures/empty.wpress", std::ios::binary);
        // zero-size file is valid per BackupFile::isValid()
        std::cout << "Created: fixtures/empty.wpress" << std::endl;
    }

    // 3. Corrupted .wpress (no EOF block)
    {
        std::ofstream f("fixtures/corrupted.wpress", std::ios::binary);
        std::string config = R"({"Encrypted":false})";
        writeHeader(f, "package.json", config);
        writeHeader(f, "data.txt", "some data", ".");
        // NO EOF block -> isValid() returns false
        std::cout << "Created: fixtures/corrupted.wpress" << std::endl;
    }

    // 4. Multi-file .wpress
    {
        std::ofstream f("fixtures/multifile.wpress", std::ios::binary);
        std::string config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        writeHeader(f, "package.json", config);
        writeHeader(f, "index.php", "<?php echo 'hi'; ?>", "wp-content/themes/test");
        writeHeader(f, "style.css", "body { color: red; }", "wp-content/themes/test");
        writeHeader(f, "readme.txt", "Test archive", ".");
        writeEof(f);
        std::cout << "Created: fixtures/multifile.wpress" << std::endl;
    }

    std::cout << "All fixtures generated." << std::endl;
    return 0;
}
