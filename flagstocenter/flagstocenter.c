 /**************************************************************
 * Flags to center
 *
 * This module allows a moderator to warp all the game flags
 * back to the center and optionally neutralize them.
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
/*                          Player Commands                             */
/************************************************************************/

local helptext_t flagstocenter =
"Targets: none\n"
"Args: {-c}\n"
"Neutralizes and warps all uncarried flags back to center\n"
"If {-c} is added, it will organize (but not neutralize) all flags in center.";

/* ?flagstocenter */
local void cCenterFlags(const char *command, const char *params, Player *p, const Target *target)
{
	int i=0;
	int x = 509, y = 512;
	FlagInfo fi;
	Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
	for (; i < adata->numFlags; i++)
	{
		flags->GetFlags(p->arena, i, &fi, 1);
		if (fi.state != FI_CARRIED)
		{
			if (x == 515)
			{
				x = 509;
				y++;
			}
			fi.x = x;
			fi.y = y;
			x++;
			if (!strstr(params, "-c"))
			{
				/* Neutralize the flags sporadically */
				fi.state = FI_NONE;
			}
			flags->SetFlags(p->arena, i, &fi, 1);
		}
	}
	chat->SendArenaSoundMessage(p->arena, 26, "All uncarried flags have been sent to center!");
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_flagstocenter[] = "Flags to center v0.5";

EXPORT int MM_flagstocenter(int action, Imodman *mm_, Arena *arena)
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
		arenaKey = aman->AllocateArenaData(sizeof(Adata));
		Adata *adata = P_ARENA_DATA(arena, arenaKey);
		adata->numFlags = cfg->GetInt(arena->cfg, "Flag", "FlagCount", 3);
		cmd->AddCommand("flagstocenter", cCenterFlags, arena, flagstocenter);
		
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		aman->FreeArenaData(arenaKey);
		cmd->RemoveCommand("flagstocenter", cCenterFlags, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
