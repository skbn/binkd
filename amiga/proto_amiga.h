/*
 * proto_amiga.h -- Amiga non-blocking BinkP protocol implementation
 *
 * proto_amiga.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _PROTO_AMIGA_H
#define _PROTO_AMIGA_H

#include "protoco2.h"
#include "readcfg.h"

/* amiga_proto_step() return codes */
#define APROTO_RUNNING 0
#define APROTO_DONE_OK 1
#define APROTO_DONE_ERR 2

/* amiga_proto_open -- Initialise a session and send the BinkP banner */
int amiga_proto_open(STATE *state, SOCKET fd, FTN_NODE *to, FTN_ADDR *fa, const char *host, const char *port, const char *dst_ip, BINKD_CONFIG *config);

/* amiga_proto_step-- run one recv / send iteration of the BinkP loop */
int amiga_proto_step(STATE *state, int readable, int writable, BINKD_CONFIG *config);

/* amiga_proto_close -- flush remaining I/O and release STATE resources */
void amiga_proto_close(STATE *state, BINKD_CONFIG *config, int ok);

#endif /* _PROTO_AMIGA_H */
