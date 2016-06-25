 /**************************************************************
 * Jackpot Announcer
 *
 * This module will announce the Jackpot after a defined threshold
 * The module was originally designed for the zone Devastation.
 *
 * By: Zachary Read
 *
 * ************************************************************/

#include "asss.h"
#include "jackpot.h"
#include "chatbot.h"
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
local Ijackpot *jp;
local Iobjects *obj;

/* Arena data */
typedef struct Adata
{
	int jpBroadcastThreshold;
	int lastJP;
}

/************************************************************************/
/*                          Game Callbacks                              */
/************************************************************************/

/* Announce the jackpot every so many points. */
local void cKill(Arena *arena, Player *k, Player *p, int bounty, int flags, int pts, int green)
{
	int jackpot = jp->GetJP(arena);
	Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
	if (jackpot > adata->lastJP)
	{
		int old = adata->lastJP / adata->jpBroadcastThreshold;
		int new = jackpot / adata->jpBroadcastThreshold;

		if (old != new && old < adata->jpBroadcastThreshold * 1)
		{
			adata->lastJP = jackpot;
			chat->SendArenaSoundMessage(arena, 2, "Jackpot is now over %i million points!", jackpot / 1000000);
		}
	}
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_jpannouncer[] = "Jackpot Announcer v0.5";

EXPORT int MM_jpannouncer(int action, Imodman *mm_, Arena *arena)
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
		jp = mm->GetInterface(I_JACKPOT, ALLARENAS);
		obj = mm->GetInterface(I_OBJECTS, ALLARENAS);

		if (!aman || !chat || !pd || !flags || !cmd || !cfg || !jp || !obj)
			return MM_FAIL;
		
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(obj);
		mm->ReleaseInterface(jp);
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
		mm->RegCallback(CB_KILL, cKill, arena);
		arenaKey = aman->AllocateArenaData(sizeof(Adata));
		Adata *adata = P_ARENA_DATA(arena, arenaKey);
		adata->jpBroadcastThreshold = cfg->GetInt(arena->cfg, "DevaJunk", "JPBroadcastThreshold", 3000000);

		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		aman->FreeArenaData(arenaKey);
		mm->UnregCallback(CB_KILL, cKill, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
