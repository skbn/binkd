/*
 * evloop.h -- non-blocking event loop for AmigaOS 3
 *
 * evloop.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _AMIGA_EVLOOP_H
#define _AMIGA_EVLOOP_H

#ifdef AMIGA

#include "readcfg.h"
#include "protoco2.h" /* STATE */

/* amiga_proto_step() return codes — also used by protocol.c */
#define APROTO_RUNNING 0  /* session alive, call again */
#define APROTO_DONE_OK 1  /* session finished, success */
#define APROTO_DONE_ERR 2 /* session failed */

/*
 * amiga_evloop_run -- entry point replacing servmgr() + clientmgr()
 *
 * Opens listen sockets (when server_flag), then runs a WaitSelect()
 * loop that multiplexes sessions dynamically based on config->max_servers
 * and config->max_clients. Minimum 2 sessions are always allocated
 * Returns only when binkd_exit != 0
 */
void amiga_evloop_run(BINKD_CONFIG *config, int server_flag, int client_flag);

#endif /* AMIGA */
#endif /* _AMIGA_EVLOOP_H */
