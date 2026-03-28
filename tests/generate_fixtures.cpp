// Standalone fixture generator for .wpress test files.
// Build: g++ -std=c++11 -o generate_fixtures generate_fixtures.cpp -lz
// Run: ./generate_fixtures

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

static const int HEADER_SIZE = 4377;

struct WpressHeader {
    char filename[255];
    char filesize[14];
    char mtime[12];
    char filepath[4096];
    // total: 255 + 14 + 12 + 4096 = 4377
};

static unsigned long g_crc = 0;

static void crcWrite(std::ofstream &out, const char *data, size_t len)
{
    g_crc = crc32(g_crc, reinterpret_cast<const Bytef *>(data), static_cast<uInt>(len));
    out.write(data, len);
}

static void writeHeader(std::ofstream &out, const std::string &name, const std::string &content,
                        const std::string &path = ".", const std::string &mtime = "", bool useCrc = false)
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

    // mtime (269-280)
    if (!mtime.empty()) {
        std::memcpy(header + 269, mtime.c_str(), std::min(mtime.size(), (size_t)12));
    }

    if (useCrc) {
        // v2 format: filepath at 281, length 4088
        std::strncpy(header + 281, path.c_str(), 4087);

        // Compute CRC32 of the file content (decompressed)
        unsigned long fileCrc = crc32(0L, Z_NULL, 0);
        fileCrc = crc32(fileCrc, reinterpret_cast<const Bytef *>(content.c_str()), static_cast<uInt>(content.size()));
        char crcHex[9];
        std::snprintf(crcHex, sizeof(crcHex), "%08x", static_cast<unsigned int>(fileCrc));
        std::memcpy(header + 4369, crcHex, 8);
    } else {
        // v1 format: filepath at 281, length 4096
        std::strncpy(header + 281, path.c_str(), 4095);
    }

    if (useCrc) {
        crcWrite(out, header, HEADER_SIZE);
        crcWrite(out, content.c_str(), content.size());
    } else {
        out.write(header, HEADER_SIZE);
        out.write(content.c_str(), content.size());
    }
}

static void writeEof(std::ofstream &out, bool useCrc = false)
{
    char eof[HEADER_SIZE];
    std::memset(eof, 0, HEADER_SIZE);

    if (useCrc) {
        // v2 EOF: filename all null, size field non-empty, CRC of all preceding data
        // Size field: write the total data size as a placeholder
        std::string sizeStr = "0";
        while (sizeStr.size() < 13)
            sizeStr += ' ';
        std::memcpy(eof + 255, sizeStr.c_str(), 13);

        // Compute archive CRC over everything so far (g_crc) plus the EOF header up to CRC field
        crcWrite(out, eof, 4369);

        char crcHex[9];
        std::snprintf(crcHex, sizeof(crcHex), "%08x", static_cast<unsigned int>(g_crc));
        out.write(crcHex, 8);
    } else {
        out.write(eof, HEADER_SIZE);
    }
}

static std::string zlibCompress(const std::string &input)
{
    // zlib compress with 4-byte chunk size prefix (matching All-in-One WP Migration format)
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    std::vector<char> outBuf(input.size() + 1024);
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.c_str()));
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_out = reinterpret_cast<Bytef *>(outBuf.data());
    strm.avail_out = static_cast<uInt>(outBuf.size());

    deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    size_t compressedSize = outBuf.size() - strm.avail_out;

    // Prepend 4-byte chunk size
    std::string result;
    char sizePrefix[4];
    uint32_t chunkSize = static_cast<uint32_t>(compressedSize);
    std::memcpy(sizePrefix, &chunkSize, 4);
    result.append(sizePrefix, 4);
    result.append(outBuf.data(), compressedSize);
    return result;
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

    // 5. v2 .wpress with per-file CRC and mtime
    {
        g_crc = crc32(0L, Z_NULL, 0);
        std::ofstream f("fixtures/v2crc.wpress", std::ios::binary);
        std::string config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        writeHeader(f, "package.json", config, ".", "1700000000", true);
        writeHeader(f, "hello.txt", "Hello, World!\n", "wp-content", "1700000000", true);
        writeHeader(f, "data.txt", "Some test data for CRC verification.", ".", "1700000000", true);
        writeEof(f, true);
        std::cout << "Created: fixtures/v2crc.wpress" << std::endl;
    }

    // 6. zlib-compressed .wpress
    {
        std::ofstream f("fixtures/compressed.wpress", std::ios::binary);
        std::string config = R"({"Encrypted":false,"Compression":{"Enabled":true,"Type":"zlib"}})";
        // Config file is NOT compressed even in compressed archives
        writeHeader(f, "package.json", config);

        // Compressed data file
        std::string original = "This is test content that will be compressed with zlib.\n";
        std::string compressed = zlibCompress(original);
        // Write header with compressed size, but content is the compressed bytes
        char header[HEADER_SIZE];
        std::memset(header, 0, HEADER_SIZE);
        std::strncpy(header, "compressed.txt", 254);
        std::string sizeStr = std::to_string(compressed.size());
        while (sizeStr.size() < 13)
            sizeStr += ' ';
        std::memcpy(header + 255, sizeStr.c_str(), 13);
        std::strncpy(header + 281, "wp-content", 4095);
        f.write(header, HEADER_SIZE);
        f.write(compressed.c_str(), compressed.size());

        writeEof(f);
        std::cout << "Created: fixtures/compressed.wpress" << std::endl;
    }

    std::cout << "All fixtures generated." << std::endl;
    return 0;
}
