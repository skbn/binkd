/*
 * session.c -- session management for AmigaOS 3 binkd
 *
 * session.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <exec/types.h>
#include <proto/exec.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "sys.h"
#include "iphdr.h"
#include "readcfg.h"
#include "common.h"
#include "tools.h"
#include "client.h"
#include "protocol.h"
#include "ftnq.h"
#include "ftnnode.h"
#include "ftnaddr.h"
#include "bsy.h"
#include "iptools.h"
#include "rfc2553.h"
#include "srv_gai.h"
#include "amiga/bsdsock.h"
#include "amiga/evloop_int.h"
#include "amiga/proto_amiga.h"

extern int binkd_exit;
extern int ext_rand;
extern int client_flag;
extern int poll_flag;

/* Session table */
int sess_alloc()
{
    int i;

    for (i = 0; i < max_sessions; i++)
    {
        if (sessions[i].phase == SESS_FREE)
        {
            memset(&sessions[i], 0, sizeof(sess_t));
            sessions[i].fd = INVALID_SOCKET;
            sessions[i].phase = SESS_FREE;
            return i;
        }
    }

    return -1;
}

void sess_free(int idx)
{
    sess_t *s = &sessions[idx];

    if (s->fd != INVALID_SOCKET)
    {
        soclose(s->fd);
        s->fd = INVALID_SOCKET;
    }

    if (s->ai_head)
    {
        freeaddrinfo(s->ai_head);
        s->ai_head = NULL;
    }

    memset(&s->state, 0, sizeof(STATE));
    s->phase = SESS_FREE;
}

/* Inbound: accept a new connection */
void do_accept(SOCKET lfd, BINKD_CONFIG *config)
{
    struct sockaddr_storage sa;
    socklen_t salen = (socklen_t)sizeof(sa);
    SOCKET fd;
    int idx;
    sess_t *s;
    char host[BINKD_FQDNLEN + 1];
    char ip[BINKD_FQDNLEN + 1];

    fd = accept(lfd, (struct sockaddr *)&sa, &salen);

    if (fd == INVALID_SOCKET)
    {
        if (TCPERRNO != EWOULDBLOCK && TCPERRNO != EAGAIN)
        {
            Log(1, "accept(): %s", TCPERR());

            /* ENOTSOCK/EOPNOTSUPP: listen socket lost or no longer valid
             * Close all listen sockets to allow evloop to recover */
            if (TCPERRNO == ENOTSOCK || TCPERRNO == EOPNOTSUPP)
            {
                close_listen_sockets();
            }
        }

        return;
    }

    if (binkd_exit)
    {
        soclose(fd);
        return;
    }

    idx = sess_alloc();

    if (idx < 0)
    {
        Log(1, "session table full, refusing inbound");
        soclose(fd);
        return;
    }

    /* getnameinfo() Is unreliable on AmiTCP: use inet_ntoa directly */
    if (((struct sockaddr *)&sa)->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&sa;
        strnzcpy(ip, inet_ntoa(sa4->sin_addr.s_addr), BINKD_FQDNLEN);
    }
    else
    {
        strnzcpy(ip, "unknown", BINKD_FQDNLEN);
    }

    /* Backresolv not supported on AmiTCP: always use IP as host */
    strnzcpy(host, ip, BINKD_FQDNLEN);

    set_nonblock(fd);

    s = &sessions[idx];
    s->fd = fd;
    s->inbound = 1;
    s->node = NULL;
    s->ai_head = NULL;
    s->last_io = time(NULL);
    strnzcpy(s->host, host, BINKD_FQDNLEN);
    strnzcpy(s->ip, ip, BINKD_FQDNLEN);
    s->port[0] = '\0';

    if (amiga_proto_open(&s->state, fd, NULL, NULL, s->host, NULL, s->ip, config) != 0)
    {
        Log(1, "proto_open failed for %s", ip);
        sess_free(idx);
        return;
    }

    s->phase = SESS_RUNNING;
    n_servers++;
    Log(4, "inbound slot[%d] from %s", idx, ip);
}

/* Outbound: Non-blocking connect() */
int start_connect(sess_t *s, BINKD_CONFIG *config)
{
    SOCKET fd;
    int rc;

    s->ip[0] = '\0';
    s->port[0] = '\0';

    fd = socket(s->ai_cur->ai_family, s->ai_cur->ai_socktype, s->ai_cur->ai_protocol);

    if (fd == INVALID_SOCKET)
    {
        Log(1, "outbound socket(): %s", TCPERR());
        return -1;
    }

    /* getnameinfo() is unreliable on AmiTCP: may return rc=0 with garbage
     * Use inet_ntoa/ntohs directly */
    if (s->ai_cur->ai_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)s->ai_cur->ai_addr;
        strnzcpy(s->ip, inet_ntoa(sa4->sin_addr.s_addr), BINKD_FQDNLEN);
        snprintf(s->port, MAXPORTSTRLEN, "%u", (unsigned)ntohs(sa4->sin_port));
    }
    else
    {
        strnzcpy(s->ip, "unknown", BINKD_FQDNLEN);
        strnzcpy(s->port, "0", MAXPORTSTRLEN);
    }

    Log(4, "connecting %s [%s]:%s", s->host, s->ip, s->port);

    if (config->bindaddr[0])
    {
        struct addrinfo src_h, *src_ai;
        memset(&src_h, 0, sizeof(src_h));
        src_h.ai_family = s->ai_cur->ai_family;
        src_h.ai_socktype = SOCK_STREAM;
        src_h.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(config->bindaddr, NULL, &src_h, &src_ai) == 0)
        {
            bind(fd, src_ai->ai_addr, (int)src_ai->ai_addrlen);
            freeaddrinfo(src_ai);
        }
    }

    set_nonblock(fd);

    rc = connect(fd, s->ai_cur->ai_addr, (int)s->ai_cur->ai_addrlen);

    if (rc == 0 || TCPERRNO == EINPROGRESS || TCPERRNO == EWOULDBLOCK)
    {
        s->fd = fd;
        s->conn_start = time(NULL);
        return 0;
    }

    Log(1, "connect %s: %s", s->host, TCPERR());

    bad_try(&s->node->fa, TCPERR(), BAD_CALL, config);
    soclose(fd);

    return -1;
}

int try_outbound(BINKD_CONFIG *config)
{
    FTN_NODE *node;
    sess_t *s;
    int idx, rc;
    struct addrinfo hints;
    char dest[FTN_ADDR_SZ + 1];
    char host[BINKD_FQDNLEN + 5 + 1];
    char port[MAXPORTSTRLEN + 1];

    if (!client_flag)
        return 0;

    if (!config->q_present)
    {
        q_free(SCAN_LISTED, config);

        if (config->printq)
            Log(-1, "scan\r");

        q_scan(SCAN_LISTED, config);
        config->q_present = 1;

        if (config->printq)
        {
            q_list(stderr, SCAN_LISTED, config);
            Log(-1, "idle\r");
        }
    }

    if (n_clients >= config->max_clients)
        return 0;

    node = q_next_node(config);

    if (!node)
        return 0;

    ftnaddress_to_str(dest, &node->fa);

    if (!bsy_test(&node->fa, F_BSY, config) || !bsy_test(&node->fa, F_CSY, config))
    {
        Log(4, "%s busy", dest);
        return 0;
    }

    idx = sess_alloc();

    if (idx < 0)
    {
        Log(2, "table full, deferring %s", dest);
        return 0;
    }

    s = &sessions[idx];
    memset(s, 0, sizeof(*s));
    s->fd = INVALID_SOCKET;
    s->node = node;
    s->inbound = 0;

    rc = get_host_and_port(1, host, port, node->hosts, &node->fa, config);

    if (rc <= 0)
    {
        Log(1, "%s: bad host list", dest);
        sess_free(idx);

        return 0;
    }

    strnzcpy(s->host, host, BINKD_FQDNLEN);
    strnzcpy(s->port, port, MAXPORTSTRLEN);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = node->IP_afamily;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rc = srv_getaddrinfo(host, port, &hints, &s->ai_head);

    if (rc != 0)
    {
        Log(1, "%s: getaddrinfo error code=%d: %s", dest, rc, gai_strerror(rc));

        bad_try(&node->fa, "getaddrinfo failed", BAD_CALL, config);
        sess_free(idx);

        return 0;
    }

    s->ai_cur = s->ai_head;

    if (start_connect(s, config) != 0)
    {
        sess_free(idx);
        return 0;
    }

    s->phase = SESS_CONNECTING;
    n_clients++;

    Log(4, "outbound slot[%d] -> %s", idx, dest);

    return 1;
}

/* Check completion of async connect() */
void check_connect(int idx, BINKD_CONFIG *config)
{
    sess_t *s = &sessions[idx];
    int err = 0;
    socklen_t el = (socklen_t)sizeof(err);
    int tmo;

    tmo = config->connect_timeout ? config->connect_timeout : 30;

    if ((int)(time(NULL) - s->conn_start) >= tmo)
    {
        Log(1, "connect timeout -> %s", s->host);

        bad_try(&s->node->fa, "Timeout", BAD_CALL, config);
        n_clients--;
        sess_free(idx);

        return;
    }

    getsockopt(s->fd, SOL_SOCKET, SO_ERROR, (char *)&err, &el);

    if (err)
    {
        Log(1, "connect -> %s: %s", s->host, strerror(err));

        bad_try(&s->node->fa, strerror(err), BAD_CALL, config);

        soclose(s->fd);
        s->fd = INVALID_SOCKET;
        s->ai_cur = s->ai_cur->ai_next;

        if (s->ai_cur && start_connect(s, config) == 0)
            return; /* trying next address */

        n_clients--;
        sess_free(idx);

        return;
    }

    Log(4, "connected -> %s [%s]", s->host, s->ip);
    ext_rand = rand();

    if (amiga_proto_open(&s->state, s->fd, s->node, NULL, s->host, s->port, s->ip, config) != 0)
    {
        Log(1, "proto_open failed for %s", s->host);

        n_clients--;
        sess_free(idx);
        return;
    }

    s->phase = SESS_RUNNING;
    s->last_io = time(NULL);
}

/* Run one protocol step on an active session */
void do_session_step(int idx, int rd, int wr, BINKD_CONFIG *config)
{
    sess_t *s = &sessions[idx];
    int rc;
    int tmo;

    tmo = config->nettimeout ? config->nettimeout : 300;

    if ((int)(time(NULL) - s->last_io) >= tmo)
    {
        Log(1, "slot[%d] net timeout", idx);

        if (s->node)
            bad_try(&s->node->fa, "Timeout", BAD_IO, config);

        amiga_proto_close(&s->state, config, 0);

        if (s->inbound)
            n_servers--;
        else
            n_clients--;

        sess_free(idx);

        return;
    }

    if (s->fd == INVALID_SOCKET)
    {
        Log(1, "slot[%d] invalid socket, closing session", idx);

        if (s->node)
            bad_try(&s->node->fa, "Invalid socket", BAD_IO, config);

        if (s->inbound)
            n_servers--;
        else
            n_clients--;

        sess_free(idx);

        return;
    }

    /* WaitSelect() may not report a readable socket when the remote has
     * sent a TCP END.  Probe with MSG_PEEK so recv_block() sees the EOF */
    if (!rd && !wr && s->state.state != P_NULL)
    {
        char peek;
        int pr = recv(s->fd, &peek, 1, MSG_PEEK);

        if (pr == 0 || (pr < 0 && TCPERRNO != EWOULDBLOCK && TCPERRNO != EAGAIN))
            rd = 1;
    }

    rc = amiga_proto_step(&s->state, rd, wr, config);

    if (rd || wr)
        s->last_io = time(NULL);

    if (rc == APROTO_RUNNING)
        return;

    amiga_proto_close(&s->state, config, rc == APROTO_DONE_OK);

    Log(4, "slot[%d] %s", idx, rc == APROTO_DONE_OK ? "OK" : "ERR");

    if (s->inbound)
        n_servers--;
    else
        n_clients--;

    sess_free(idx);

    if (poll_flag && n_clients == 0 && n_servers == 0)
        binkd_exit = 1;
}
