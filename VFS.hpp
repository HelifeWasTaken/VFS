#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <exception>
#include <filesystem>
#include <fstream>
#include <cstring>

#ifndef HELIFE_VIRTUALFS_NAMESPACE
#define HELIFE_VIRTUALFS_NAMESPACE hl
#endif

#define HELIFE_VIRTUALFS_MAX_FILE_PATH (4095 + 1)

#ifndef __GNUC__
    #define HELIFE_VIRTUALFS_PACKED __attribute__((packed))
#else
    #define HELIFE_VIRTUALFS_PACKED
#endif

namespace HELIFE_VIRTUALFS_NAMESPACE {

    using VFSFilename = char[HELIFE_VIRTUALFS_MAX_FILE_PATH];

    extern "C" {
        struct VFSHeader {
            VFSFilename fileName = { 0 };
            std::size_t fileSize = -1;
        } HELIFE_VIRTUALFS_PACKED;
    }

    class VirtualFS {
        public:
            class Error : public std::exception {
                private:
                    const std::string _msg;
                public:
                    Error(const std::string& err);

                    const char *what() const noexcept;
            };

            using VFSLookupMap = std::unordered_map<std::string, std::vector<std::uint8_t>>;

        private:
            VFSLookupMap _fs;

            static std::vector<std::uint8_t> loadRawByteBuffer(const std::string& fileName);

            static bool isBufferVFSValid(const std::vector<std::uint8_t>& buf);

            std::vector<std::uint8_t>& getElementByName(const std::string& name);

            bool hasElement(const std::string& name) const;

            void checkFilePathSize(const std::string& name);

        public:
            VirtualFS() = default;

            VirtualFS& addFile(const std::string& name);

            VirtualFS& addFile(const std::string& fileName, const std::string& toRename);

            VirtualFS& addDirectory(const std::string& dirName, const std::string& prefix="");

            VirtualFS& storeFS(const std::string& fsName="db.hvfs");

            VirtualFS& loadFS(const std::string& fsName="db.hvfs");

            VirtualFS& remove(const std::string& name);

            VirtualFS& rename(const std::string& originalName, const std::string& newName);

            // If loading `filePath` fails `originalName` will still be removed
            // This function is used to update a single file
            VirtualFS& updateFile(const std::string& originalName, const std::string& filePath);

            // If loading `filePath` fails `originalName` will still be removed
            // This function is used to update a single file
            VirtualFS& updateFile(const std::string& originalName);

            const std::vector<std::uint8_t>& get(const std::string& name) const;

            VirtualFS& clear();

    };
}
