 /**************************************************************
 * Doors Module
 *
 * Opens, closes, and returns the door state back to normal
 * using the command ?doors or ?doormode.
 *
 * By: Zachary Read
 *
 * ************************************************************/
#include <string.h>

#include "asss.h"
#include "clientset.h"

/* Interfaces */
local Imodman *mm;
local Ichat *chat;
local Icmdman *cmd;
local Iplayerdata *pd;
local Iconfig *cfg;
local Iclientset *cs;
local Iarenaman *aman;

local override_key_t ok_Doormode;

/************************************************************************/
/*                         Player Commands                              */
/************************************************************************/

local helptext_t doors =
"Targets: none\n"
"Args: open, close, normal\n"
"Using ?doors <open/close/normal> will either open them, close them, or randomize them.\n";

/* When someone types ?doors*/
local void cDoors(const char *command, const char *params, Player *p, const Target *target)
{
	char *find;
	if ((find = strstr(params, "off")) || (find = strstr(params, "open")))
	{
		Player *g;
		Link *link;
		
		cs->ArenaOverride(p->arena, ok_Doormode, 0);
		
		pd->Lock();
		FOR_EACH_PLAYER(g)
		{
			if (g->arena == p->arena)
				cs->SendClientSettings(g);
		}
		pd->Unlock();
		
		chat->SendArenaSoundMessage(p->arena, 2, "Doors will now open.");
		return;
	}
	else if ((find = strstr(params, "on")) || (find = strstr(params, "close")))
	{
		Player *g;
		Link *link;
		
		cs->ArenaOverride(p->arena, ok_Doormode, 255);

		pd->Lock();
		FOR_EACH_PLAYER(g)
		{
			if (g->arena == p->arena)
				cs->SendClientSettings(g);
		}
		pd->Unlock();
		chat->SendArenaSoundMessage(p->arena, 2, "Doors will now close.");
		return;
	}
	else if ((find = strstr(params, "normal")) || (find = strstr(params, "random")))
	{
		Player *g;
		Link *link;
		
		cs->ArenaUnoverride(p->arena, ok_Doormode);
		
		pd->Lock();
		FOR_EACH_PLAYER(g)
		{
			if (g->arena == p->arena)
				cs->SendClientSettings(g);
		}
		pd->Unlock();
		chat->SendArenaSoundMessage(p->arena, 2, "Doors will now return to normal.");
		return;
	}
	else
	{
		chat->SendMessage(p, "Syntax: ?doors open/close/normal");
		return;
	}
}

/************************************************************************/
/*                           Module Init                                */
/************************************************************************/

EXPORT const char info_doors[] = "Doors v0.5";

EXPORT int MM_doors(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;

		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		cs = mm->GetInterface(I_CLIENTSET, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		
		if (!chat || !pd || !cmd || !cfg || !cs || !aman)
			return MM_FAIL;
		
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(cs);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(chat);
		
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		cmd->AddCommand("doors", cDoors, arena, doors);
		cmd->AddCommand("doormode", cDoors, arena, doors);
			
		ok_Doormode = cs->GetOverrideKey("Door", "Doormode");

		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		cmd->RemoveCommand("doors", cDoors, arena);
		cmd->RemoveCommand("doormode", cDoors, arena);

		return MM_OK;
	}
	return MM_FAIL;
}
