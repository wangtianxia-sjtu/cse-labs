#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <queue>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {

  enum lock_status { FREE, LOCKED, REVOKING };

 private:
  int nacquire;
  pthread_mutex_t lock_server_mutex;
  std::map<lock_protocol::lockid_t, lock_status> locks;
  std::map<lock_protocol::lockid_t, std::queue<std::string>> waiting_set;
  std::map<lock_protocol::lockid_t, std::string> lock_owners;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
