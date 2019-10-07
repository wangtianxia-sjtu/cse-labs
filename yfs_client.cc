// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


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
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a directory", inum);
        return true;
    }
    printf("isdir: %lld is not a directory", inum);
    return false;
}

bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("issymlink: %lld is a symlink", inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
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

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::status status;
    std::string buf;
    status = ec->get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::setattr: ec->get(ino, buf)" << std::endl;
        return RPCERR;
    }
    size_t original_size = buf.size();
    if (size == 0) {
        std::string write_buf;
        status = ec->put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: ec->put(ino, write_buf) 0" << std::endl;
            return RPCERR;
        }
        return OK;
    }
    if (size > original_size) {
        // try to set size larger than the original size, add paddings
        std::string write_buf(size, 0);
        for (size_t i = 0; i < original_size; ++i) {
            write_buf[i] = buf[i];
        }
        status = ec->put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: ec->put(ino, write_buf) 1" << std::endl;
            return RPCERR;
        }
        return OK;
    }
    if (size < original_size) {
        // try to set a smaller size
        std::string write_buf(size, 0);
        for (size_t i = 0; i < size; ++i) {
            write_buf[i] = buf[i];
        }
        status = ec->put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::setattr: ec->put(ino, write_buf) 2" << std::endl;
            return RPCERR;
        }
        return OK;
    }
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent information.
     */
    bool found = false;
    inum i;
    lookup(parent, name, found, i);
    if (found == true)
        return yfs_client::EXIST;
    extent_protocol::extentid_t id;
    extent_protocol::status status = ec->create(extent_protocol::T_FILE, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: ec->create" << std::endl;
        return RPCERR;
    }
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = ec->get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: ec->get" << std::endl;
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = ec->put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create: ec->put" << std::endl;
        return RPCERR;
    }
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent information.
     */
    bool found = false;
    inum i;
    lookup(parent, name, found, i);
    if (found == true)
        return yfs_client::EXIST;
    extent_protocol::extentid_t id;
    extent_protocol::status status = ec->create(extent_protocol::T_DIR, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: ec->create" << std::endl;
        return RPCERR;
    }
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = ec->get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: ec->get" << std::endl;
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = ec->put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::mkdir: ec->put" << std::endl;
        return RPCERR;
    }
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string local_buf;
    std::list<dirent> dirent_list;
    ec->get(parent, local_buf);
    directory2list(local_buf, dirent_list);
    for (std::list<dirent>::iterator it = dirent_list.begin(); it != dirent_list.end(); ++it) {
        if (it->name.compare(name) == 0) {
            // find it!
            found = true;
            ino_out = it->inum;
            return r;
        }
    }
    found = false;
    return NOENT;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    extent_protocol::attr attr;
    extent_protocol::status status;
    status = ec->getattr(dir, attr);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::readdir: ec->getattr" << std::endl;
        return RPCERR;
    }

    if (attr.type != extent_protocol::T_DIR) {
        std::cout << "Error in yfs_client::readdir: not a directory" << std::endl;
        return RPCERR;
    }

    status = ec->get(dir, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::readdir: ec->get" << std::endl;
        return RPCERR;
    }
    
    directory2list(buf, list);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    extent_protocol::status status;
    std::string buf;

    status = ec->get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read: ec->get" << std::endl;
        return RPCERR;
    }
    for (off_t i = off; i < buf.size() && i - off < size; ++i) {
        data.push_back(buf[i]);
    }
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    extent_protocol::status status;
    std::string buf;

    status = ec->get(ino, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read: ec->get" << std::endl;
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
        status = ec->put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::read: ec->put 0" << std::endl;
            return RPCERR;
        }
        bytes_written = off + size - original_size;
        return OK;
    }
    if (off < buf.size() && off + size - 1 < buf.size()) {
        // write at the middle of a file
        std::string write_buf = buf;
        for (off_t i = off; i < off + size; ++i) {
            write_buf[i] = data[i - off];
        }
        status = ec->put(ino, write_buf);
        if (status != extent_protocol::OK) {
            std::cout << "Error in yfs_client::read: ec->put 1" << std::endl;
            return RPCERR;
        }
        bytes_written = size;
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
        bytes_written = size;
        return OK;
    }
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    extent_protocol::status status;
    std::string buf;
    std::list<dirent> dirent_list;
    status = ec->get(parent, buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::unlink: ec->get(parent, buf)" << std::endl;
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
        return NOENT;
    }

    // remove the file
    ec->remove(target);

    // update the parent directory content
    dirent d = *it;
    dirent_list.remove(d);
    std::string write_buf;
    list2directory(write_buf, dirent_list);
    ec->put(parent, write_buf);
    return r;
}

int yfs_client::create_symlink(inum parent, const char* link, mode_t mode, const char* name, inum& ino_out) {

    bool found = false;
    inum i;
    lookup(parent, name, found, i);
    if (found == true)
        return yfs_client::EXIST;
    extent_protocol::extentid_t id;
    extent_protocol::status status = ec->create(extent_protocol::T_SYMLINK, id);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: ec->create(extent_protocol::T_SYMLINK, id)" << std::endl;
        return RPCERR;
    }
    ino_out = id;

    // modify the parent information
    std::string buf;
    std::list<dirent> dirent_list;
    std::string write_buf;
    status = ec->get(parent, buf);

    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: ec->get(parent, buf)" << std::endl;
        return RPCERR;
    }

    directory2list(buf, dirent_list);
    dirent d;
    d.inum = id;
    d.name = name;
    dirent_list.push_back(d);
    list2directory(write_buf, dirent_list);
    status = ec->put(parent, write_buf);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: ec->put(parent, write_buf)" << std::endl;
        return RPCERR;
    }

    // write the link as the file content
    std::string file_content(link);
    status = ec->put(id, file_content);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::create_symlink: ec->put(id, file_content)" << std::endl;
        return RPCERR;
    }
    return OK;
}

int yfs_client::read_symlink(inum ino, std::string &link) {

    extent_protocol::status status;
    status = ec->get(ino, link);
    if (status != extent_protocol::OK) {
        std::cout << "Error in yfs_client::read_symlink: ec->get(ino, link)" << std::endl;
        return RPCERR;
    }
    return OK;
}
