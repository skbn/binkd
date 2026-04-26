/*
 * proto_amiga.c -- Amiga non-blocking BinkP protocol implementation
 *
 * proto_amiga.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sys.h"
#include "readcfg.h"
#include "common.h"
#include "protocol.h"
#include "ftnaddr.h"
#include "ftnnode.h"
#include "ftnq.h"
#include "tools.h"
#include "bsy.h"
#include "inbound.h"
#include "protoco2.h"
#include "prothlp.h"
#include "binlog.h"
#include "evloop.h"

/* External functions from protocol.c */
extern int init_protocol(STATE *state, SOCKET s_in, SOCKET s_out, FTN_NODE *to, FTN_ADDR *fa, BINKD_CONFIG *config);
extern int banner(STATE *state, BINKD_CONFIG *config);
extern int recv_block(STATE *state, BINKD_CONFIG *config);
extern int send_block(STATE *state, BINKD_CONFIG *config);
extern void bsy_touch(BINKD_CONFIG *config);
extern FTNQ *process_rcvdlist(STATE *state, FTNQ *q, BINKD_CONFIG *config);
extern int start_file_transfer(STATE *state, FTNQ *q, BINKD_CONFIG *config);
extern void ND_set_status(const char *status, FTN_ADDR *fa, STATE *state, BINKD_CONFIG *config);
extern void deinit_protocol(STATE *state, BINKD_CONFIG *config, int status);
extern void evt_set(EVTQ *eq);
extern void msg_send2(STATE *state, t_msg m, char *s1, char *s2);

/* External functions from other modules */
extern void log_end_of_session(int err, STATE *state, BINKD_CONFIG *config);
extern void inb_remove_partial(STATE *state, BINKD_CONFIG *config);
extern void good_try(FTN_ADDR *fa, char *comment, BINKD_CONFIG *config);
extern void bad_try(FTN_ADDR *fa, const char *error, const int where, BINKD_CONFIG *config);
extern int create_poll(FTN_ADDR *fa, int flvr, BINKD_CONFIG *config);
extern void hold_node(FTN_ADDR *fa, time_t hold_until, BINKD_CONFIG *config);
extern int binkd_exit;

/* External variables */
extern int n_servers;

/*
 * amiga_proto_open -- Initialise a session and send the BinkP banner
 *
 * fd      : Connected socket (same fd for in and out)
 * to      : Outbound node, NULL for inbound
 * fa      : Local AKA to use, may be NULL
 * host    : Remote hostname or dotted-IP string (caller-owned, stable)
 * port    : Remote port string, may be NULL
 * dst_ip  : Numeric remote IP, may be NULL (falls back to host)
 * config  : Current config
 *
 * Returns 0 on success, -1 on error (caller must close fd)
 */
int amiga_proto_open(STATE *state, SOCKET fd, FTN_NODE *to, FTN_ADDR *fa, const char *host, const char *port, const char *dst_ip, BINKD_CONFIG *config)
{
    struct sockaddr_storage sa;
    socklen_t salen = (socklen_t)sizeof(sa);
    char ownhost[BINKD_FQDNLEN + 1];
    char ownserv[MAXSERVNAME + 1];
    int rc;

    if (!init_protocol(state, fd, fd, to, fa, config))
        return -1;

    /* Peer identity for logging and %ip config checks */
    state->ipaddr = dst_ip ? (char *)dst_ip : (char *)host;
    state->peer_name = (host && *host) ? (char *)host : state->ipaddr;

    /* local endpoint: Not used further, skip to avoid dangling pointer */

    Log(2, "%s session with %s%s%s", to ? "outgoing" : "incoming", state->peer_name, port ? ":" : "", port ? port : "");

    /* banner() sends M_NUL lines and ADR messages */
    if (!banner(state, config))
        return -1;

    /* refuse if server limit reached */
    if (!to && n_servers > config->max_servers)
    {
        Log(1, "too many servers");
        msg_send2(state, M_BSY, "Too many servers", 0);

        return -1;
    }

    return 0;
}

/*
 * amiga_proto_step -- Run one recv/send iteration of the BinkP loop
 *
 * readable : Non-zero if the socket has incoming data (from WaitSelect)
 * writable : Non-zero if the socket can accept outgoing data
 *
 * Returns APROTO_RUNNING, APROTO_DONE_OK, or APROTO_DONE_ERR
 *
 * This is the loop body of protocol() with the select() call removed
 * recv_block() and send_block() already handle EWOULDBLOCK gracefully,
 * so calling this on a non-blocking socket is safe
 */
int amiga_proto_step(STATE *state, int readable, int writable, BINKD_CONFIG *config)
{
    FTNQ *q;
    int no;

    if (state->io_error)
        return APROTO_DONE_ERR;

    /* Advance outgoing file queue if nothing is being sent */
    if (!state->local_EOB && state->q && !state->out.f && !state->waiting_for_GOT && !state->off_req_sent && state->state != P_NULL)
    {
        while (1)
        {
            q = 0;
            if (state->flo.f || (q = select_next_file(state->q, state->fa, state->nfa)) != 0)
            {
                if (start_file_transfer(state, q, config))
                    break;
            }
            else
            {
                q_free(state->q, config);
                state->q = 0;
                break;
            }
        }
    }

    /* Nothing left to send — issue EOB */
    if (!state->out.f && !state->q && !state->local_EOB && state->state != P_NULL && !state->sent_fls)
    {
        if (!state->delay_EOB || (state->major * 100 + state->minor > 100))
        {
            state->local_EOB = 1;
            msg_send2(state, M_EOB, 0, 0);
        }
    }

    /* Recv step: Only when socket is readable */
    if (readable)
    {
        if (!recv_block(state, config))
            return APROTO_DONE_ERR;
    }

    /*
     * send step: drive even when writable=0 if there is buffered data,
     * pending messages, a file mid-transfer, or an EOF to flush.
     */
    if (writable || state->msgs || state->oleft || state->send_eof || (state->out.f && !state->off_req_sent && !state->waiting_for_GOT))
    {
        no = send_block(state, config);

        if (!no && no != 2)
            return APROTO_DONE_ERR;
    }

    bsy_touch(config);

    /* Batch/Session-end detection — Mirrors the break logic in protocol() */
    if (state->remote_EOB && !state->sent_fls && state->local_EOB && !state->GET_FILE_balance && !state->in.f && !state->out.f)
    {
        if (state->rcvdlist)
        {
            state->q = process_rcvdlist(state, state->q, config);

            q_to_killlist(&state->killlist, &state->n_killlist, state->q);
            free_rcvdlist(&state->rcvdlist, &state->n_rcvdlist);
        }

        Log(6, "batch: %i msgs", state->msgs_in_batch);

        if (state->msgs_in_batch <= 2 || (state->major * 100 + state->minor <= 100))
        {
            /* Session done */
            ND_set_status("", &state->ND_addr, state, config);
            state->ND_addr.z = -1;

            return APROTO_DONE_OK;
        }

        /* Start next batch */
        state->msgs_in_batch = 0;
        state->remote_EOB = 0;
        state->local_EOB = 0;

        if (OK_SEND_FILES(state, config))
        {
            state->q = q_scan_boxes(state->q, state->fa, state->nfa, state->to ? 1 : 0, config);
            state->q = q_sort(state->q, state->fa, state->nfa, config);
        }
    }

    return APROTO_RUNNING;
}

/*
 * amiga_proto_close -- Flush remaining I/O and release STATE resources
 * Must be called after APROTO_DONE_OK or APROTO_DONE_ERR
 */
void amiga_proto_close(STATE *state, BINKD_CONFIG *config, int ok)
{
    int no;
    char buf[BLK_HDR_SIZE + MAX_BLKSIZE];
    int status = ok ? 0 : 1;

    /* Drain inbound queue */
    if (!state->io_error)
    {
        while ((no = recv(state->s_in, buf, (int)sizeof(buf), 0)) > 0)
            Log(9, "purged %d bytes", no);
    }

    /* Flush pending outbound messages */
    while (!state->io_error && (state->msgs || (state->optr && state->oleft)) && send_block(state, config))
        ;

    if (ok)
    {
        log_end_of_session(0, state, config);
        process_killlist(state->killlist, state->n_killlist, 's');
        inb_remove_partial(state, config);

        if (state->to)
            good_try(&state->to->fa, "CONNECT/BND", config);
    }
    else
    {
        log_end_of_session(1, state, config);
        process_killlist(state->killlist, state->n_killlist, 0);

        if (!binkd_exit && state->to)
            bad_try(&state->to->fa, "Bad session", BAD_IO, config);

        /* Restore poll flavour if files were left mid-transfer */
        if (state->to && tolower(state->maxflvr) != 'h')
        {
            Log(4, "restoring poll with '%c' flavour", state->maxflvr);

            create_poll(&state->to->fa, state->maxflvr, config);
        }
    }

    if (state->to && state->r_skipped_flag && config->hold_skipped > 0)
    {
        Log(2, "holding skipped mail for %lu sec", (unsigned long)config->hold_skipped);

        hold_node(&state->to->fa, safe_time() + config->hold_skipped, config);
    }

    deinit_protocol(state, config, status);
    evt_set(state->evt_queue);
    state->evt_queue = NULL;
}
