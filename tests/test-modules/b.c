/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2010 Lawrence Livermore National Security, LLC.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  UCRL-CODE-2003-005.
 *  This file is part of Pdsh, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
 *
 *  Pdsh is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Pdsh is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Pdsh; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>

#include "src/pdsh/mod.h"

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static int opt_a(opt_t *, int, char *);
static int b_init (void);

/*
 * Export pdsh module operations structure
 */
struct pdsh_module_operations a_module_ops = {
    (ModInitF)       b_init,
    (ModExitF)       NULL,
    (ModReadWcollF)  NULL,
    (ModPostOpF)     NULL,
};

/*
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations a_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/*
 * Export module options
 */
struct pdsh_module_option a_module_options[] =
 { { 'a', NULL, "the a option for Module B", DSH | PCP, (optFunc) opt_a },
   PDSH_OPT_TABLE_END
 };

/*
 * Machines module info
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "B",
  "Mark Grondona",
  "Module test A",
  DSH,

  &a_module_ops,
  &a_rcmd_ops,
  &a_module_options[0],
};

static int opt_a(opt_t *pdsh_opt, int opt, char *arg)
{
    fprintf (stdout, "B: got option\n");
    return 0;
}

static int b_init (void)
{
    fprintf (stdout, "B: in init\n");
    return 0;
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */
