/*
 * evloop_int.h -- internal types shared by evloop.c, sock.c, session.c
 *
 * evloop_int.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef AMIGA_EVLOOP_INT_H
#define AMIGA_EVLOOP_INT_H

#include "protoco2.h"
#include "amiga/bsdsock.h"
#include "ftnnode.h"
#include "readcfg.h"

/* Session lifecycle */
typedef enum
{
    SESS_FREE = 0,       /* slot available */
    SESS_CONNECTING = 1, /* waiting for connect() */
    SESS_RUNNING = 2     /* BinkP session active  */
} sess_phase_t;

/* Per-session state */
typedef struct
{
    sess_phase_t phase;
    SOCKET fd;
    STATE state;
    int inbound; /* 1=accepted, 0=outbound */

    FTN_NODE *node;
    struct addrinfo *ai_head; /* full getaddrinfo list  */
    struct addrinfo *ai_cur;  /* candidate being tried  */
    time_t conn_start;

    char host[BINKD_FQDNLEN + 1];
    char port[MAXPORTSTRLEN + 1];
    char ip[BINKD_FQDNLEN + 1];

    time_t last_io;
} sess_t;

/* Globals defined in evloop.c */
extern sess_t *sessions;
extern int max_sessions;

/* Defined in server.c and client.c respectively */
extern int n_servers;
extern int n_clients;

/* sock.c */
void set_nonblock(SOCKET fd);
int open_listen_sockets(BINKD_CONFIG *config);
void close_listen_sockets();

/* session.c */
int sess_alloc();
void sess_free(int idx);
void do_accept(SOCKET lfd, BINKD_CONFIG *config);
int start_connect(sess_t *s, BINKD_CONFIG *config);
void check_connect(int idx, BINKD_CONFIG *config);
int try_outbound(BINKD_CONFIG *config);
void do_session_step(int idx, int rd, int wr, BINKD_CONFIG *config);

#endif /* AMIGA_EVLOOP_INT_H */
