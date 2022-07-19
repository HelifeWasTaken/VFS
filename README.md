# VFS

## Documentation (WIP)

Considering this filesystem:
```
dir
├── elem1.txt
├── elem2.txt
└── elem3.png

0 directories, 3 files
```

```cpp
int main()
{
    VirtualFS fs;
    
    fs.addDirectory("dir"); // may throw if it can't read the directory
    
    {
        VirtualFSReader reader(fs);
        
        for (auto it : reader) {
            VirtualFS::VFSHeader& header = it->second;
            // it->first == it->second.fileName should always be true
            
            std::cout << "Name: " << header.fileName
                      << ", fileSize: " << header.fileSize
                      << ", offsetPtr: " << header.offsetPtr
                      << std::endl;
        }
    }
    // prints
    // dir/elem1.txt
    // dir/elem2.txt
    // dir/elem3.png
}
```

```cpp
PNG bytesToPng(std::vector<std::uint8_t>& buffer);
PNG rawBytesToPng(std::uint8_t *buffer);

int main()
{
    VirtualFS fs;
    
    fs.addFile("dir/elem3.png", "pngfile"); // load dir/elem3.png as pngfile
    
    VirtualFSReader vfsReader(fs);
    
    PNG png = vfsReader.getElementAndReinterpretIt("pngfile", bytesToPng);
    
    // highly unsafe if the DB is not made by you or modified
    PNG unsafe_png = vfsReader.getElementAndReinterpretIt("pngfile", rawBytesToPng);
}
```

```cpp
int main()
{
    VirtualFS fs;
    
    fs.addDirectory("dir");
    
    fs.storeFS("db.hvfs");
}
```

```cpp
int main()
{
    VirtualFS fs;
    
    fs.loadFS("db.hvfs");
}
```

## TODO:

- Use compression algorithms and do not store it in raw
- Improve speed in fetching data
- Documentation about member functions
