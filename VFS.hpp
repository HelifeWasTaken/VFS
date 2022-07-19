#pragma once

#include <vector>
#include <string>
#include <exception>

#ifndef HELIFE_VIRTUALFS_NAMESPACE
	#define HELIFE_VIRTUALFS_NAMESPACE hl
#endif

namespace HELIFE_VIRTUALFS_NAMESPACE {

class VirtualFS {
public:
	class Error : public std::exception {
		Error(const std::string& err) : _msg(err) {}

		const char *what() const noexcept { return _msg.c_str(); }
	};

private:
	const size_t MAX_FILE_PATH_UNICODE = (MAX_PATH + 1) * sizeof(uint32_t);

	struct VFSHeader {
		char fileName[MAX_FILE_PATH_UNICODE];
		size_t fileSize;
		size_t offsetPtr;
	};

	std::vector<std::uint8_t> _buf;
#ifndef HELIFE_VIRTUALFS_DISABLE_LOG
	std::vector<std::string> _log;
#endif

	void addLog(const std::string& log)
	{
#ifndef HELIFE_VIRTUALFS_DISABLE_LOG
		_log.push_back(log);
#endif
	}

	std::vector<std::uint8_t> loadRawByteBuffer(const std::string& fileName)
	{
		std::ifstream file(fileName, ios::in | ios::binary);

		if (file.is_open() == false) {
			const std::string err = std::string("File could not be opened");
			_log.push_back(err);
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
			addLog(err);
			throw Error(err);
		}

		std::uint8_t *currentFileRawBuffer = new uint8_t[fileSize];

		file.read(currentFileRawBuffer, fileSize);

		std::vector<std::uint8_t> res(fileSize);

		res.insert(res.end(), currentFileRawBuffer, currentFileRawBuffer + fileSize);

		delete currentFileRawBuffer;

		return res;
	}

	bool isCurrentFsBufferValid() const
	{
		size_t ptr = 0;

		while (ptr != _buf.size()) {
			VFSHeader vfs;

			if (ptr + sizeof(VFSHeader) > _buf.size()) {
				return false;
			}

			memcpy(&vfs, _buf.data() + ptr, sizeof(VFSHeader));

			ptr += sizeof(VFSHeader);
			if (ptr + vfs.fileSize > _buf.size()) {
				return false;
			}
			ptr += vfs.fileSize;
		}
		return true;
	}

public:
	VirtualFS() = default;

	VirtualFS& addFile(const std::string& fileName)
	{
		if (fileName.size() >= sizeof(VFSHeader)) {
			const std::string err = std::string("File path is too long: ")
				std::to_string(fileName.size()) + " > " +
				std::to_string(MAX_FILE_PATH_UNICODE - 1);
			addLog(err);
			throw Error(err);
		}

		std::vector<uint8_t> bufFile = loadRawByteBuffer(fileName);

		_buf.reserve(_buf.size() + bufFile.size() + sizeof(VFSHeader));

		VFSHeader vfsHeader = { { 0 }, bufFile.size(),
					_buf.size() + sizeof(VFSHeader) };

		std::memcpy(&vfsHeader.fileName[0],
			fileName.c_str(),
			fileName.size() * sizeof(char));

		_buf.insert(_buf.end(),
			static_cast<const uint8_t *>(&vfsHeader),
			static_cast<const uint8_t *>(&vfsHeader) + sizeof(VFSHeader)
		);

		_buf.insert(_buf.end(),
			bufFile.data(),
			bufFile.data() + bufFile.size() * sizeof(char)
		);
		delete currentFileRawBuffer;
		return *this;
	}

	VirtualFS& addDirectory(const std::string& dirName, const std::string& prefix="")
	{
		const std::string fullPath = prefix + dirName;

		for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
			try {
				if (entry.is_regular_file()) {
					addFile(fullPath + "/" + entry.path());
				} else if (entry.is_directory()) {
					addDirectory(entry.path(), fullPath + "/");
				} else {
					const std::string err = std::string("Entry: [")
						+ fullPath + "/" + entry +
						"] is neither a regular file or directory";


					addLog(err);
				}
			} catch (const Error& e) {
				addLog(e.what());
			}
		}
		return *this;
	}

	VirtualFS& storeFS(const std::string& fsName="db.hvfs")
	{
		std::ostream outFile(fsName, std::ofstream::binary);

		outFile.write(_buf.data(), _buf.size());
		return *this;
	}


	VirtualFS& loadFS(const std::string& fsName="db.hvfs")
	{
		_buf.clear();
		_buf = std::move(loadRawByteBuffer(fsName));

		if (isCurrentFsBufferValid() == false) {
			const std::string err = std::string("Vfs: Loaded buffer: [" + fsName + "] is not valid");
			addLog(err);
			_buf.clear();
			throw Error(err);
		}
		return *this;
	}

	const std::vector<std::uint8_t>& bytes() const
	{
		return _buf;
	}
};

class VirtualFSReader {
public:
	class Error : public VirtualFS::Error {
		Error(const std::string& err) : VirtualFS::Error(err) {}
	};

	using LookupContainer = std::unordered_map<std::string, VirtualFS::VFSHeader>;
	using iterator = LookupContainer::iterator;

private:
	VirtualFS& _vfsRef;
	LookupContainer _vfsElements;

	void _load()
	{
		const auto& bytes = _vfsRef.bytes();
		size_t offset = 0;

		while (offset != _vfsRef.size()) {
			VirtualFS::VFSHeader header;

			std::memcpy(&header, bytes.data() + offset, sizeof(VirtualFS::VFSHeader));
			_vfsElements[std::string(header.fileName)] = header;
			offset = header.offsetPtr + header.fileSize;
		}
	}

public:
	explicit VirtualFSReader(VirtualFS& vfsRef) : _vfsRef(vfsRef) { _load(); }

	VirtualFSReader& operator=(const VirtualFSReader&) = delete;
	VirtualFSReader& operator=(VirtualFSReader&&) = delete;
	VirtualFSReader(const VirtualFSReader&) = delete;
	VirtualFSReader(VirtualFSReader&&) = delete;

	const VirtualFS::VFSHeader& getHeader(const std::string& s) const
	{
		auto it = _vfsElements.find(s);

		if (it == _vfsElements.end()) {
			throw Error("VfsReader: Could not find [" + s + "] in the VFSElements");
		}
		return it->second;
	}

	const std::uint8_t *getAsRawBytes(const VirtualFS::VFSHeader& elem) const
	{
		return _vfsRef.bytes().data() + elem.offsetPtr;
	}

	std::vector<std::uint8_t> getAsVector(const VirtualFS::VFSHeader& elem) const
	{
		const auto& bytes = _vfsRef.bytes();
		std::vector<std::uint8_t> res(bytes.size());

		res.insert(res.end(), bytes.data(), bytes.data() + bytes.size());
		return res;
	}

	template<typename Result, typename Reinterpreter>
	Result getElementAndReinterpretIt(const std::string& elementName,
				     const Reinterpreter& reinterpreter,
				     const bool& reinterpreterUsesRawBytes=false)
	{
		if (reinterpreterUsesRawBytes)
			return reinterpreter(getAsRawBytes(getHeader(elementName)));
		else
			return reinterpreter(getAsVector(getHeader(elementName)));
	}

	template<typename Result, typename Reinterpreter>
	Result getElementAndReinterpretIt(const VirtualFS::VFSHeader& elem,
				     const Reinterpreter& reinterpreter,
				     const bool& reinterpreterUsesRawBytes=false)
	{
		if (reinterpreterUsesRawBytes)
			return reinterpreter(getAsRawBytes(elem));
		else
			return reinterpreter(getAsVector(elem));
	}

	template<typename Result, typename Reinterpreter>
	Result getElementAndReinterpretIt(const iterator& elem,
				     const Reinterpreter& reinterpreter,
				     const bool& reinterpreterUsesRawBytes=false)
	{
		if (reinterpreterUsesRawBytes)
			return reinterpreter(getAsRawBytes(elem->second));
		else
			return reinterpreter(getAsVector(elem->second));
	}

	iterator begin() { return _vfsElements.begin(); }
	iterator end() { return _vfsElements.end(); }
};

}
