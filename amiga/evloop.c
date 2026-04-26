/*
 * evloop.c -- non-blocking event loop for AmigaOS 3
 *
 * evloop.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

/* Suppress clib bsdsocket prototypes before any socket header */
#ifndef CLIB_BSDSOCKET_PROTOS_H
#define CLIB_BSDSOCKET_PROTOS_H
#endif

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sys.h"
#include "readcfg.h"
#include "common.h"
#include "tools.h"
#include "protocol.h"
#include "sem.h"
#include "server.h"
#include "amiga/bsdsock.h"
#include "amiga/evloop.h"
#include "amiga/evloop_int.h"
#include "amiga/proto_amiga.h"

/* Externals */
extern SOCKET sockfd[MAX_LISTENSOCK];
extern int sockfd_used;
extern int binkd_exit;
extern int server_flag, client_flag;

/* Session table (shared with sock.c and session.c) */
sess_t *sessions = NULL;
int max_sessions = 0;

/*
 * calc_max_sessions -- Compute session slot count from config + flags
 * Shared by init and config-reload paths
 */
static int calc_max_sessions(BINKD_CONFIG *config, int srv_flag, int cli_flag)
{
    int servers = config->max_servers;
    int clients = config->max_clients;
    int total;

    if (servers == 0 && clients == 0)
    {
        Log(5, "DEBUG: Using default 2 slots (no config found)");
        return 2;
    }

    Log(5, "DEBUG: Raw values: servers=%d, clients=%d", servers, clients);

    if (srv_flag && servers < 1)
        servers = 1;

    if (cli_flag && clients < 1)
        clients = 1;

    total = servers + clients;

    if (total < 2)
        total = 2;

    Log(5, "DEBUG: Calculated max_sessions=%d", total);

    return total;
}

/* init_session_table -- Allocate and zero-initialise the session array */
static int init_session_table(int slots)
{
    int i;

    sessions = calloc(slots, sizeof(sess_t));

    if (!sessions)
    {
        Log(1, "Failed to allocate session table");
        return 0;
    }

    for (i = 0; i < slots; i++)
    {
        memset(&sessions[i], 0, sizeof(sess_t));
        memset(&sessions[i].state, 0, sizeof(STATE));
        sessions[i].fd = INVALID_SOCKET;
        sessions[i].phase = SESS_FREE;
    }

    return 1;
}

/*
 * build_fdsets -- Populate r/w fd_sets from listen sockets and sessions
 * Returns the highest fd seen (maxfd)
 */
static int build_fdsets(fd_set *r, fd_set *w)
{
    int i, maxfd = 0;

    FD_ZERO(r);
    FD_ZERO(w);

    /* server side: listen sockets */
    for (i = 0; i < sockfd_used; i++)
    {
        if (sockfd[i] != INVALID_SOCKET)
        {
            FD_SET(sockfd[i], r);

            if ((int)sockfd[i] > maxfd)
                maxfd = (int)sockfd[i];
        }
    }

    /* client + server sessions */
    for (i = 0; i < max_sessions; i++)
    {
        sess_t *s = &sessions[i];

        if (s->phase == SESS_FREE || s->fd == INVALID_SOCKET)
            continue;

        if ((int)s->fd > maxfd)
            maxfd = (int)s->fd;

        if (s->phase == SESS_CONNECTING)
        {
            /* client: waiting for non-blocking connect() */
            FD_SET(s->fd, w);
        }
        else
        {
            /* Server or established client session */
            FD_SET(s->fd, r);

            if (s->state.msgs || s->state.oleft || s->state.send_eof || (s->state.out.f && !s->state.off_req_sent && !s->state.waiting_for_GOT))
                FD_SET(s->fd, w);
        }
    }

    Log(5, "DEBUG: Sessions processed, maxfd=%d", maxfd);
    return maxfd;
}

/*
 * handle_server_accept -- Accept new inbound connections on all listen fds
 * Returns 0 normally, -1 if binkd_exit was set during accept
 */
static int handle_server_accept(fd_set *r, BINKD_CONFIG *config)
{
    int i;

    Log(5, "DEBUG: Before accept loop");

    for (i = 0; i < sockfd_used; i++)
    {
        if (FD_ISSET(sockfd[i], r))
            do_accept(sockfd[i], config);

        if (binkd_exit)
        {
            Log(5, "DEBUG: binkd_exit during accept loop");
            return -1;
        }
    }

    Log(5, "DEBUG: After accept loop");

    return 0;
}

/*
 * advance_sessions -- Step every active session (server + client)
 * Returns the number of non-free sessions processed
 */
static int advance_sessions(fd_set *r, fd_set *w, BINKD_CONFIG *config)
{
    int i, active = 0;

    Log(5, "DEBUG: Before advance sessions");

    for (i = 0; i < max_sessions; i++)
    {
        sess_t *s = &sessions[i];

        Log(5, "DEBUG: Session %d, phase=%d, fd=%d", i, s->phase, (int)s->fd);

        if (s->phase == SESS_FREE)
            continue;

        active++;

        if (s->phase == SESS_CONNECTING)
        {
            /* client: Complete the non-blocking connect */
            if (FD_ISSET(s->fd, w))
                check_connect(i, config);
        }
        else
        {
            int rd = FD_ISSET(s->fd, r);
            int wr = FD_ISSET(s->fd, w);

            /* Always step: protocol must advance internal state even
             * when WaitSelect reports no activity (e.g. second batch
             * EOB send, TCP FIN detection after remote closes) */
            do_session_step(i, rd, wr, config);
        }

        if (binkd_exit)
            break;
    }

    return active;
}

/*
 * handle_config_reload -- Resize session table and reopen listen sockets
 * Returns 1 if the caller should break out of the main loop, 0 otherwise
 */
static int handle_config_reload(BINKD_CONFIG **config, int srv_flag, int cli_flag)
{
    int i, new_max;
    BINKD_CONFIG *nc = lock_current_config();

    if (nc)
    {
        new_max = calc_max_sessions(nc, srv_flag, cli_flag);

        if (new_max != max_sessions)
        {
            sess_t *ns = realloc(sessions, new_max * sizeof(sess_t));

            if (ns)
            {
                for (i = max_sessions; i < new_max; i++)
                {
                    memset(&ns[i], 0, sizeof(sess_t));
                    memset(&ns[i].state, 0, sizeof(STATE));
                    ns[i].fd = INVALID_SOCKET;
                    ns[i].phase = SESS_FREE;
                }

                sessions = ns;
                max_sessions = new_max;

                Log(4, "Session table resized to %d slots", max_sessions);
            }
            else
            {
                Log(1, "Failed to resize session table, keeping current size");
            }
        }

        unlock_config_structure(nc, 0);
    }

    close_listen_sockets();
    *config = lock_current_config();

    if (srv_flag && open_listen_sockets(*config) < 0)
    {
        unlock_config_structure(*config, 0);
        return 1; /* fatal — break main loop */
    }

    unlock_config_structure(*config, 0);
    *config = lock_current_config();
    return 0;
}

/* evloop_cleanup -- Close sessions and free resources on exit */
static void evloop_cleanup(BINKD_CONFIG *config, int config_locked)
{
    int i;

    if (config_locked)
        unlock_config_structure(config, 0);

    if (sessions)
    {
        for (i = 0; i < max_sessions; i++)
        {
            if (sessions[i].phase == SESS_RUNNING)
            {
                amiga_proto_close(&sessions[i].state, config, 0);

                if (sessions[i].inbound)
                    n_servers--;
                else
                    n_clients--;
            }
            else if (sessions[i].phase == SESS_CONNECTING)
            {
                n_clients--;
            }
            sess_free(i);
        }

        free(sessions);
        sessions = NULL;
    }

    close_listen_sockets();
    amiga_sock_cleanup();
    Log(4, "evloop done");
}

/* amiga_evloop_run -- Entry point: init, then main WaitSelect() loop */
void amiga_evloop_run(BINKD_CONFIG *config, int srv_flag, int cli_flag)
{
    int config_locked = 0;
    time_t last_rescan = 0;
    time_t now;
    fd_set r, w;
    struct timeval tv;
    int n, maxfd;
    int active_sessions = 0;
    static int idle_loops = 0;

    /* Sync globals so try_outbound() and friends see the correct flags */
    server_flag = srv_flag;
    client_flag = cli_flag;

    sockfd_used = 0;
    srand((unsigned int)time(NULL));

    Log(5, "DEBUG: server_flag=%d, client_flag=%d", server_flag, client_flag);
    Log(5, "DEBUG: max_servers=%d, max_clients=%d", config->max_servers, config->max_clients);

    /* Initialise session table */
    max_sessions = calc_max_sessions(config, server_flag, client_flag);

    if (max_sessions < 2)
    {
        Log(2, "WARNING: max_sessions=%d is too low, forcing to 2", max_sessions);
        max_sessions = 2;
    }

    Log(4, "evloop start (AmigaOS 3, WaitSelect, %d slots)", max_sessions);

    if (!init_session_table(max_sessions))
        return;

    /* server: Open listen sockets */
    if (server_flag && open_listen_sockets(config) < 0)
    {
        Log(0, "evloop: cannot open listen sockets");
        free(sessions);
        sessions = NULL;
        return;
    }

    Log(5, "DEBUG: Listen sockets opened, sockfd_used=%d", sockfd_used);

    /* Initial outbound attempt before waiting (important for poll -p mode) */
    Log(5, "DEBUG: Initial try_outbound before main loop");
    try_outbound(config);
    last_rescan = time(NULL); /* Reset timer since we just did an attempt */

    /* ===== Main loop ===== */
    for (;;)
    {
        if (binkd_exit)
        {
            Log(1, "binkd_exit detected at loop start, exiting");
            break;
        }

        /* Build fd_sets */
        Log(5, "DEBUG: Building fd_sets");
        maxfd = build_fdsets(&r, &w);

        tv.tv_sec = 1;
        tv.tv_usec = 0L;

        /* WaitSelect() with nfds>0 but empty fd_sets causes guru #80000006 :/
         * Use select(0,...) as a pure sleep when no sockets are active */
        if (maxfd < 1 && (sockfd_used > 0 || n_clients > 0))
            maxfd = 1;

        Log(5, "DEBUG: Calling select() with maxfd=%d", maxfd);

        if (maxfd == 0)
        {
            /* No sockets yet -- use Delay() instead of select(0,...)
             * as WaitSelect with nfds=0 can block indefinitely on AmigaOS */
            Delay(50); /* 1 second = 50 ticks at 50Hz PAL */
            n = 0;     /* simulate timeout */
        }
        else
            n = select(maxfd + 1, &r, &w, NULL, &tv);

        Log(5, "DEBUG: select() returned n=%d", n);

        if (binkd_exit)
        {
            Log(1, "binkd_exit detected after select(), exiting");
            break;
        }

        Delay(1UL); /* 1 tick = 20ms @ 50Hz, prevents CPU hogging */

        /* Handle select errors */
        if (n < 0)
        {
            if (TCPERRNO == EINTR || TCPERRNO == EWOULDBLOCK)
            {
                Log(5, "DEBUG: select interrupted, continuing");
                continue;
            }

            if (TCPERRNO == ENOTSOCK || TCPERRNO == EBADF)
            {
                Log(2, "select: %s, reopening", TCPERR());

                close_listen_sockets();

                if (server_flag && open_listen_sockets(config) < 0)
                    break;

                continue;
            }

            Log(1, "select: %s", TCPERR());
            break;
        }
        else if (n == 0)
        {
            Log(5, "DEBUG: select timeout, continuing");
        }

        /* server: Accept new inbound connections */
        if (server_flag)
        {
            if (handle_server_accept(&r, config) < 0)
                break;
        }

        if (binkd_exit)
            break;

        /* server + client: Advance all active sessions */
        active_sessions = advance_sessions(&r, &w, config);

        if (binkd_exit)
            break;

        /* client: Time-based outbound scan + config reload */
        now = time(NULL);

        if (now - last_rescan >= (config->rescan_delay > 0 ? config->rescan_delay : 1) || last_rescan == 0)
        {
            Log(5, "DEBUG: Before try_outbound");

            try_outbound(config);

            Log(5, "DEBUG: After try_outbound");

            if (checkcfg())
            {
                if (handle_config_reload(&config, server_flag, client_flag))
                    break;

                config_locked = 1;
            }

            config->q_present = 0;
            last_rescan = now;
        }

        /* client: Poll-mode idle exit */
        /* Reset counter whenever there is something going on */
        if (n_clients > 0 || active_sessions > 0)
        {
            if (idle_loops > 0)
                Log(2, "Activity detected, reset idle counter");

            idle_loops = 0;
        }

        if (!server_flag && active_sessions == 0 && n_clients == 0)
        {
            idle_loops++;

            Log(2, "Idle loop %d/2 (no server, no sessions, no clients)", idle_loops);

            if (idle_loops > 1)
            {
                Log(0, "the queue is empty, quitting...");
                break;
            }
        }
    }
    /* ===== End main loop ===== */

    evloop_cleanup(config, config_locked);
}
