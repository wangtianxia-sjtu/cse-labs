// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"

class extent_protocol {
 public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus { OK, RPCERR, NOENT, IOERR };
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    remove,
    create,
    getattr_content
  };

  enum types {
    T_DIR = 1,
    T_FILE,
    T_SYMLINK
  };

  struct attr {
    uint32_t type;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;
  };

  struct attr_content {
    attr a;
    std::string content;
  };
};

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.type;
  u >> a.atime;
  u >> a.mtime;
  u >> a.ctime;
  u >> a.size;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.type;
  m << a.atime;
  m << a.mtime;
  m << a.ctime;
  m << a.size;
  return m;
}

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr_content &a) {
  u >> a.a;
  u >> a.content;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr_content a) {
  m << a.a;
  m << a.content;
  return m;
}

#endif 
