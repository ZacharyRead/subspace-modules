 /**************************************************************
 * Flags in safe warning
 *
 * This module will warn a player that enters a safe zone with
 * flags.
 *
 * By: Zachary Read
 *
 * ************************************************************/

#include "asss.h"
#include "objects.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Interfaces */
local Imodman *mm;
local Ichat *chat;
local Icmdman *cmd;
local Iplayerdata *pd;
local Iflagcore *flags;
local Iobjects *obj;

/************************************************************************/
/*                          Game Callbacks                              */
/************************************************************************/

/* Warn player with flags while entering safe. */
local void cFlagCheck(Player *p, int x, int y, int entering)
{
	if (entering > 0)
	{
		if (flags->CountPlayerFlags(p) > 0)
		{
			Link link = { NULL, p };
			LinkedList lst = { &link, &link };
			chat->SendAnyMessage(&lst, MSG_SYSOPWARNING, SOUND_BEEP1, NULL,	"WARNING: It is illegal to enter a safety zone with flags!");
			chat->SendModMessage("[%s] entered safety zone with flags", p->name);
		}
	}
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_flagsinsafewarning[] = "Flags in safe warning v0.5";

EXPORT int MM_flagsinsafewarning(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		flags = mm->GetInterface(I_FLAGCORE, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		obj = mm->GetInterface(I_OBJECTS, ALLARENAS);

		if (!chat || !pd || !flags || !cmd || !obj)
			return MM_FAIL;
		
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(obj);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(flags);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(chat);
		
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		mm->RegCallback(CB_SAFEZONE, cFlagCheck, arena);

		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		mm->UnregCallback(CB_SAFEZONE, cFlagCheck, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
