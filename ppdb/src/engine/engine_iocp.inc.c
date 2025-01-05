// This file should be removed from engine layer
// The implementation should be moved to base layer (e.g. ppdb/src/base/base_iocp.c)
// with the same functionality but using base layer interfaces

// Remove entire file contents and add a compatibility header:

#ifdef _WIN32

// Forward declarations to maintain backward compatibility
#include "../base/base_iocp.h"

// Compatibility type definitions
typedef ppdb_base_iocp_loop_t ppdb_engine_iocp_loop_t;
typedef ppdb_base_iocp_handle_t ppdb_engine_iocp_handle_t;

// Forward compatibility functions
#define ppdb_engine_iocp_loop_create ppdb_base_iocp_loop_create
#define ppdb_engine_iocp_loop_destroy ppdb_base_iocp_loop_destroy
#define ppdb_engine_iocp_loop_run ppdb_base_iocp_loop_run
#define ppdb_engine_iocp_handle_create ppdb_base_iocp_handle_create
#define ppdb_engine_iocp_handle_destroy ppdb_base_iocp_handle_destroy
#define ppdb_engine_iocp_read ppdb_base_iocp_read
#define ppdb_engine_iocp_write ppdb_base_iocp_write

#endif // _WIN32
