// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  pthread_mutex_init(&lock_client_mutex, NULL);
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  pthread_mutex_lock(&lock_client_mutex);
  if (locks.find(lid) == locks.end()) {
    locks[lid] = new lock_metadata_client();
  }
  lock_metadata_client* this_lock = locks[lid];
  if (this_lock->stat == lock_client_cache::NONE) {
    if (this_lock->acquiring == true) {
      // someone is acquiring the lock!
      pthread_cond_t* cv = new pthread_cond_t;
      pthread_cond_init(cv, NULL);
      this_lock->waiting_set.push(cv);
      pthread_cond_wait(cv, &lock_client_mutex);
      delete cv;
      this_lock->waiting_set.pop();
      pthread_mutex_unlock(&lock_client_mutex);
      return lock_protocol::OK;
    }
    // the client know nothing about the lock
    this_lock->acquiring = true;
    int result = -1;
    pthread_mutex_unlock(&lock_client_mutex);
    while (result != lock_protocol::OK) {
      result = cl->call(lock_protocol::acquire, lid, id, r);
      sleep(2);
    }
    pthread_mutex_lock(&lock_client_mutex);
    this_lock->stat = lock_client_cache::LOCKED;
    this_lock->acquiring = false;
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }

  if (this_lock->stat == lock_client_cache::LOCKED) {
    pthread_cond_t* cv = new pthread_cond_t;
    pthread_cond_init(cv, NULL);
    this_lock->waiting_set.push(cv);
    pthread_cond_wait(cv, &lock_client_mutex);
    delete cv;
    this_lock->waiting_set.pop();
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }

  if (this_lock->stat == lock_client_cache::FREE) {
    this_lock->stat = lock_client_cache::LOCKED;
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }

  pthread_mutex_unlock(&lock_client_mutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&lock_client_mutex);
  if (locks.find(lid) == locks.end()) {
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }
  lock_metadata_client* this_lock = locks[lid];
  if (this_lock->waiting_set.empty()) {
    this_lock->stat = lock_client_cache::FREE;
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }
  else {
    pthread_cond_signal(this_lock->waiting_set.front());
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::OK;
  }
  pthread_mutex_unlock(&lock_client_mutex);
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  pthread_mutex_lock(&lock_client_mutex);
  if (locks.find(lid) == locks.end()) {
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::REVOKE_SUCCEEDED;
  }
  lock_metadata_client* this_lock = locks[lid];
  if (this_lock->stat == lock_client_cache::FREE) {
    this_lock->stat = lock_client_cache::NONE;
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::REVOKE_SUCCEEDED;
  }
  if (this_lock->stat == lock_client_cache::LOCKED) {
    pthread_mutex_unlock(&lock_client_mutex);
    return lock_protocol::REVOKE_FAILED;
  }
  if (this_lock->stat == lock_client_cache::NONE) {
    if (this_lock->acquiring) {
      pthread_mutex_unlock(&lock_client_mutex);
      return lock_protocol::NO_LOCK_ACQUIRING;
    }
    else {
      pthread_mutex_unlock(&lock_client_mutex);
      return lock_protocol::NO_LOCK_NOT_ACQUIRING;
    }
  }
  pthread_mutex_unlock(&lock_client_mutex);
  return lock_protocol::REVOKE_SUCCEEDED;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  return ret;
}



