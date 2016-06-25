 /**************************************************************
 * Flags: No quick win
 *
 * If a player enters a safe zone with all flags, this module
 * will remove a single flag from that player to prevent them
 * from being able to quick win, as sitting in a safe zone with
 * all flags makes it impossible for other players to take the
 * flags away.
 *
 * The module was originally designed for the zone Devastation.
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
local Iarenaman *aman;
local Ichat *chat;
local Icmdman *cmd;
local Iplayerdata *pd;
local Iflagcore *flags;
local Iconfig *cfg;
local Iobjects *obj;

/* Arena data */
typedef struct Adata
{
	char numFlags;
}

/************************************************************************/
/*                          Game Callbacks                              */
/************************************************************************/

/* This function will take away a flag (id: 0) and place it at the default spawn location. */
local void cQuickwinCheck(Player *p, int x, int y, int entering)
{
	if (entering > 0)
	{
		int i = 0;
		FlagInfo fi;
		flags->GetFlags(p->arena, i, &fi, 1);
		Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
		if (adata->numFlags && (flags->CountPlayerFlags(p) == adata->numFlags))
		{
			fi.state = FI_NONE;
			fi.carrier = NULL;
			flags->SetFlags(p->arena, i, &fi, 1);
			chat->SendModMessage("A flag has been removed from player %s (possible Quickwin)", p->name);
		}
	}
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_flagsnoquickwin[] = "Flags to center v0.5";

EXPORT int MM_flagsnoquickwin(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		
		aman = mm->GetInterface(I_ARENAMAN, arena);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		flags = mm->GetInterface(I_FLAGCORE, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		obj = mm->GetInterface(I_OBJECTS, ALLARENAS);

		if (!aman || !chat || !pd || !flags || !cmd || !cfg || !obj)
			return MM_FAIL;
		
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(obj);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(flags);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(aman);
		
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		mm->RegCallback(CB_SAFEZONE, cQuickwinCheck, arena);
		
		arenaKey = aman->AllocateArenaData(sizeof(Adata));
		Adata *adata = P_ARENA_DATA(arena, arenaKey);
		adata->numFlags = cfg->GetInt(arena->cfg, "Flag", "FlagCount", 3);
		
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		aman->FreeArenaData(arenaKey);
		
		mm->UnregCallback(CB_SAFEZONE, cQuickwinCheck, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
