// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&lock_server_mutex, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  int r;
  pthread_mutex_lock(&lock_server_mutex);
  if (locks.find(lid) == locks.end()) {
    // No one has created this lock
    locks[lid] = new lock_metadata();
  }
  lock_metadata* this_lock = locks[lid];
  if (this_lock->stat == lock_server_cache::FREE) {
    this_lock->owner = id;
    this_lock->stat = lock_server_cache::LOCKED;
    this_lock->version++;
    pthread_mutex_unlock(&lock_server_mutex);
    return lock_protocol::OK;
  }
  else {
    if (this_lock->owner == id) {
      this_lock->version++;
      pthread_mutex_unlock(&lock_server_mutex);
      return lock_protocol::OK;
    }
    else {
      if (this_lock->revoking) {
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
      this_lock->revoking = true;
      std::string owner = this_lock->owner;
      unsigned long long old_version = this_lock->version;
      pthread_mutex_unlock(&lock_server_mutex);
      int result = handle(owner).safebind()->call(rlock_protocol::revoke, lid, r);
      pthread_mutex_lock(&lock_server_mutex);
      if (result < 0) {
        this_lock = locks[lid];
        this_lock->revoking = false;
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
      if (result == lock_protocol::REVOKE_SUCCEEDED) {
        this_lock = locks[lid];
        unsigned long long new_version = this_lock->version;
        this_lock->revoking = false;
        if (new_version == old_version) {
          this_lock->owner = id;
          this_lock->version++;
          this_lock->stat = lock_server_cache::LOCKED;
          pthread_mutex_unlock(&lock_server_mutex);
          return lock_protocol::OK;
        }
        if (this_lock->stat == lock_server_cache::FREE) {
          this_lock->owner = id;
          this_lock->version++;
          this_lock->stat = lock_server_cache::LOCKED;
          pthread_mutex_unlock(&lock_server_mutex);
          return lock_protocol::OK;
        }
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
      if (result == lock_protocol::REVOKE_FAILED) {
        this_lock = locks[lid];
        this_lock->revoking = false;
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
      if (result == lock_protocol::NO_LOCK_ACQUIRING) {
        this_lock = locks[lid];
        this_lock->revoking = false;
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
      if (result == lock_protocol::NO_LOCK_NOT_ACQUIRING) {
        this_lock = locks[lid];
        this_lock->revoking = false;
        unsigned long long new_version = this_lock->version;
        if (new_version == old_version) {
          // No one has modified this lock during RPC
          this_lock->owner = id;
          this_lock->version++;
          this_lock->stat = lock_server_cache::LOCKED;
          pthread_mutex_unlock(&lock_server_mutex);
          return lock_protocol::OK;
        }
        if (this_lock->stat == lock_server_cache::FREE) {
          this_lock->owner = id;
          this_lock->version++;
          this_lock->stat = lock_server_cache::LOCKED;
          pthread_mutex_unlock(&lock_server_mutex);
          return lock_protocol::OK;
        }
        pthread_mutex_unlock(&lock_server_mutex);
        return lock_protocol::RETRY;
      }
    }
  }
  pthread_mutex_unlock(&lock_server_mutex);
  return lock_protocol::RETRY;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  pthread_mutex_lock(&lock_server_mutex);
  if (locks.find(lid) == locks.end()) {
    // try to release a lock that does not exist
    pthread_mutex_unlock(&lock_server_mutex);
    return lock_protocol::NON_SUITABLE_RELEASE;
  }
  lock_metadata* this_lock = locks[lid];
  if (this_lock->stat == lock_server_cache::FREE) {
    // try to release a non-held lock
    pthread_mutex_unlock(&lock_server_mutex);
    return lock_protocol::NON_SUITABLE_RELEASE;
  }
  if (this_lock->owner != id) {
    // try to release a lock held by others
    pthread_mutex_unlock(&lock_server_mutex);
    return lock_protocol::NON_SUITABLE_RELEASE;
  }
  this_lock->owner = "";
  this_lock->version++;
  this_lock->stat = lock_server_cache::FREE;
  pthread_mutex_unlock(&lock_server_mutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

