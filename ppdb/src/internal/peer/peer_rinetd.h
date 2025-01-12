#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

extern const poly_cmd_option_t rinetd_options[];

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t rinetd_cmd_handler(int argc, char** argv);

#endif // PEER_RINETD_H 