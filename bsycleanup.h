/*
 * bsycleanup.h -- Cleanup functions for .bsy/.csy/.try files
 *
 * bsycleanup.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _BSYCLEANUP_H
#define _BSYCLEANUP_H

#include "readcfg.h"


/* cleanup_old_bsy -- Clean up old .bsy/.csy/.try files at startup */
void cleanup_old_bsy(BINKD_CONFIG *config);

#endif /* _BSYCLEANUP_H */
