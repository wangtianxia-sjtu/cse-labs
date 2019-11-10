// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&lock_server_mutex, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  pthread_mutex_lock(&lock_server_mutex);
  std::map<lock_protocol::lockid_t, lock_status>::iterator it = locks.find(lid);
  if (it == locks.end()) {
    locks[lid] = LOCKED;
    pthread_cond_t* conditional_variable = new pthread_cond_t;
    pthread_cond_init(conditional_variable, NULL);
    conditional_variables[lid] = conditional_variable;
    pthread_mutex_unlock(&lock_server_mutex);
    return ret;
  }
  else {
    if (it->second == FREE) {
      it->second = LOCKED;
      pthread_mutex_unlock(&lock_server_mutex);
      return ret;
    }
    else {
      // iterator may be invalidated here
      while (locks[lid] == LOCKED) {
        pthread_cond_wait(conditional_variables[lid], &lock_server_mutex);
      }
      locks[lid] = LOCKED;
      pthread_mutex_unlock(&lock_server_mutex);
      return ret;
    }
  }
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  pthread_mutex_lock(&lock_server_mutex);
  locks[lid] = FREE;
  pthread_cond_signal(conditional_variables[lid]);
  pthread_mutex_unlock(&lock_server_mutex);
  return ret;
}
