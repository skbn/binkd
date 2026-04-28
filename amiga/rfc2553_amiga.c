/*
 * rfc2553_amiga.c -- getaddrinfo/getnameinfo fallback for AmigaOS 3
 *
 * rfc2553_amiga.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifdef AMIGA

#include "amiga/bsdsock.h" /* LP stubs + SocketBase */
#include "rfc2553.h"       /* sets HAVE_GETADDRINFO / HAVE_GETNAMEINFO */
#include "sem.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define safe_strncpy(dst, src, n)   \
    do                              \
    {                               \
        strncpy((dst), (src), (n)); \
        (dst)[(n) - 1] = '\0';      \
    } while (0)

#ifndef HAVE_GETADDRINFO

void freeaddrinfo(struct addrinfo *ai); /* forward decl */

int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo **tail = res;
    struct hostent *hent = NULL;
    unsigned int port;
    int proto;
    const char *end;
    char **addrp;

    static char passive_dummy = '\0';
    char *passive_list[2] = {&passive_dummy, NULL};

    if (!res)
    {
        return EAI_UNKNOWN;
    }

    *res = NULL;

    port = servname ? htons((unsigned short)strtol(servname, (char **)&end, 0)) : 0;
    proto = (hints && hints->ai_socktype) ? hints->ai_socktype : SOCK_STREAM;

    lockresolvsem();

    if (servname && end != servname + strlen(servname))
    {
        struct servent *se = NULL;

        if (!hints || hints->ai_socktype == SOCK_STREAM)
            se = getservbyname((char *)servname, "tcp");

        if (hints && hints->ai_socktype == SOCK_DGRAM)
            se = getservbyname((char *)servname, "udp");

        if (!se)
        {
            releaseresolvsem();
            return EAI_NONAME;
        }

        port = se->s_port;

        if (strcmp((char *)se->s_proto, "tcp") == 0)
            proto = SOCK_STREAM;
        else if (strcmp((char *)se->s_proto, "udp") == 0)
            proto = SOCK_DGRAM;
        else
        {
            releaseresolvsem();
            return EAI_NONAME;
        }

        if (hints && hints->ai_socktype && hints->ai_socktype != proto)
        {
            releaseresolvsem();
            return EAI_SERVICE;
        }
    }

    if (!hints || !(hints->ai_flags & AI_PASSIVE))
    {
        unsigned long nip = inet_addr((char *)nodename);

        if (nip != (unsigned long)INADDR_NONE)
        {
            struct addrinfo *ai = (struct addrinfo *)calloc(1, sizeof(*ai));
            struct sockaddr_in *sin;

            if (!ai)
            {
                releaseresolvsem();
                return EAI_MEMORY;
            }
            *tail = ai;

            sin = (struct sockaddr_in *)calloc(1, sizeof(*sin));

            if (!sin)
            {
                free(ai);
                releaseresolvsem();
                return EAI_MEMORY;
            }

            ai->ai_family = AF_INET;
            ai->ai_socktype = proto;
            ai->ai_protocol = (proto == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;
            ai->ai_addrlen = sizeof(*sin);
            ai->ai_addr = (struct sockaddr *)sin;
            sin->sin_family = AF_INET;
            sin->sin_port = port;
            sin->sin_addr.s_addr = nip;

            releaseresolvsem();
            return 0;
        }

        hent = gethostbyname((char *)nodename);

        if (!hent)
        {
            int herr = errno;
            releaseresolvsem();
            return (herr == TRY_AGAIN) ? EAI_AGAIN : (herr == NO_RECOVERY) ? EAI_FAIL
                                                                           : EAI_NONAME;
        }

        if (!hent->h_addr_list || !hent->h_addr_list[0])
        {
            releaseresolvsem();
            return EAI_NONAME;
        }

        addrp = hent->h_addr_list;
    }
    else
    {
        addrp = passive_list;
    }

    for (; *addrp; addrp++)
    {
        struct addrinfo *ai = (struct addrinfo *)calloc(1, sizeof(*ai));
        struct sockaddr_in *sin;

        if (!ai)
        {
            releaseresolvsem();
            freeaddrinfo(*res);
            *res = NULL;
            return EAI_MEMORY;
        }

        if (!*res)
            *res = ai;
        *tail = ai;
        tail = &ai->ai_next;

        sin = (struct sockaddr_in *)calloc(1, sizeof(*sin));

        if (!sin)
        {
            releaseresolvsem();
            freeaddrinfo(*res);
            *res = NULL;
            return EAI_MEMORY;
        }

        ai->ai_family = AF_INET;
        ai->ai_socktype = proto;
        ai->ai_protocol = (proto == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;
        ai->ai_addrlen = sizeof(*sin);
        ai->ai_addr = (struct sockaddr *)sin;
        sin->sin_family = AF_INET;
        sin->sin_port = port;

        if (!hints || !(hints->ai_flags & AI_PASSIVE))
        {
            size_t cpylen = sizeof(sin->sin_addr);

            if (hent->h_length > 0 && (size_t)hent->h_length < cpylen)
                cpylen = (size_t)hent->h_length;

            memcpy(&sin->sin_addr, *addrp, cpylen);
        }
    }

    releaseresolvsem();
    return 0;
}

void freeaddrinfo(struct addrinfo *ai)
{
    struct addrinfo *next;

    while (ai)
    {
        free(ai->ai_addr);
        next = ai->ai_next;
        free(ai);
        ai = next;
    }
}

static const char *ai_errlist[] =
    {
        "Success",
        "hostname nor servname provided, or not known",
        "Temporary failure in name resolution",
        "Non-recoverable failure in name resolution",
        "No address associated with hostname",
        "ai_family not supported",
        "ai_socktype not supported",
        "service name not supported for ai_socktype",
        "Address family for hostname not supported",
        "Memory allocation failure",
        "System error returned in errno",
        "Unknown error",
};

char *gai_strerror(int ecode)
{
    if (ecode > 0 || ecode < EAI_UNKNOWN)
        ecode = EAI_UNKNOWN;
    return (char *)ai_errlist[-ecode];
}

#endif /* !HAVE_GETADDRINFO */

#ifndef HAVE_GETNAMEINFO

#ifndef NI_DATAGRAM
#define NI_DATAGRAM (1 << 4)
#endif

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

    if (sa->sa_family != AF_INET)
        return EAI_ADDRFAMILY;

    if (host && hostlen > 0)
    {
        if (!(flags & NI_NUMERICHOST))
        {
            struct hostent *he;

            lockresolvsem();
            he = gethostbyaddr((char *)&sin->sin_addr, sizeof(sin->sin_addr), AF_INET);

            if (he)
            {
                safe_strncpy(host, (char *)he->h_name, hostlen);
                releaseresolvsem();
            }
            else
            {
                int herr = errno;
                releaseresolvsem();
                if (flags & NI_NAMEREQD)
                    return (herr == TRY_AGAIN) ? EAI_AGAIN : (herr == NO_RECOVERY) ? EAI_FAIL
                                                                                   : EAI_NONAME;
                flags |= NI_NUMERICHOST;
            }
        }

        if (flags & NI_NUMERICHOST)
        {
            lockhostsem();
            safe_strncpy(host, (char *)Inet_NtoA(sin->sin_addr.s_addr), hostlen);
            releasehostsem();
        }
    }

    if (serv && servlen > 0)
    {
        if (!(flags & NI_NUMERICSERV))
        {
            struct servent *se;

            lockresolvsem();

            se = (flags & NI_DATAGRAM) ? getservbyport(ntohs(sin->sin_port), "udp") : getservbyport(ntohs(sin->sin_port), "tcp");

            if (se)
            {
                safe_strncpy(serv, (char *)se->s_name, servlen);
                releaseresolvsem();
            }
            else
            {
                releaseresolvsem();

                if (flags & NI_NAMEREQD)
                    return EAI_NONAME;

                flags |= NI_NUMERICSERV;
            }
        }

        if (flags & NI_NUMERICSERV)
            snprintf(serv, servlen, "%u", ntohs(sin->sin_port));
    }

    return 0;
}

#endif /* !HAVE_GETNAMEINFO */
#endif /* AMIGA */
