/*
 * compat_netinet_in.h -- Wrapper for netinet/in.h typedef clash
 *
 * compat_netinet_in.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _AMIGA_COMPAT_NETINET_IN_H
#define _AMIGA_COMPAT_NETINET_IN_H

#ifdef __GNUC__
#include_next <netinet/in.h>
#else
#include <netinet/in.h>
#endif

#endif /* _AMIGA_COMPAT_NETINET_IN_H */
