 /**************************************************************
 * Anti Hack Module
 *
 * The Anti Hack module will periodically check if a player's
 *  timer drift is off. This could mean one of three things:
 *	  1) He's cheating
 *	  2) He's in an odd time zone (such as Newfoundland)
 *	  3) Lagging really really badly.
 * As such, the moderator should verify if the idle time
 *  is also above zero.
 *
 * Per-Arena Setting:
 *
 * [ Antihack ]
 *  Action = 0
 * ; 0 = nothing (default), 1 = spec, 2 = spec lock, 3 = kick
 *  Time = 1200
 * ; in milliseconds (e.g. 1200 = 12 seconds)
 *
 * By: Zachary Read
 *
 * ************************************************************/

#include <stdio.h>
#include <string.h>

#include "asss.h"
#include "idle.h"

/* Interfaces */
local Iplayerdata *pd;
local Ilogman *lm;
local Ichat *chat;
local Ilagquery *lagq;
local Inet *net;
local Igroupman *groupman;
local Iidle *idle;
local Imainloop *ml;
local Iarenaman *aman;
local Iconfig *cfg;
local Igame *game;

/* This will check if the player's timer drift and idle time are off. */
local void Einfo(Player *t)
{
	int i, drift = 0, count = 0;
	struct TimeSyncHistory history;

	if (IS_STANDARD(t))
	{
		lagq->QueryTimeSyncHistory(t, &history);
		for (i = 1; i < TIME_SYNC_SAMPLES; i++)
		{
			int j = (i + history.next) % TIME_SYNC_SAMPLES;
			int k = (i + history.next - 1) % TIME_SYNC_SAMPLES;
			int delta = (history.servertime[j] - history.clienttime[j]) -
						(history.servertime[k] - history.clienttime[k]);
			if (delta >= -10000 && delta <= 10000)
			{
				drift += delta;
				count++;
			}
		}
	}
	
	if (count)
	{
		if ((drift/count != 0) && ((drift/count < -25) || (drift/count > 25)) && (t->p_ship != SHIP_SPEC))
		{
			chat->SendModMessage("Warning (possible cheating): [%s] Idle: %d s  Timer drift: %d",
				t->name,
				idle ? idle->GetIdle(t) : -1,
				count ? drift/count : 0);
			
			/* 0=do nothing, 1=spec, 2=spec lock, 3=kick */
			int action = cfg->GetInt(t->arena->cfg, "Antihack", "Action", 0);
			if (action == 1)
			{
				if (t->p_freq != t->arena->specfreq)
				{
					game->SetShipAndFreq(t, SHIP_SPEC, t->arena->specfreq);
				}
				return;
			}
			else if (action == 2)
			{
				Target target;
				target.type = T_PLAYER;
				target.u.p = t;
				game->Lock(&target, 0, 1, 0);
				return;
			}
			else if (action == 3)
			{
				pd->KickPlayer(t);
				return;
			}
		}
	}
}

/* Every ten seconds, verify if a player is cheating. */
local int diffcheck(void *arena_)
{
	Arena *arena = arena_;
	Player *g;
	
	Link *link;
	pd->Lock();
	FOR_EACH_PLAYER(g)
	{
		if ((g->arena == arena) && (g->type != T_FAKE))
		{
			Einfo(g);
		}
	}
	pd->Unlock();
	
	return 1;
}

local void ArenaAction(Arena *arena, int action)
{
	if (action == AA_CREATE)
	{
		/* Start Timers */
		int time = cfg->GetInt(arena->cfg, "Antihack", "Time", 1200);
		ml->SetTimer(diffcheck, time, time, arena, arena);
	}
	else if (action == AA_DESTROY)
	{
		ml->ClearTimer(diffcheck, arena);
	}
}

/* Module information (?modinfo) */
EXPORT const char info_antihack[] = "Antihack v1.3";

EXPORT int MM_antihack(int action, Imodman *mm, Arena *arena)
{
	if (action == MM_LOAD)
	{
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		lm = mm->GetInterface(I_LOGMAN, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		lagq = mm->GetInterface(I_LAGQUERY, ALLARENAS);
		net = mm->GetInterface(I_NET, ALLARENAS);
		groupman = mm->GetInterface(I_GROUPMAN, ALLARENAS);
		idle = mm->GetInterface(I_IDLE, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		game = mm->GetInterface(I_GAME, ALLARENAS);
		
		if (!pd || !lm || !chat || !lagq || !net || !groupman || !idle || !ml || !aman || !cfg || !game) // check interfaces
		{
			/* Release interfaces */
			mm->ReleaseInterface(pd);
			mm->ReleaseInterface(lm);
			mm->ReleaseInterface(chat);
			mm->ReleaseInterface(lagq);
			mm->ReleaseInterface(net);
			mm->ReleaseInterface(groupman);
			mm->ReleaseInterface(idle);
			mm->ReleaseInterface(ml);
			mm->ReleaseInterface(aman);
			mm->ReleaseInterface(cfg);
			mm->ReleaseInterface(game);
			
			return MM_FAIL;
		}
		else
		{
			mm->RegCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);
			  return MM_OK;
		}
	}
	else if (action == MM_UNLOAD)
	{
		/* Clear Timers */
		ml->ClearTimer(diffcheck, NULL);
		
		mm->UnregCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);
		
		/* Release interfaces */
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(lm);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(lagq);
		mm->ReleaseInterface(net);
		mm->ReleaseInterface(groupman);
		mm->ReleaseInterface(idle);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(game);
		return MM_OK;
	}
	else
		return MM_FAIL;
}
