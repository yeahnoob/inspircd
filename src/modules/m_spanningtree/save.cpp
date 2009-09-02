/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

/**
 * SAVE command - force nick change to UID on timestamp match
 */
bool TreeSocket::ForceNick(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;

	User* u = this->ServerInstance->FindNick(params[0]);
	time_t ts = atol(params[1].c_str());

	if (u && u->age == ts)
	{
		Utils->DoOneToAllButSender(prefix,"SAVE",params,prefix);

		if (!u->ForceNickChange(u->uuid.c_str()))
		{
			this->ServerInstance->Users->QuitUser(u, "Nickname collision");
		}
	}

	return true;
}

