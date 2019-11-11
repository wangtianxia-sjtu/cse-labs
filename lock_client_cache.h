// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "extent_client.h"
#include "lang/verify.h"
#include <queue>
#include <map>


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_metadata_client;

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  pthread_mutex_t lock_client_mutex;
  std::map<lock_protocol::lockid_t, lock_metadata_client*> locks;
  extent_client* ec;

 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  lock_client_cache(std::string xdst, extent_client* _ec, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
  enum lock_status_client { FREE, NONE, LOCKED };
};

class lock_metadata_client {
  public:
    std::queue<pthread_cond_t*> waiting_set;
    int stat; // enum lock_status_client { FREE, NONE, LOCKED }
    bool acquiring;
    lock_metadata_client() {
      stat = lock_client_cache::NONE;
      acquiring = false;
    }
};


#endif
