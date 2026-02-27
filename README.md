
# CarrotDB🥕

In-memory key value database implemented in modern C++ (C++23) built from scratch to understand distributed systems and Redis internals.

## Features

### Supported Commands

**Strings**
- `SET`, `GET`, `INCR`

**Lists**
- `RPUSH`, `LPUSH`, `LRANGE`, `LLEN`, `LPOP`, `BLPOP`

**Streams**
- `XADD`, `XRANGE`, `XREAD`

**Sorted Sets**
- `ZADD`, `ZRANGE`, `ZRANK`, `ZCARD`, `ZSCORE`, `ZREM`

**Geospatial**
- `GEOADD`, `GEOPOS`, `GEODIST`, `GEOSEARCH`

**Server**
- `PING`, `ECHO`, `KEYS`, `TYPE`, `INFO`

**Transactions**
- `MULTI`, `EXEC`, `DISCARD`

**Pub/Sub**
- `SUBSCRIBE`, `PUBLISH`

**ACL (Access Control)**
- `ACL WHOAMI`, `ACL GETUSER`, `ACL SETUSER`, `AUTH`

**Replication**
- `INFO`, `REPLCONF`, `PSYNC`, `WAIT`

---

## Important Notice

> **Note**: This project is **NOT production ready**. It is built for learning purposes to understand how Redis works internally. The project is under active development and may contain bugs, incomplete features, and performance limitations. Use at your own risk.

---

## Architecture

### Event-Driven I/O
The server uses a reactor pattern with Linux `epoll` for non-blocking I/O, enabling high-performance concurrent client handling.

### Protocol
Implements RESP (Redis Serialization Protocol) for client-server communication.

### Replication
Supports master-slave replication with `PSYNC` for efficient partial resynchronization.

### Persistence
RDB format parsing for loading snapshots on startup.

### Security
ACL-based authentication system with user management.

### Data Structures
- **Lists**: Custom listpack implementation
- **Sorted Sets**: Skiplist-based implementation
- **Strings**: Radix tree for efficient storage
- **Geospatial**: Geohash for location encoding
- **Streams**: Internal stream data structure

---

## File Structure

```
src/
├── main.cpp                      # Entry point
├── server.cpp/h                  # Core server implementation
├── server-config.cpp             # Command-line argument parsing
├── eventloop.cpp/h               # Epoll-based event loop
├── connection.cpp/h              # TCP connection handling
├── acceptor.cpp/h                # Server socket acceptor
├── networking.cpp/h              # Network utilities
├── io.cpp/h                      # File I/O utilities
├── db/
│   ├── db.cpp/h                   # Database core & command execution
│   ├── XADD/xadd_db.cpp           # Stream XADD command
│   ├── XRANGE/xrange_db.cpp       # Stream XRANGE command
│   ├── LLEN/llen_db.cpp          # List LLEN command
│   ├── LRANGE/lrange_db.cpp      # List LRANGE command
│   ├── INCR/incr_db.cpp          # String INCR command
│   ├── KEYS/keys_db.cpp          # KEYS command
│   ├── ZADD/zadd_db.cpp          # Sorted set ZADD command
│   ├── ZSCORE/zscore_db.cpp      # Sorted set ZSCORE command
│   ├── ZREM/zrem_db.cpp          # Sorted set ZREM command
│   ├── GEOADD/geoadd_db.cpp      # Geospatial GEOADD command
│   ├── GEODIST/geodist_db.cpp    # Geospatial GEODIST command
│   ├── GEOSEARCH/geosearch_db.cpp # Geospatial GEOSEARCH command
│   ├── EXEC/exec_db.cpp          # Transaction EXEC command
│   ├── DISCARD/discard_db.cpp    # Transaction DISCARD command
│   └── GET_CONFIG/get_config_db.cpp # GETCONFIG command
├── parser/
│   ├── parser.cpp/h              # RESP protocol parser
│   ├── command-parser.cpp/h      # Command parsing
│   └── command.h                 # Command type definitions
├── models/
│   ├── client.h                  # Client state management
│   ├── server-model.h            # Server configuration model
│   ├── redisObject.h             # Redis object representation
│   ├── user.h                    # User model for ACL
│   ├── resp.h                    # RESP encoding/decoding
│   └── IEventLoop.h              # Event loop interface
├── replication.cpp/h              # Master-slave replication
├── master-connection.cpp/h        # Slave connection to master
├── slave-replication.cpp/h        # Slave replication logic
├── rdb.cpp/h                      # RDB persistence
├── acl.cpp/h                      # Access control lists
├── sortedset.cpp/h                # Sorted set (skiplist) implementation
├── geohash.cpp/h                  # Geospatial hashing
├── listpack.cpp/h                 # Listpack implementation
├── radix-tree.cpp/h               # Radix tree for strings
└── stream.cpp/h                   # Stream data structure
```

---

## Building

### Requirements
- CMake 3.13+
- C++23 compatible compiler
- Linux (epoll-based event loop)

### Dependencies
- ASIO (networking)
- OpenSSL (crypto)
- pthreads

Dependencies are managed via vcpkg.

### Build Commands

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make
```

The executable will be created at `build/redis`.

---

## Running

### Basic Usage

```bash
# Start with default port (6379)
./redis

# Specify custom port
./redis --port 6380
```

### Persistence

```bash
# Load RDB file on startup
./redis --dir /path/to/directory --dbfilename dump.rdb
```

### Replication

```bash
# Run as a replica (slave)
./redis --replicaof "127.0.0.1 6379"
```

### Supported Flags
- `--port <number>` - Set server port (default: 6379)
- `--replicaof <host> <port>` - Configure as replica of master
- `--dir <path>` - Directory for RDB file
- `--dbfilename <name>` - RDB filename

---

## License

MIT License
