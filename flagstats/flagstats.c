 /**************************************************************
 * Flag Statistics Module
 *
 * The following module handles player statistics every time
 * a jackpot is either won or reset.
 *
 * Current functionality includes:
 *
 * 1. Displaying active player's game statistics.
 * W=kills, L=deaths, TK=teamkills, FW=flagkills, FL=flagdeaths,
 * G=goals, FD=flagdrops, FT=flagtime, SC=shipchanges, KP=killpoints
 *
 * By: Zachary Read
 *
 * ************************************************************/

#include "asss.h"
#include "flagcore.h"
#include "jackpot.h"
#include "game.h"
#include "arenaman.h"

/* Player Data */
typedef struct Pdata
{
	char shipchanges;
	char played;
} Pdata;

local int playerKey;

/* Interfaces */
local Imodman *mm;
local Ichat *chat;
local Iarenaman *aman;
local Iplayerdata *pd;
local Imainloop *ml;
local Istats *stats;
local Ijackpot *jp;
local Igame *game;

local void FlagReset(Arena *arena, int freq, int points, int stat, int interval, Player *p, Target *target);

local int gametime = 0;
local int wintime = 0;

/* Get the current time. */
local void GetWintime(Arena *arena, int action)
{
	if ((action == AA_CREATE) && (wintime == 0)) //Make sure we don't redeclare wintime when arena is recreated.
	{
		wintime = current_millis();
	}
}

/* Set data->played if the player has changed ships.  */
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq)
{
	Pdata *data = PPDATA(p, playerKey);
	data->shipchanges++;
	if (p->p_ship != SHIP_SPEC)
	{
		data->played = 1;
	}
}

/* Set data->shipchanges to 0 when player enters the arena. */
local void PlayerAction(Arena *arena, Player *p, int action)
{
	if (action == PA_ENTERARENA)
	{
		Pdata *data = PPDATA(p, playerKey);
		data->shipchanges = 0;
	}
}

/* List stats. */
local void FlagReset(Arena *arena, int freq, int points, int stat, int interval, Player *p, Target *target)
{
	gametime = current_millis();
	
	if ((gametime - wintime) < 0)
		wintime = gametime;
	
	int ctsecs = (gametime - wintime) / 1000;
	int ctmins = ctsecs / 60;
	int chours = ctmins / 60;
	int csecs = ctsecs - (ctmins * 60);
	int cmins = ctmins - (chours * 60);
	
	Link *link;
	Player *g;
	int kills = 0, deaths = 0, tks = 0, fkills = 0, fdeaths = 0, goals = 0, flagdrops = 0, flagtime = 0, shipchanges = 0, killpoints = 0;
	int reward = jp->GetJP(arena) + 10;
	
	chat->SendArenaMessage(arena,"|-------------------------------------------------------------------------------|");
	chat->SendArenaMessage(arena,"| Reward: %-12d                                     Game Time: %03i:%02i:%02i |", reward, chours, cmins, csecs);
    chat->SendArenaMessage(arena,"|-------------------------------------------------------------------------------|");
    chat->SendArenaMessage(arena,"| Player                   W    L    TK   FW   FL   G   FD  FT   SC  KP         |");
    chat->SendArenaMessage(arena,"|-------------------------------------------------------------------------------|");
	
	pd->Lock();
	FOR_EACH_PLAYER(g)
	{
		Pdata *data = PPDATA(g, playerKey);
		/* Only list players who've played. */
		if ((data->played == 1 && g->arena == arena) || (g->p_ship != SHIP_SPEC && g->arena == arena))
		{
			kills = stats->GetStat(g, STAT_KILLS, INTERVAL_GAME);
			deaths = stats->GetStat(g, STAT_DEATHS, INTERVAL_GAME);
			tks = stats->GetStat(g, STAT_TEAM_KILLS, INTERVAL_GAME);
			fkills = stats->GetStat(g, STAT_FLAG_KILLS, INTERVAL_GAME);
			fdeaths = stats->GetStat(g, STAT_FLAG_DEATHS, INTERVAL_GAME);
			goals = stats->GetStat(g, STAT_BALL_GOALS, INTERVAL_GAME);
			flagdrops = stats->GetStat(g, STAT_FLAG_DROPS, INTERVAL_GAME);
			flagtime = stats->GetStat(g, STAT_FLAG_CARRY_TIME, INTERVAL_GAME);
	  
			shipchanges = data->shipchanges;
	  
			killpoints = stats->GetStat(g, STAT_KILL_POINTS, INTERVAL_GAME);
			  chat->SendArenaMessage(arena,"| %-24s %-4d %-4d %-4d %-4d %-4d %-3d %-3d %-4d %-3d %-10d |",
				  g->name, kills, deaths, tks, fkills, fdeaths, goals, flagdrops, flagtime, shipchanges, killpoints);
		  }
	}
	pd->Unlock();
	chat->SendArenaMessage(arena,"|-------------------------------------------------------------------------------|");
	wintime = current_millis();
	
	/* Reset Shipchanges and check if they've played (i.e. unspecced) */
	pd->Lock();
	FOR_EACH_PLAYER(g)
	{
		Pdata *data = PPDATA(g, playerKey);
		data->shipchanges = 0;
		data->played = 0;
	}
	pd->Unlock();
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

/* Module information (?modinfo) */
EXPORT const char info_flagstats[] = "Flagstats v1.1";

/* The entry point: */
EXPORT int MM_flagstats(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;

		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		stats = mm->GetInterface(I_STATS, ALLARENAS);
		jp = mm->GetInterface(I_JACKPOT, ALLARENAS);
		game = mm->GetInterface(I_GAME, ALLARENAS);

		if (!chat || !aman || !pd || !ml || !stats || !jp || !game) // check interfaces
		{
			// release interfaces if loading failed
			mm->ReleaseInterface(game);
			mm->ReleaseInterface(jp);
			mm->ReleaseInterface(stats);
			mm->ReleaseInterface(ml);
			mm->ReleaseInterface(pd);
			mm->ReleaseInterface(aman);
			mm->ReleaseInterface(chat);

			return MM_FAIL;
		}
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		// release interfaces
		mm->ReleaseInterface(game);
		mm->ReleaseInterface(jp);
		mm->ReleaseInterface(stats);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(chat);
		
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		// player data
		playerKey = pd->AllocatePlayerData(sizeof(Pdata));

		if (!playerKey)
			return MM_FAIL;
		
		// register callbacks
		mm->RegCallback(CB_FLAGRESET, FlagReset, arena);
		mm->RegCallback(CB_ARENAACTION, GetWintime, arena);
		mm->RegCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
		mm->RegCallback(CB_PLAYERACTION, PlayerAction, arena);

		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		// free player data
		pd->FreePlayerData(playerKey);
		
		// unregister callbacks
		mm->UnregCallback(CB_FLAGRESET, FlagReset, arena);
		mm->UnregCallback(CB_ARENAACTION, GetWintime, arena);
		mm->UnregCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
		mm->UnregCallback(CB_PLAYERACTION, PlayerAction, arena);
		
		return MM_OK;
	}
	return MM_FAIL;
}
