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

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
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
    printf("isfile: %lld is a dir\n", inum);
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
    return ! isfile(inum);
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

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

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

    return r;
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

    return r;
}

