#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <map>
#include <pthread.h>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  int directory2list(std::string &data, std::list<dirent> &result);
  int list2directory(std::string &data, std::list<dirent> &result);

 public:
  yfs_client(std::string, std::string);
  yfs_client();

  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  
  /** you may need to add symbolic link related methods here.*/
  int create_symlink(inum parent, const char* link, mode_t, const char* name, inum& ino_out);
  int read_symlink(inum, std::string &);
};

class local_file_cache_entry {
  public:
    std::string content;
    bool valid;
    bool isFile;
    yfs_client::fileinfo fileinfo;
};

class local_cache {
  private:
    std::map<yfs_client::inum, local_file_cache_entry> cache_map;
    pthread_mutex_t lock;

  public:
    local_cache() {
      pthread_mutex_init(&lock, NULL);
    }

    // local_file_cache_entry getCache TODO
};

#endif 
