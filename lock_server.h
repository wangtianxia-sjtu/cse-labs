// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

  enum lock_status { FREE, LOCKED };

 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t, lock_status> locks;
  std::map<lock_protocol::lockid_t, pthread_cond_t*> conditional_variables;
  pthread_mutex_t lock_server_mutex;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







