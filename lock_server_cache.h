#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <queue>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_metadata;

class lock_server_cache {

 public:
  enum lock_status { FREE, LOCKED };

 private:
  int nacquire;
  pthread_mutex_t lock_server_mutex;
  std::map<lock_protocol::lockid_t, lock_metadata*> locks;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

class lock_metadata {
  public:
    int stat; // enum lock_status { FREE, LOCKED }
    bool revoking;
    std::string owner;
    unsigned long long version;
    lock_metadata() {
      stat = lock_server_cache::FREE;
      revoking = false;
      owner = "";
      version = 0ULL;
    }
};

#endif
