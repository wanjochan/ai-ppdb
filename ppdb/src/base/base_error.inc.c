#ifndef PPDB_BASE_ERROR_INC_C
#define PPDB_BASE_ERROR_INC_C

const char* ppdb_error_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "Success";
        case PPDB_ERR_NULL_POINTER:
            return "Null pointer";
        case PPDB_ERR_INVALID_ARGUMENT:
            return "Invalid argument";
        case PPDB_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_ERR_NOT_IMPLEMENTED:
            return "Not implemented";
        case PPDB_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case PPDB_ERR_TIMEOUT:
            return "Timeout";
        case PPDB_ERR_BUSY:
            return "Busy";
        case PPDB_ERR_FULL:
            return "Full";
        case PPDB_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_ERR_EXISTS:
            return "Already exists";
        default:
            return "Unknown error";
    }
}

#endif // PPDB_BASE_ERROR_INC_C 