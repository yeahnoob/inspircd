/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inspircd.h"
#include "core_user.h"

CommandMode::CommandMode(Module* parent)
	: Command(parent, "MODE", 1)
	, seq(0)
{
	syntax = "<target> <modes> {<mode-parameters>}";
	memset(&sent, 0, sizeof(sent));
}

CmdResult CommandMode::Handle(const std::vector<std::string>& parameters, User* user)
{
	const std::string& target = parameters[0];
	Channel* targetchannel = ServerInstance->FindChan(target);
	User* targetuser = NULL;
	if (!targetchannel)
	{
		if (IS_LOCAL(user))
			targetuser = ServerInstance->FindNickOnly(target);
		else
			targetuser = ServerInstance->FindNick(target);
	}

	if ((!targetchannel) && ((!targetuser) || (IS_SERVER(targetuser))))
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nick/channel", target.c_str());
		return CMD_FAILURE;
	}
	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel);
		return CMD_SUCCESS;
	}

	// Populate a temporary Modes::ChangeList with the parameters
	Modes::ChangeList changelist;
	ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;
	ServerInstance->Modes.ModeParamsToChangeList(user, type, parameters, changelist);

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreMode, MOD_RESULT, (user, targetuser, targetchannel, changelist));

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_NONE;
	if (IS_LOCAL(user))
	{
		if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			if ((targetuser) && (user != targetuser))
			{
				// Local users may only change the modes of other users if a module explicitly allows it
				user->WriteNumeric(ERR_USERSDONTMATCH, ":Can't change mode for other users");
				return CMD_FAILURE;
			}

			// This is a mode change by a local user and modules didn't explicitly allow/deny.
			// Ensure access checks will happen for each mode being changed.
			flags |= ModeParser::MODE_CHECKACCESS;
		}
		else if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE; // Entire mode change denied by a module
	}
	else
		flags |= ModeParser::MODE_LOCALONLY;

	if (IS_LOCAL(user))
		ServerInstance->Modes->ProcessSingle(user, targetchannel, targetuser, changelist, flags);
	else
		ServerInstance->Modes->Process(user, targetchannel, targetuser, changelist, flags);

	if ((ServerInstance->Modes.GetLastParse().empty()) && (targetchannel) && (parameters.size() == 2))
	{
		/* Special case for displaying the list for listmodes,
		 * e.g. MODE #chan b, or MODE #chan +b without a parameter
		 */
		this->DisplayListModes(user, targetchannel, parameters[1]);
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandMode::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}

void CommandMode::DisplayListModes(User* user, Channel* chan, const std::string& mode_sequence)
{
	seq++;

	for (std::string::const_iterator i = mode_sequence.begin(); i != mode_sequence.end(); ++i)
	{
		unsigned char mletter = *i;
		if (mletter == '+')
			continue;

		ModeHandler* mh = ServerInstance->Modes->FindMode(mletter, MODETYPE_CHANNEL);
		if (!mh || !mh->IsListMode())
			return;

		/* Ensure the user doesnt request the same mode twice,
		 * so they can't flood themselves off out of idiocy.
		 */
		if (sent[mletter] == seq)
			continue;

		sent[mletter] = seq;
		ServerInstance->Modes.ShowListModeList(user, chan, mh);
	}
}

void CommandMode::DisplayCurrentModes(User* user, User* targetuser, Channel* targetchannel)
{
	if (targetchannel)
	{
		// Display channel's current mode string
		user->WriteNumeric(RPL_CHANNELMODEIS, "%s +%s", targetchannel->name.c_str(), targetchannel->ChanModes(targetchannel->HasUser(user)));
		user->WriteNumeric(RPL_CHANNELCREATED, "%s %lu", targetchannel->name.c_str(), (unsigned long)targetchannel->age);
	}
	else
	{
		if (targetuser == user || user->HasPrivPermission("users/auspex"))
		{
			// Display user's current mode string
			user->WriteNumeric(RPL_UMODEIS, ":+%s", targetuser->FormatModes());
			if (targetuser->IsOper())
			{
				ModeHandler* snomask = ServerInstance->Modes->FindMode('s', MODETYPE_USER);
				user->WriteNumeric(RPL_SNOMASKIS, "%s :Server notice mask", snomask->GetUserParameter(user).c_str());
			}
		}
		else
		{
			user->WriteNumeric(ERR_USERSDONTMATCH, ":Can't view modes for other users");
		}
	}
}
