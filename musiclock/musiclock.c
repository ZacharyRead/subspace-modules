 /**************************************************************
 * MusicLock
 *
 * When a player or team picks up all the flags, this module
 *  will display a lvz, and announce to the arena that the
 *  teams are locked. If a player tries to change teams, it
 *  will place them back on their previous frequency.
 *
 * Arena setting:
 *
 * [ Lockjackpot ]
 *   ; Frequencies will be locked if the jackpot is over this
 *   ; number and someone picks up the flags (default 10M)
 *   Minimum = 10000000
 *
 * This module was originally developed for the zone Devastation,
 * consequently it would need to be modified for the flag
 * count, and for the lvz used (see the obj->Toggle lines).
 * You could switch out the obj->Toggle lines and use the arena
 * message lines instead.
 *
 * By: Zachary Read
 *
 **************************************************************/

#include "asss.h"
#include "config.h"
#include "objects.h"
#include "fg_wz.h"
#include "flagcore.h"
#include "game.h"
#include "jackpot.h"

typedef struct Pdata
{
	int oldfreq;
} Pdata;

local int pdkey;

typedef struct MyArenaData
{
	int state;
} MyArenaData;

local void PlayerAction(Player *p, int action, Arena *arena);
local void ArenaAction(Arena *arena, int action);
local void LockCheck(Arena *a, Player *p, int freq, int *points);
local void ChangeFreq(Player *p, int newship, int oldship, int newfreq, int oldfreq);

local int adkey;

/* Interfaces */
local Imodman *mm;
local Iobjects *obj;
local Ichat *chat;
local Iconfig *cfg;
local Iflagcore *flagcore;
local Iarenaman *aman;
local Igame *game;
local Ijackpot *jp;
local Iplayerdata *pd;

local int lockjackpot = 0;
local int jackpot = 0;

/************************************************************************/
/*                          Game Callbacks                              */
/************************************************************************/

/* If a player enters the game, make his current frequency his old frequency. */
local void PlayerAction(Player *p, int action, Arena *arena)
{
	MyArenaData *at = P_ARENA_DATA(p->arena, adkey);
	Pdata *d = PPDATA(p, pdkey);
	if (at->state == 1 && action == PA_ENTERARENA)
	{
		d->oldfreq = p->p_freq;
	}
	else
	{
		d->oldfreq = p->p_freq;
		if (at->state != 1)
		{
			at->state = 0;
		}
	}
}

/* When the arena is created, set the state to 0 */
local void ArenaAction(Arena *arena, int action)
{
	MyArenaData *at = P_ARENA_DATA(arena, adkey);
	if (action == AA_CREATE)
	{
		at->state = 0;
	}
}

/* Check the jackpot, check the flags, and lock/unlock the teams. */
local void LockCheck(Arena *a, Player *p, int freq, int *points)
{
	jackpot = jp->GetJP(a);
	lockjackpot = cfg->GetInt(a->cfg, "Lockjackpot", "Minimum", 10000000);

	MyArenaData *at = P_ARENA_DATA(a, adkey);

	if (jackpot > lockjackpot)
	{
		Target target;
		target.type = T_ARENA;
		target.u.arena = a;
		int freqflags = flagcore->CountFreqFlags(p->arena, p->p_freq);

		if (at->state == 0)
		{
			if (freqflags > 24)
			{
				obj->Toggle(&target, 321, 1);
//				chat->SendArenaMessage(a, "Teams have been locked!");

				at->state = 1;

				Arena *arena;
				Player *g;

				Link *link;
				pd->Lock();
				FOR_EACH_PLAYER(g)
				{
					if (g->arena == arena)
					{
						Pdata *d = PPDATA(g, pdkey);
						d->oldfreq = g->p_freq;
					}
				}
				pd->Unlock();

				return;
			}
		}
		else if (at->state == 1)
		{
			if (freqflags < 25)
			{
				obj->Toggle(&target, 321, 0);
//				chat->SendArenaMessage(a, "Teams are now unlocked.");

				at->state = 0;

				return;
			}
		}
	}
	else if ((jackpot < lockjackpot) && (at->state == 1))
	{
		Target target;
		target.type = T_ARENA;
		target.u.arena = a;

		obj->Toggle(&target, 321, 0);
//		chat->SendArenaMessage(a, "Teams are now unlocked.");

		at->state = 0;
	}
	else
		return;
}

/* When a player changes teams, check if the state of the game is locked. */
local void ChangeFreq(Player *p, int newship, int oldship, int newfreq, int oldfreq)
{
	Pdata *d = PPDATA(p, pdkey);
	MyArenaData *at = P_ARENA_DATA(p->arena, adkey);

	if (!d->oldfreq)
	{
		d->oldfreq = 0;
	}

	if (at->state == 1)
	{
		int newfreq = p->p_freq;
		if ((newfreq != d->oldfreq) && (d->oldfreq != p->arena->specfreq) && (newfreq != p->arena->specfreq))
		{
			game->SetFreq(p, d->oldfreq);
			chat->SendMessage(p, "Notice: you can only play on team %i.", d->oldfreq);
		}
	}
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

/* Module information (?modinfo) */
EXPORT const char info_musiclock[] = "MusicLock v1.3";

/* The entry point: */
EXPORT int MM_musiclock(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		obj = mm->GetInterface(I_OBJECTS, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		flagcore = mm->GetInterface(I_FLAGCORE, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		game = mm->GetInterface(I_GAME, ALLARENAS);
		jp = mm->GetInterface(I_JACKPOT, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		
		// check interfaces
		if (!chat || !obj || !cfg || !flagcore || !aman || !game || !jp || !pd)
		{
			// release interfaces
			mm->ReleaseInterface(pd);
			mm->ReleaseInterface(jp);
			mm->ReleaseInterface(game);
			mm->ReleaseInterface(aman);
			mm->ReleaseInterface(flagcore);
			mm->ReleaseInterface(cfg);
			mm->ReleaseInterface(obj);
			mm->ReleaseInterface(chat);
			
			return MM_FAIL;
		}
		
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		// release interfaces
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(jp);
		mm->ReleaseInterface(game);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(flagcore);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(obj);
		mm->ReleaseInterface(chat);
		
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		adkey = aman->AllocateArenaData(sizeof(struct MyArenaData));
		pdkey = pd->AllocatePlayerData(sizeof(struct Pdata));
		
		if (adkey == -1 || pdkey == -1)
			return MM_FAIL;
		
		// register callbacks
		mm->RegCallback(CB_FLAGGAIN, LockCheck, arena);
		mm->RegCallback(CB_FLAGLOST, LockCheck, arena);
		mm->RegCallback(CB_SHIPFREQCHANGE , ChangeFreq, arena);
		mm->RegCallback(CB_PLAYERACTION, PlayerAction, arena);
		mm->RegCallback(CB_ARENAACTION, ArenaAction, arena);
		
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		// free data
		aman->FreeArenaData(adkey);
		pd->FreePlayerData(pdkey);

		// unregister callbacks
		mm->UnregCallback(CB_FLAGGAIN, LockCheck, arena);
		mm->UnregCallback(CB_FLAGLOST, LockCheck, arena);
		mm->UnregCallback(CB_SHIPFREQCHANGE , ChangeFreq, arena);
		mm->UnregCallback(CB_PLAYERACTION, PlayerAction, arena);
		mm->UnregCallback(CB_ARENAACTION, ArenaAction, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
