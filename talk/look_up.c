/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
   Free Software Foundation, Inc.

   This file is part of GNU Inetutils.

   GNU Inetutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Inetutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Inetutils; see the file COPYING.  If not, write
   to the Free Software Foundation, Inc., 51 Franklin Street,
   Fifth Floor, Boston, MA 02110-1301 USA. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_OSOCKADDR_H
# include <osockaddr.h>
#endif
#include <protocols/talkd.h>
#include <unistd.h>
#include <errno.h>
#include "talk_ctl.h"
#include "talk.h"

/*
 * Look for an invitation on 'machine'
 */
static int
look_for_invite (CTL_RESPONSE *rp)
{
  current_state = "Checking for invitation on caller's machine";
  ctl_transact (his_machine_addr, msg, LOOK_UP, rp);
  /* the switch is for later options, such as multiple invitations */
  switch (rp->answer)
    {

    case SUCCESS:
      msg.id_num = htonl (rp->id_num);
      return (1);

    default:
      /* there wasn't an invitation waiting for us */
      return (0);
    }
}

/*
 * See if the local daemon has an invitation for us.
 */
int
check_local ()
{
  CTL_RESPONSE response;
  register CTL_RESPONSE *rp = &response;

  /* the rest of msg was set up in get_names */
  msg.ctl_addr.sa_family = htons (ctl_addr.sin_family);
  memcpy (msg.ctl_addr.sa_data,
	  ((struct sockaddr *) &ctl_addr)->sa_data,
	  sizeof ((struct sockaddr *) & ctl_addr)->sa_data);

  /* must be initiating a talk */
  if (!look_for_invite (rp))
    return (0);
  /*
   * There was an invitation waiting for us,
   * so connect with the other (hopefully waiting) party
   */
  current_state = "Waiting to connect with caller";
  do
    {
      if (rp->addr.sa_family != AF_INET)
	p_error ("Response uses invalid network address");
      errno = 0;
      if (connect (sockt,
		   (struct sockaddr *) &rp->addr, sizeof (rp->addr)) != -1)
	return (1);
    }
  while (errno == EINTR);
  if (errno == ECONNREFUSED)
    {
      /*
       * The caller gave up, but his invitation somehow
       * was not cleared. Clear it and initiate an
       * invitation. (We know there are no newer invitations,
       * the talkd works LIFO.)
       */
      ctl_transact (his_machine_addr, msg, DELETE, rp);
      close (sockt);
      open_sockt ();
      return (0);
    }
  p_error ("Unable to connect with initiator");
}

