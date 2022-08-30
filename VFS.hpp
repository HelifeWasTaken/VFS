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

namespace HELIFE_VIRTUALFS_NAMESPACE {

    using VFSFilename = char[HELIFE_VIRTUALFS_MAX_FILE_PATH];

    extern "C" {
        struct VFSHeader {
            VFSFilename fileName = { 0 };
            std::size_t fileSize = -1;
        };
    }

    class VirtualFS {
        public:
            class Error : public std::exception {
                private:
                    const std::string _msg;
                public:
                    Error(const std::string& err) : _msg(err) {}

                    const char *what() const noexcept { return _msg.c_str(); }
            };

            using VFSLookupMap = std::unordered_map<std::string, std::vector<std::uint8_t>>;

        private:
            VFSLookupMap _fs;

            static inline std::vector<std::uint8_t> loadRawByteBuffer(const std::string& fileName)
            {
                std::ifstream file(fileName, std::fstream::in | std::fstream::binary);

                if (file.is_open() == false) {
                    const std::string err = std::string("File could not be opened");
                    throw Error(err);
                }

                std::error_code ec;
                std::uintmax_t fileSize = std::filesystem::file_size(
                        std::filesystem::path(fileName),
                        ec);
                if (ec) {
                    const std::string err = std::string("Vfs: ")
                        + fileName + " " +
                        std::string(ec.message());
                    throw Error(err);
                }

                std::uint8_t *currentFileRawBuffer = new uint8_t[fileSize];

                file.read(reinterpret_cast<char *>(currentFileRawBuffer), fileSize);

                std::vector<std::uint8_t> res;

                res.insert(res.end(), currentFileRawBuffer, currentFileRawBuffer + fileSize);

                std::cout << "Filesize: " << fileSize << std::endl;

                delete[] currentFileRawBuffer;

                return res;
            }

            static inline bool isBufferVFSValid(const std::vector<std::uint8_t>& buf)
            {
                std::size_t ptr = 0;

                while (ptr != buf.size()) {
                    std::cout << ptr << " " << buf.size() << std::endl;
                    VFSHeader vfs;

                    if (ptr + sizeof(VFSHeader) > buf.size()) {
                        std::cout << "False: " << ptr << " " << buf.size() << std::endl;
                        return false;
                    }

                    memcpy(&vfs, buf.data() + ptr, sizeof(VFSHeader));

                    ptr += sizeof(VFSHeader);
                    if (ptr + vfs.fileSize > buf.size()) {
                        std::cout << "False2: " << ptr << " " << buf.size() << " " << vfs.fileSize << std::endl;
                        return false;
                    }
                    ptr += vfs.fileSize;
                }
                std::cout << "True: " << ptr << " " << buf.size() << std::endl;
                return true;
            }

            inline std::vector<std::uint8_t>& getElementByName(const std::string& name)
            {
                auto elem = _fs.find(name);

                if (elem != _fs.end())
                    return elem->second;
                const std::string err = "VFS get element: [" + name + "] failed";
                throw Error(err);
            }

            inline bool hasElement(const std::string& name) const
            {
                return _fs.find(name) != _fs.end();
            }

            inline void checkFilePathSize(const std::string& name)
            {
                if (name.size() >= HELIFE_VIRTUALFS_MAX_FILE_PATH) {
                    const std::string err = std::string("File path is too long: ") +
                        std::to_string(name.size()) +
                        " > " +
                        std::to_string(HELIFE_VIRTUALFS_MAX_FILE_PATH - 1);
                    throw Error(err);
                }
            }

        public:
            VirtualFS() = default;

            inline VirtualFS& addFile(const std::string& name)
            {
                if (hasElement(name)) {
                    std::string err = "The file: [" + name + "] already exist in the VFS";
                    throw Error(err);
                }

                checkFilePathSize(name);

                _fs[name] = loadRawByteBuffer(name);

                return *this;
            }

            inline VirtualFS& addFile(const std::string& fileName, const std::string& toRename)
            {
                checkFilePathSize(toRename);
                return addFile(fileName).rename(fileName, toRename);
            }

            inline VirtualFS& addDirectory(const std::string& dirName, const std::string& prefix="")
            {
                const std::string fullPath = prefix + dirName;

                for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
                    try {
                        if (entry.is_regular_file()) {
                            addFile(fullPath + "/" + entry.path().string());
                        } else if (entry.is_directory()) {
                            addDirectory(entry.path(), fullPath + "/");
                        } else {
                            // TODO might do something here maybe warning
                        }
                    } catch (const Error& e) {
                        // TODO mostly because
                    }
                }
                return *this;
            }

            inline VirtualFS& storeFS(const std::string& fsName="db.hvfs")
            {
                std::fstream outFile(fsName, std::fstream::out | std::fstream::binary);

                for (const auto& it : _fs) {
                    VFSHeader header = { 0 };

                    header.fileSize = it.second.size();
                    std::memcpy(header.fileName, it.first.c_str(), it.first.size());

                    outFile.write(reinterpret_cast<const char *>(&header), sizeof(VFSHeader));
                    outFile.write(reinterpret_cast<const char *>(it.second.data()), it.second.size());
                }
                return *this;
            }

            inline VirtualFS& loadFS(const std::string& fsName="db.hvfs")
            {
                std::vector<std::uint8_t> buf;

                buf = std::move(loadRawByteBuffer(fsName));

                std::cout << buf.size() << std::endl;

                if (isBufferVFSValid(buf) == false) {
                    const std::string err = std::string("Vfs: Loaded buffer: [" + fsName + "] is not valid");
                    throw Error(err);
                }

                std::size_t ptr = 0;
                VFSHeader header = { 0 };

                while (ptr != buf.size()) {
                    std::memcpy(&header, buf.data() + ptr, sizeof(VFSHeader));

                    if (hasElement(header.fileName)) {
                        const std::string err = "The file : ["
                            + std::string(header.fileName) +
                            "] is already in the VFS.";
                        throw Error(err);
                    }

                    _fs[header.fileName] = std::vector<std::uint8_t>(
                            buf.begin() + ptr + sizeof(VFSHeader),
                            buf.begin() + ptr + sizeof(VFSHeader) + header.fileSize
                            );
                    ptr += sizeof(VFSHeader) + header.fileSize;
                }

                return *this;
            }


            inline VirtualFS& remove(const std::string& name)
            {
                if (_fs.find(name) != _fs.end()) {
                    _fs.erase(name);
                }
                return *this;
            }

            inline VirtualFS& rename(const std::string& originalName, const std::string& newName)
            {
                checkFilePathSize(newName);

                if (!hasElement(originalName)) {
                    const std::string err = "The original name : [" + originalName
                        + "] is already in the VFS.";
                    throw Error(err);
                }

                if (hasElement(newName)) {
                    const std::string err = "The new name: [" + newName
                        + "] is already in the VFS.";
                    throw Error(err);
                }

                auto data = std::move(_fs[originalName]);
                _fs[newName] = std::move(data);
                _fs.erase(originalName);

                return *this;
            }

            // If loading `filePath` fails `originalName` will still be removed
            // This function is used to update a single file
            inline VirtualFS& updateFile(const std::string& originalName, const std::string& filePath)
            {
                return remove(originalName).addFile(filePath, originalName);
            }

            // If loading `filePath` fails `originalName` will still be removed
            // This function is used to update a single file
            inline VirtualFS& updateFile(const std::string& originalName)
            {
                return updateFile(originalName, originalName);
            }

            inline const std::vector<std::uint8_t>& get(const std::string& name) const
            {
                auto it = _fs.find(name);

                if (it == _fs.end()) {
                    const std::string err = "The file: " + name + " could not be found in the VFS";
                    throw Error(err);
                }
                return it->second;
            }

            inline VirtualFS& clear()
            {
                _fs.clear();
                return *this;
            }

    };
}
