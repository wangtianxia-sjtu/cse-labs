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

class local_cache;

class yfs_client {
  extent_client *ec;
  lock_client *lc;
  local_cache *cache;
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
  extent_protocol::status _create(uint32_t type, extent_protocol::extentid_t &id);
  extent_protocol::status _get(extent_protocol::extentid_t eid, std::string &buf);
  extent_protocol::status _getattr(extent_protocol::extentid_t eid, extent_protocol::attr &attr);
  extent_protocol::status _put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status _remove(extent_protocol::extentid_t eid);

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
    bool modified;
    extent_protocol::attr attributes;

    local_file_cache_entry() {
      valid = true;
      modified = false;
    }
};

class local_cache {
  private:
    std::map<yfs_client::inum, local_file_cache_entry*> cache_map;
    pthread_mutex_t lock;

  public:
    local_cache() {
      pthread_mutex_init(&lock, NULL);
    }

    local_file_cache_entry* getCache(yfs_client::inum inode) {
      pthread_mutex_lock(&lock);
      if (cache_map.find(inode) == cache_map.end()) {
        return NULL;
      }
      local_file_cache_entry* result = cache_map[inode];
      pthread_mutex_unlock(&lock);
      return result;
    }

    void addCache(yfs_client::inum inode, local_file_cache_entry* entry) {
      pthread_mutex_lock(&lock);
      if (cache_map.find(inode) != cache_map.end()) {
        return;
      }
      cache_map[inode] = entry;
      pthread_mutex_unlock(&lock);
    }

    void deleteCache(yfs_client::inum inode) {
      pthread_mutex_lock(&lock);
      std::map<yfs_client::inum, local_file_cache_entry*>::iterator it;
      if ((it = cache_map.find(inode)) == cache_map.end())
        return;
      else
        cache_map.erase(it);
      pthread_mutex_unlock(&lock);
    }
};

#endif 
