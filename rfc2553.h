/* ######################################################################

   RFC 2553 Emulation - Provides emulation for RFC 2553 getaddrinfo,
                        freeaddrinfo and getnameinfo
   
   These functions are necessary to write portable protocol independent
   networking. They transparently support IPv4, IPv6 and probably many 
   other protocols too. This implementation is needed when the host does 
   not support these standards. It implements a simple wrapper that 
   basically supports only IPv4. 

   Perfect emulation is not provided, but it is passable..
   
   Originally written by Jason Gunthorpe <jgg@debian.org> and placed into
   the Public Domain, do with it what you will.
  
   ##################################################################### */

#ifndef RFC2553EMU_H
#define RFC2553EMU_H

#include "iphdr.h"

/* Amiga: define EAI_* before including netdb.h to prevent libnix redefinition */
#if defined(AMIGA)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

  #undef EAI_NONAME
  #undef EAI_AGAIN
  #undef EAI_FAIL
  #undef EAI_NODATA
  #undef EAI_FAMILY
  #undef EAI_SOCKTYPE
  #undef EAI_SERVICE
  #undef EAI_ADDRFAMILY
  #undef EAI_MEMORY
  #undef EAI_SYSTEM
  #undef EAI_UNKNOWN
  #define EAI_NONAME     -1
  #define EAI_AGAIN      -2
  #define EAI_FAIL       -3
  #define EAI_NODATA     -4
  #define EAI_FAMILY     -5
  #define EAI_SOCKTYPE   -6
  #define EAI_SERVICE    -7
  #define EAI_ADDRFAMILY -8
  #define EAI_MEMORY     -9
  #define EAI_SYSTEM     -10
  #define EAI_UNKNOWN    -11

#include <netdb.h>

/* EAI_ADDRFAMILY is BSD/macOS specific; Linux/glibc does not define it
 * Map it to EAI_FAMILY which has the same meaning on those platforms */
#ifndef EAI_ADDRFAMILY
#ifdef EAI_FAMILY
#define EAI_ADDRFAMILY EAI_FAMILY
#else
#define EAI_ADDRFAMILY -9
#endif
#endif

#endif

/* Autosense getaddrinfo */
#if defined(AI_PASSIVE) && defined(EAI_NONAME) && !defined(AMIGA)
#define HAVE_GETADDRINFO
#endif

/* Autosense getnameinfo */
#if defined(NI_NUMERICHOST)
#define HAVE_GETNAMEINFO
#endif

/* getaddrinfo support? */
#ifndef HAVE_GETADDRINFO
  /* Renamed to avoid type clashing.. (for debugging) */
  struct addrinfo_emu
  {   
     int     ai_flags;     /* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
     int     ai_family;    /* PF_xxx */
     int     ai_socktype;  /* SOCK_xxx */
     int     ai_protocol;  /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
     size_t  ai_addrlen;   /* length of ai_addr */
     char   *ai_canonname; /* canonical name for nodename */
     struct sockaddr  *ai_addr; /* binary address */
     struct addrinfo_emu  *ai_next; /* next structure in linked list */
  };
  #define addrinfo addrinfo_emu

#ifdef AMIGA
#ifdef getaddrinfo
#undef getaddrinfo
#endif
#ifdef freeaddrinfo
#undef freeaddrinfo
#endif
#ifdef gai_strerror
#undef gai_strerror
#endif
#endif

  int getaddrinfo(const char *nodename, const char *servname,
                  const struct addrinfo *hints,
                  struct addrinfo **res);
  void freeaddrinfo(struct addrinfo *ai);
  char *gai_strerror(int ecode);

  #ifndef AI_PASSIVE
  #define AI_PASSIVE (1<<1)
  #endif
  
  #ifndef EAI_NONAME
  #define EAI_NONAME     -1
  #define EAI_AGAIN      -2
  #define EAI_FAIL       -3
  #define EAI_NODATA     -4
  #define EAI_FAMILY     -5
  #define EAI_SOCKTYPE   -6
  #define EAI_SERVICE    -7
  #define EAI_ADDRFAMILY -8
  #define EAI_MEMORY     -9
  #define EAI_SYSTEM     -10
  #define EAI_UNKNOWN    -11
  #endif

  /* If we don't have getaddrinfo then we probably don't have 
     sockaddr_storage either (same RFC) so we definately will not be
     doing any IPv6 stuff. Do not use the members of this structure to
     retain portability, cast to a sockaddr. */
  #define sockaddr_storage sockaddr_in
#endif

/* getnameinfo support (glibc2.0 has getaddrinfo only) */
#ifndef HAVE_GETNAMEINFO

  int getnameinfo(const struct sockaddr *sa, socklen_t salen,
		  char *host, size_t hostlen,
		  char *serv, size_t servlen,
		  int flags);

  #ifndef NI_MAXHOST
  #define NI_MAXHOST 1025
  #define NI_MAXSERV 32
  #endif

  #ifndef NI_NUMERICHOST
  #define NI_NUMERICHOST (1<<0)
  #define NI_NUMERICSERV (1<<1)
/*  #define NI_NOFQDN (1<<2) */
  #define NI_NAMEREQD (1<<3)
  #define NI_DATAGRAM (1<<4)
  #endif

  #define sockaddr_storage sockaddr_in
#endif

/* Glibc 2.0.7 misses this one */
#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST 0
#endif

/* Win32 doesn't support these */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

/* Win32 doesn't support these */
#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV 0
#endif


#endif
