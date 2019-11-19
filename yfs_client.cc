// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>


bool operator==(const yfs_client::dirent &d1, const yfs_client::dirent &d2) {
    return (d1.inum == d2.inum && d1.name == d2.name);
}

yfs_client::yfs_client()
{
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  cache = new local_cache();
  lc = new lock_client_cache(lock_dst, ec, cache);
  // lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  // lc->release(1);
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    lc->acquire(inum);

    printf("get lock %d in isfile\n", inum);

    if (_getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        lc->release(inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
    lc->release(inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    // return ! isfile(inum);
    extent_protocol::attr a;

    lc->acquire(inum);

    printf("get lock %d in isdir\n", inum);

    if (_getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a directory", inum);
        lc->release(inum);
        return true;
    }
    printf("isdir: %lld is not a directory", inum);
    lc->release(inum);
    return false;
}

bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    lc->acquire(inum);

    printf("get lock %d in issymlink\n", inum);

    if (_getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("issymlink: %lld is a symlink", inum);
        lc->release(inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink", inum);
    lc->release(inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    lc->acquire(inum);

    printf("get lock %d in getfile\n", inum);

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (_getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    lc->acquire(inum);

    printf("get lock %d in getdir\n", inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (_getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lc->release(inum);
    return r;
}

int
yfs_client::directory2list(std::string &data, std::list<dirent> &result) {
    int r = OK;
    std::string data_copy = data;
    if (data == "")
        return OK;
    while (true) {

        // retrieve the first sizeof(int) characters in data_copy and convert to int
        char int_buf[sizeof(int)];
        for (unsigned long long i = 0; i < sizeof(int); ++i) {
            int_buf[i] = data_copy[i];
        }
        int entry_length = *(int *)int_buf;
        // std::cout << "entry_length: " << entry_length << std::endl;
        if (entry_length == 0)
            break;

        // retrieve the entry name
        std::string name;
        int name_length = entry_length - sizeof(int) - sizeof(unsigned long long);
        for (unsigned long long i = sizeof(int); i < sizeof(int) + name_length; ++i) {
            name.push_back(data_copy[i]);
        }

        // retrieve the inum
        char inum_buf[sizeof(unsigned long long)];
        unsigned long long inum_offset = sizeof(int) + name_length;
        for (unsigned long long i = inum_offset; i < inum_offset + sizeof(unsigned long long); ++i) {
            inum_buf[i - inum_offset] = data_copy[i];
        }
        unsigned long long inum = *(unsigned long long*)inum_buf;
        // std::cout << "inum: " << inum << std::endl;
        dirent d;
        d.name = name;
        d.inum = inum;
        result.push_back(d);
        data_copy = data_copy.substr(entry_length);
    }
    return r;
}

int
yfs_client::list2directory(std::string &data, std::list<dirent> &result) {
    for (std::list<dirent>::iterator it = result.begin(); it != result.end(); ++it) {
        struct dirent d = *it;
        int length = sizeof(int) + d.name.size() + sizeof(unsigned long long);
        char* length_p = (char*) &length;
        for (unsigned long long j = 0; j < sizeof(int); ++j) {
            data.push_back(length_p[j]);
        }
        for (int j = 0; j < d.name.size(); ++j) {
            data.push_back(d.name[j]);
        }
        unsigned long long inum = d.inum;
        char* inum_p = (char*) &inum;
        for (int j = 0; j < sizeof(unsigned long long); ++j) {
            data.push_back(inum_p[j]);
        }
    }
    int zero = 0;
    char* zero_p = (char *) &zero;
    for (int i = 0; i < sizeof(int); ++i) {
        data.push_back(zero_p[i]);    
    }
    // std::cout << "Finish list2directory" << std::endl;
    return OK;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    lc->acquire(ino);

    printf("get lock %d in setattr\n", ino);

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::status status;
    std::string buf;
    status = _get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::setattr: _get(ino, buf)" << std::endl;
        lc->release(ino);
        return RPCERR;
    }
    size_t original_size = buf.size();
    if (size == 0) {
        std::string write_buf;
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: _put(ino, write_buf) 0" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        lc->release(ino);
        return OK;
    }
    if (size > original_size) {
        // try to set size larger than the original size, add paddings
        std::string write_buf(size, 0);
        for (size_t i = 0; i < original_size; ++i) {
            write_buf[i] = buf[i];
        }
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: _put(ino, write_buf) 1" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        lc->release(ino);
        return OK;
    }
    if (size < original_size) {
        // try to set a smaller size
        std::string write_buf(size, 0);
        for (size_t i = 0; i < size; ++i) {
            write_buf[i] = buf[i];
        }
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: _put(ino, write_buf) 2" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        lc->release(ino);
        return OK;
    }
    lc->release(ino);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    lc->acquire(parent);

    printf("get lock %d in create\n", parent);

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent information.
     */
    bool found = false;

    /* Lookup and set found's value */
    {
        std::string local_buf;
        std::list<dirent> dirent_list;
        _get(parent, local_buf);
        directory2list(local_buf, dirent_list);
        for (std::list<dirent>::iterator it = dirent_list.begin(); it != dirent_list.end(); ++it) {
            if (it->name.compare(name) == 0) {
                // find it!
                found = true;
                break;
            }
        }
    }

    if (found == true) {
        lc->release(parent);
        return yfs_client::EXIST;
    }
    extent_protocol::extentid_t id;
    extent_protocol::status status = _create(extent_protocol::T_FILE, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: _create" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = _get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: _get" << std::endl;
        lc->release(parent);
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = _put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: _put" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    lc->acquire(parent);

    printf("get lock %d in mkdir\n", parent);

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent information.
     */
    bool found = false;

    /* Lookup and set found's value */
    {
        std::string local_buf;
        std::list<dirent> dirent_list;
        _get(parent, local_buf);
        directory2list(local_buf, dirent_list);
        for (std::list<dirent>::iterator it = dirent_list.begin(); it != dirent_list.end(); ++it) {
            if (it->name.compare(name) == 0) {
                // find it!
                found = true;
                break;
            }
        }
    }

    if (found == true) {
        lc->release(parent);
        return yfs_client::EXIST;
    }
    extent_protocol::extentid_t id;
    extent_protocol::status status = _create(extent_protocol::T_DIR, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: _create" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = _get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: _get" << std::endl;
        lc->release(parent);
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = _put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: _put" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    lc->acquire(parent);

    printf("get lock %d in lookup\n", parent);

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string local_buf;
    std::list<dirent> dirent_list;
    _get(parent, local_buf);
    directory2list(local_buf, dirent_list);
    for (std::list<dirent>::iterator it = dirent_list.begin(); it != dirent_list.end(); ++it) {
        if (it->name.compare(name) == 0) {
            // find it!
            found = true;
            ino_out = it->inum;
            lc->release(parent);
            return r;
        }
    }
    found = false;
    lc->release(parent);
    return NOENT;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    
    lc->acquire(dir);

    printf("get lock %d in readdir\n", dir);

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    extent_protocol::attr attr;
    extent_protocol::status status;
    status = _getattr(dir, attr);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::readdir: _getattr" << std::endl;
        lc->release(dir);
        return RPCERR;
    }

    if (attr.type != extent_protocol::T_DIR) {
        std::cout << "Error in yfs_client::readdir: not a directory" << std::endl;
        lc->release(dir);
        return RPCERR;
    }

    status = _get(dir, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::readdir: _get" << std::endl;
        lc->release(dir);
        return RPCERR;
    }
    
    directory2list(buf, list);
    lc->release(dir);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    lc->acquire(ino);

    printf("get lock %d in read\n", ino);

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    extent_protocol::status status;
    std::string buf;

    status = _get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read: _get" << std::endl;
        lc->release(ino);
        return RPCERR;
    }
    for (off_t i = off; i < buf.size() && i - off < size; ++i) {
        data.push_back(buf[i]);
    }
    lc->release(ino);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    lc->acquire(ino);

    printf("get lock %d in write\n", ino);

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    extent_protocol::status status;
    std::string buf;

    status = _get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read: _get" << std::endl;
        lc->release(ino);
        return RPCERR;
    }

    if (off >= buf.size()) {
        // offset is beyond the end of the original file
        std::string write_buf(off + size, 0);
        size_t original_size = buf.size();
        for (size_t i = 0; i < original_size; ++i) {
            write_buf[i] = buf[i];
        }
        for (off_t i = off; i < off + size; ++i) {
            write_buf[i] = data[i - off];
        }
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::read: _put 0" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        // bytes_written = off + size - original_size;
        bytes_written = size;
        lc->release(ino);
        return OK;
    }
    if (off < buf.size() && off + size - 1 < buf.size()) {
        // write at the middle of a file
        std::string write_buf = buf;
        for (off_t i = off; i < off + size; ++i) {
            write_buf[i] = data[i - off];
        }
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::read: _put 1" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        bytes_written = size;
        lc->release(ino);
        return OK;
    }
    if (off < buf.size() && off + size - 1 >= buf.size()) {
        // this write will cause the file to grow
        std::string write_buf(off + size, 0);
        size_t original_size = buf.size();
        for (size_t i = 0; i < original_size; ++i) {
            write_buf[i] = buf[i];
        }
        for (off_t i = off; i < off + size; ++i) {
            write_buf[i] = data[i - off];
        }
        status = _put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::read: _put 2" << std::endl;
            lc->release(ino);
            return RPCERR;
        }
        bytes_written = size;
        lc->release(ino);
        return OK;
    }
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    lc->acquire(parent);

    printf("get lock %d in unlink\n", parent);

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    extent_protocol::status status;
    std::string buf;
    std::list<dirent> dirent_list;
    status = _get(parent, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::unlink: _get(parent, buf)" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    directory2list(buf, dirent_list);
    inum target;
    bool found = false;
    std::list<dirent>::iterator it;
    for (it = dirent_list.begin(); it != dirent_list.end(); ++it) {
        if (it->name.compare(name) == 0) {
            target = it->inum;
            found = true;
            break;
        }
    }
    if (!found) {
        std::cout << "Error in yfs_client::unlink: No such file" << std::endl;
        lc->release(parent);
        return NOENT;
    }

    // remove the file
    _remove(target);

    // update the parent directory content
    dirent d = *it;
    dirent_list.remove(d);
    std::string write_buf;
    list2directory(write_buf, dirent_list);
    _put(parent, write_buf);
    lc->release(parent);
    return r;
}

int yfs_client::create_symlink(inum parent, const char* link, mode_t mode, const char* name, inum& ino_out) {

    lc->acquire(parent);

    printf("get lock %d in create_symlink\n", parent);

    bool found = false;

    /* Lookup and set found's value */
    {
        std::string local_buf;
        std::list<dirent> dirent_list;
        _get(parent, local_buf);
        directory2list(local_buf, dirent_list);
        for (std::list<dirent>::iterator it = dirent_list.begin(); it != dirent_list.end(); ++it) {
            if (it->name.compare(name) == 0) {
                // find it!
                found = true;
                break;
            }
        }
    }

    if (found == true) {
        lc->release(parent);
        return yfs_client::EXIST;
    }
    extent_protocol::extentid_t id;
    extent_protocol::status status = _create(extent_protocol::T_SYMLINK, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: _create(extent_protocol::T_SYMLINK, id)" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    
    // No need to acquire a lock for ino_out here
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = _get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: _get(parent, buf)" << std::endl;
        lc->release(parent);
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = _put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: _put(parent, write_buf)" << std::endl;
        lc->release(parent);
        return RPCERR;
    }

    // write the link as the file content
    std::string file_content(link);
    status = _put(id, file_content);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: _put(id, file_content)" << std::endl;
        lc->release(parent);
        return RPCERR;
    }
    lc->release(parent);
    return OK;
}

int yfs_client::read_symlink(inum ino, std::string &link) {

    lc->acquire(ino);

    printf("get lock %d in read_symlink\n", ino);

    extent_protocol::status status;
    status = _get(ino, link);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read_symlink: _get(ino, link)" << std::endl;
        lc->release(ino);
        return RPCERR;
    }
    lc->release(ino);
    return OK;
}

extent_protocol::status yfs_client::_create(uint32_t type, extent_protocol::extentid_t &id) {
    printf("calling _create with type %d\n", type);
    extent_protocol::status status = -1;
    while (status != extent_protocol::OK) {
        status = ec->create(type, id);
    }
    printf("calling lc->acquire with type %d in _create\n", id);
    lc->acquire(id);
    printf("acquired lock %d in _create\n", id);
    _put(id, "");
    printf("releasing lock %d in _create\n", id);
    lc->release(id);
    printf("released lock %d in _create\n", id);
    return status;
}

extent_protocol::status yfs_client::_get(extent_protocol::extentid_t eid, std::string &buf) {
    printf("Calling _get with %d\n", eid);
    local_file_cache_entry* entry = cache->getCache(eid);
    if (!entry || !entry->valid) {
        printf("In yfs_client::_get: No such entry\n");
        if (!entry) {
            printf("In yfs_client::_get: No such entry (null)\n");
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            entry = new local_file_cache_entry;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            cache->addCache(eid, entry);
        }
        else {
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            entry->valid = true;
            entry->modified = false;
        }
        printf("Recover from yfs_client::_get: No such entry\n");
    }
    buf.assign(entry->content);
    return extent_protocol::OK;
}

extent_protocol::status yfs_client::_getattr(extent_protocol::extentid_t eid, extent_protocol::attr &attr) {
    printf("Calling _getattr with %d\n", eid);
    local_file_cache_entry* entry = cache->getCache(eid);
    if (!entry || !entry->valid) {
        printf("Error in yfs_client::_getattr: No such entry\n");
        if (!entry) {
            printf("Error in yfs_client::_getattr: No such entry (null)\n");
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            entry = new local_file_cache_entry;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            cache->addCache(eid, entry);
        }
        else {
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            entry->valid = true;
            entry->modified = false;
        }
        printf("Recover from error in yfs_client::_getattr: No such entry\n");
    }
    attr.atime = entry->attributes.atime;
    attr.ctime = entry->attributes.ctime;
    attr.mtime = entry->attributes.mtime;
    attr.size = entry->attributes.size;
    attr.type = entry->attributes.type;
    return extent_protocol::OK;
}

extent_protocol::status yfs_client::_put(extent_protocol::extentid_t eid, std::string buf) {
    printf("Calling _put with %d and %s\n", eid, buf.c_str());
    local_file_cache_entry* entry = cache->getCache(eid);
    if (!entry || !entry->valid) {
        printf("Error in yfs_client::_put: No such entry\n");
        if (!entry) {
            printf("Error in yfs_client::_put: No such entry (null)\n");
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            entry = new local_file_cache_entry;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            cache->addCache(eid, entry);
        }
        else {
            extent_protocol::status status = -1;
            std::string local_buf;
            extent_protocol::attr attributes;
            extent_protocol::attr_content attr_content;
            while (status != extent_protocol::OK) {
                status = ec->getattr_content(eid, attr_content);
            }
            local_buf = attr_content.content;
            attributes = attr_content.a;
            entry->content.assign(local_buf);
            entry->attributes.atime = attributes.atime;
            entry->attributes.ctime = attributes.ctime;
            entry->attributes.mtime = attributes.mtime;
            entry->attributes.size = attributes.size;
            entry->attributes.type = attributes.type;
            entry->valid = true;
            entry->modified = false;
        }
        printf("Recover from error in yfs_client::_put: No such entry\n");
    }
    entry->content.assign(buf);
    entry->modified = true;
    entry->attributes.atime = time(NULL);
    entry->attributes.ctime = time(NULL);
    entry->attributes.mtime = time(NULL);
    entry->attributes.size = buf.size();
    return extent_protocol::OK;
}

extent_protocol::status yfs_client::_remove(extent_protocol::extentid_t eid) {
    printf("Calling _remove with %d\n", eid);
    extent_protocol::status status = -1;
    while (status != extent_protocol::OK) {
        status = ec->remove(eid);
    }
    if (cache->getCache(eid) != NULL) {
        cache->deleteCache(eid);
    }
    return status;
}
