 /**************************************************************
 * Event: Jugger
 *
 * Handles everything to do with the Juggernaut event
 *  for Devastation.
 *
 * All players start off on freq 0. A player must then find the
 * flag to become the juggernaut. If the juggernaut eliminates
 * a specified amount of opponents, he wins. Should he/she happen
 * to die, then the new player becomes the juggernaut.
 *
 * Requirements
 *   The arena must have one defined flag in its settings that
 *     players can carry.
 *
 * To start the event, a moderator just needs to type ?start jugger
 * (some options are available). Typing ?stop will cancel the event.
 *
 * Based on a plugin originally designed by user XDOOM for
 * Deva-bot, recreated by Zachary Read for the ASSS server.
 *
 **************************************************************/

#include "asss.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define JUGGER_FREQ 100
#define HUMAN_FREQ 0

/* Player data */
typedef struct Pdata
{
	int kills;        //the number of kills a player has acquired
} Pdata;

local int playerKey;

/* Arena data */
typedef struct Adata
{
    int rkill;         //the required number of kills to win
	int lockships;     //0 = no, 1 = ships are restricted
	int started;       //0 = no game, 1 = pending, 2 = started
	int defaultship;   //this is the default ship if someone happens to change to a restricted ship
	int jship;         //the jugger's ship
	int jlockship;     //0 = no, 1 = jugger's ship is restricted
} Adata;

local int arenaKey;

/* Interfaces */
local Imodman *mm;
local Iarenaman *aman;
local Icapman *capman;
local Ichat *chat;
local Icmdman *cmd;
local Iflagcore *flags;
local Igame *game;
local Imainloop *ml;
local Iplayerdata *pd;

local int allships[7];

/************************************************************************/
/*                              Prototypes                              */
/************************************************************************/
//interface functions
local char* getOption(const char *string, char param);

//game functions
local void Abort(Arena *arena, Player *host, int debug);
local void Begin(Player *host, Arena *arena, const char *params);
local void LegalShip(int ship, Arena *arena);
local void CheckLegalShip(Arena *arena);
local int TimeUp(void *p);
local void LCheck(Arena *arena, Player *pe);
local void Check(Player *p);
local void HideFlag(Arena *arena);
local void ShowFlag(Arena *arena);
local void TransferFlag(Player *p);
local void CheckPlayers(Arena *arena);
local void Stop(Arena* arena);

//callbacks
local void Kill(Arena *arena, Player *k, Player *p, int bounty, int flags, int pts, int green);
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq);
local void PlayerAction(Player *p, int action, Arena *arena);
local void Flaglost(Arena *arena, Player *p);
local void Flaggain(Arena *arena, Player *p);


/************************************************************************/
/*                          Interface Functions                         */
/************************************************************************/

local char* getOption(const char *string, char param)
{
    if (!param)
        return NULL;

    char search[4] = "-d(\0";
    search[1] = param;

    char *buf = strstr(string,search);
    if (buf)
    {
        char *result = calloc(64, sizeof(char));
        int i = 3, j = 0;
        for(; ((i < 67) && (j == 0)); i++)
        {
            if (buf[i] != ')')
            {
                result[i-3] = buf[i];
            }
            else
            {
                j = 1;
            }
        }

        result[i-2] = '\0';
        return result;
    }
    else
        return "";
}

/************************************************************************/
/*                            Game Functions                           */
/************************************************************************/

local void Abort(Arena *arena, Player *host, int debug)
{
    /* Reset values */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    adata->started = 0;
    adata->rkill = 0;
    adata->lockships = 0;
    adata->jship = 0;
    adata->jlockship = 0;

    chat->SendMessage(host, "Game aborted: Invalid syntax. Please type '?start' for more help.");
}

local void Begin(Player *host, Arena *arena, const char *params)
{
    /* Set the game as started */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    if (adata->started >= 1)
    {
        chat->SendMessage(host, "There is already a game running. Please use ?stop to end the current game.");
        return;
    }
    else
        adata->started = 1;

    /* Get Game Options*/
    //kills
    int rkills;
    char *next, *string;
    string = getOption(params, 'k');

    if (string != next)
    {
        rkills = atoi(string);
        if ((rkills <= 50) && (rkills > 0))
            adata->rkill = rkills;
        else
        {
            Abort(arena, host, 1);
            return;
        }
    }
    else
    {
        Abort(arena, host, 2);
        return;
    }

    //ships
    string = getOption(params, 's');

    int ii;
    for (ii = 0; ii <= 8; ii++)
    {
        allships[ii] = 0; //reset every ship
    }


    int length = strlen(string);

    if ((string != next) && (string) && (length >= 1))
    {
        if ((length % 2 == 0)  || (length > 15) || (!length))
        {
            Abort(arena, host, 3);
            return;
        }
        else
        {
            int i;
            for (i = 0; i < length; i++)
            {
                if (string[i] != ',');
                {
                    char first[64];
                    //strncpy(first, string + i, 60);
                    snprintf(first, sizeof(first) - 10, "%s", string + i * 2);
                    first[1] = '\0';

                    int legal = atoi(first) - 1;
                    if (legal == -1)
                        break;

                    if (legal < 0 || legal > 7)
                    {
                        Abort(arena, host, 4);
                        return;
                    }
                    else
                    {
                        LegalShip(legal, arena);
                    }
                    first[0] = '\0';
                    //free(first);
                }
            }
            char first[64];
            strncpy(first, string, 4);
            first[1] = '\0';
            adata->defaultship = atoi(first) - 1;
            CheckLegalShip(arena);

            adata->lockships = 1;
        }
    }
    else
    {
        for (ii = 0; ii < 8; ii++)
        {
            allships[ii] = 1; //all ships are legal
        }
        adata->lockships = 0;
    }
    
    chat->SendArenaSoundMessage(arena, 2, "Jugger will start in 20 seconds, enter if playing.");
    if (adata->lockships)
        chat->SendArenaMessage(arena, "Allowed ships: %s", string);
    
    //jugger's ship
    string = getOption(params, 'j');
    int jship;
    
    length = strlen(string);

    if ((string != next) && (string) && (length >= 1))
    {
        jship = atoi(string);
        if ((jship < 9) && (jship > 0))
        {
            adata->jship = jship - 1;
            adata->jlockship = 1;
        }
        else
        {
            Abort(arena, host, 1);
            return;
        }
    }
    else
    {
        adata->jlockship = 0;
    }
    
    if (adata->jlockship)
        chat->SendArenaMessage(arena, "Jugger ship: %i", adata->jship + 1);

    CheckLegalShip(arena);
    
    //Hide the Flag
    HideFlag(arena);

    adata->started = 1;
    /* Start the timer */
    ml->SetTimer(TimeUp, 2000, 2000, host, NULL);
}

/* Set as legal ship  */
local void LegalShip(int ship, Arena *arena)
{
    //chat->SendArenaMessage(arena, "Legal ship : %i", ship);
    allships[ship] = 1;
}

/* Check if the player is in a legal ship */
local void CheckLegalShip(Arena *arena)
{
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    if (!adata->lockships)
        return;

    int defaultship = adata->defaultship;

    Player *g;
    Link *link;
    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
        if ((g->arena == arena) && (g->p_ship != SHIP_SPEC) && (g->p_freq != JUGGER_FREQ))
        {
            int i, legal = 0;
            for (i = 0; i < 8; i++)
            {
                if ((g->p_ship == i) && (allships[i] == 1))
                    legal = 1;
            }
            if (!legal)
                game->SetShip(g, defaultship);
        }
        else if ((g->arena == arena) && (g->p_ship != SHIP_SPEC) && (g->p_freq == JUGGER_FREQ))
        {
            if ((adata->jlockship) && (g->p_ship != adata->jship))
            {
                game->SetShip(g, adata->jship);
            }
        }
    }
    pd->Unlock();
}

/* After 20 seconds have passed */
local int TimeUp(void *p)
{
    Player *host = p;
    Arena *arena = host->arena;

    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    if (adata->started == 0)
        return 0;

    //set the game to started
    adata->started = 2;
    
    Player *g;
    Link *link;
    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
        if (g->arena == arena)
        {
            if (g->p_ship != SHIP_SPEC)
            {
                game->SetFreq(g, HUMAN_FREQ); //place everyone on team 0
                Pdata *pdata = PPDATA(g, playerKey);
                pdata->kills = 0;

                Target target;
                target.type = T_PLAYER;
                target.u.p = g;
                game->GivePrize(&target, 7, 1); //warp everyone
            }
        }
    }
    pd->Unlock();

    CheckLegalShip(arena);
    
    //Reveal the flag location
    ShowFlag(arena);

    chat->SendArenaSoundMessage(arena, 104, "Juggernaut has started! The first person to get %i %s as the juggernaut wins!", adata->rkill, adata->rkill == 1 ? "kill" : "kills");

    //register callbacks
    mm->RegCallback(CB_KILL, Kill, arena);
    mm->RegCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->RegCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->RegCallback(CB_FLAGGAIN, Flaggain, arena);
    mm->RegCallback(CB_FLAGLOST, Flaglost, arena);
    return 0;
}

/* Check if players have left the arena or specced. */
local void LCheck(Arena *arena, Player *pe)
{
	Player *winner;
	
    Player *p;
    Link *link;
    int i = 0;
    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
        if ((p->arena == arena) && (p->p_ship != SHIP_SPEC) && (p!=pe))
        {
            winner = p;
            i++;
        }
    }
    pd->Unlock();
    if (i == 1)
    {
        chat->SendArenaSoundMessage(arena, 5, "Game Over! This round's winner is %s.", winner->name);
        Stop(arena);
    }
    else if (i == 0)
    {
        chat->SendArenaSoundMessage(arena, 1, "Game Aborted: There are no players left in the game.");
        Stop(arena);
    }
}

/* If the juggernaut has won. */
local void Check(Player *p)
{
    Pdata *pdata = PPDATA(p, playerKey);
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
	
	if (!pdata->kills)
		pdata->kills = 0;

    if (pdata->kills == adata->rkill)
    {
        chat->SendArenaSoundMessage(p->arena, 5, "Game over! %s was the fastest killer as juggernaut and is the juggernaut winner!", p->name);
        Stop(p->arena);
    }
}

/* Hide Flag */
local void HideFlag(Arena *arena)
{
    FlagInfo fi;
    flags->GetFlags(arena, 0, &fi, 1);
    //fi.state = FI_NONE;
    if (fi.state == FI_CARRIED)
    {
        fi.state = FI_NONE;
        fi.carrier = NULL;
        flags->SetFlags(arena, 0, &fi, 1);
    }
    fi.carrier = NULL;
    fi.x = 100;
    fi.y = 100;
    fi.freq = -1;
    flags->SetFlags(arena, 0, &fi, 1);
}

/* Show Flag */
local void ShowFlag(Arena *arena)
{
    FlagInfo fi;
    flags->GetFlags(arena, 0, &fi, 1);
    //fi.state = FI_NONE;
    fi.freq = -1;
    fi.carrier = NULL;
    
    srand(time(NULL));
    fi.x = rand() %225 + 400;
    fi.y = rand() %200 + 425;
    if (fi.y > 603)
        fi.y = 440;
    flags->SetFlags(arena, 0, &fi, 1);
}

/* Transfer the flag to this player */
local void TransferFlag(Player *p)
{
    FlagInfo fi; 
    flags->GetFlags(p->arena, 0, &fi, 1);
    fi.state = FI_CARRIED;
    fi.carrier = p;
    flags->SetFlags(p->arena, 0, &fi, 1); 
}

/* Check if players are on legal frequencies */
local void CheckPlayers(Arena *arena)
{
    Link *link;
    Player *p;
    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
        if (p->arena == arena)
        {
            if ((p->p_freq != HUMAN_FREQ) && (p->p_freq != JUGGER_FREQ) && (p->p_ship != SHIP_SPEC))
            {
                game->SetFreq(p, HUMAN_FREQ);
            }
        }
    }
    pd->Unlock();
}

/* End the current game */
local void Stop(Arena* arena)
{
    Player *p;
    Link *link;

    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
		if (p->arena == arena)
		{
			/* Warp all players */
			Target target;
			target.type = T_PLAYER;
			target.u.p = p;
			game->GivePrize(&target, 7, 1);
			
			Pdata *pdata = PPDATA(p, playerKey);
			pdata->kills = 0;
		}
    }
    pd->Unlock();

    /* Reset arena data */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    adata->started = 0;
    adata->rkill = 0;
    adata->lockships = 0;

    /* Clear timers and unregister callbacks */
    ml->ClearTimer(TimeUp, arena);
    mm->UnregCallback(CB_KILL, Kill, arena);
    mm->UnregCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->UnregCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->UnregCallback(CB_FLAGGAIN, Flaggain, arena);
    mm->UnregCallback(CB_FLAGLOST, Flaglost, arena);
}

/************************************************************************/
/*                           Game Callbacks                             */
/************************************************************************/

/* Add to juggernaut's kills, or change juggernaut. */
local void Kill(Arena *arena, Player *k, Player *p, int bounty, int flags, int pts, int green)
{
    CheckPlayers(arena);
    
    /* IF JUGGER */
    if (k->p_freq == JUGGER_FREQ)
    {
        /* Let's add to the number of kills the juggernaut has. */
        Pdata *pdata = PPDATA(k, playerKey);
		
		if (!pdata->kills)
			pdata->kills = 0;
		
        pdata->kills++;

        Check(k);
    }
    /* IF HUMAN */
    else if (k->p_freq == HUMAN_FREQ)
    {
        game->SetFreq(k, JUGGER_FREQ);
        game->SetFreq(p, HUMAN_FREQ);
        CheckLegalShip(arena);
        
        TransferFlag(k);
        
        /* Announce the new juggernaut. */
        Pdata *pdata = PPDATA(k, playerKey);
        Adata *adata = P_ARENA_DATA(k->arena, arenaKey);
        
        int left = adata->rkill - pdata->kills;
        chat->SendArenaSoundMessage(p->arena, 2, "%s just killed the juggernaut and is now the new juggernaut!", k->name);
        chat->SendArenaSoundMessage(p->arena, 2, "%s only needs %i more %s to win!", k->name, left, left == 1 ? "kill" : "kills");
        Check(k);
    }
}

/* Check if a player spectates the game. */
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq)
{
    CheckPlayers(p->arena);
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
    
    if (p->p_ship == SHIP_SPEC)
    {
        LCheck(p->arena, p);
    }
    else if (p->p_freq == JUGGER_FREQ)
    {
        if ((adata->jlockship) && (p->p_ship != adata->jship))
        {
            game->SetShip(p, adata->jship);
            TransferFlag(p);
        }
    }
    else
    {
        if (!adata->lockships)
            return;

        int i, legal = 0;
        for (i = 0; i < 8; i++)
        {
            if ((p->p_ship == i) && (allships[i] == 1))
                legal = 1;
        }
        if (!legal)
        {
            if (oldship == SHIP_SPEC)
            {
                Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
                int defaultship = adata->defaultship;
                game->SetShip(p, defaultship);
            }
            else
                game->SetShip(p, oldship);
        }
    }
}

/* Check if a player leaves the arena. */
local void PlayerAction(Player *p, int action, Arena *arena)
{
	if (!p->arena)
		return;
    //CheckPlayers(arena);
	
    if ((action == PA_DISCONNECT) || (action == PA_LEAVEARENA))
	{
        LCheck(arena, p);
	}
	
    /* Send a new player the status of the game. */
    if ((action == PA_ENTERARENA) || (action == PA_ENTERGAME))
    {
		if (p->arena == arena && playerKey)
		{
			//chat->SendArenaSoundMessage(arena, 5, "DEBUG 003");
			Pdata *pdata = PPDATA(p, playerKey);
			if (!pdata)
				return;
			
			Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
			pdata->kills = 0;
			chat->SendSoundMessage(p, 26, "We are playing Jugger to %i %s.", adata->rkill, adata->rkill == 1 ? "kill" : "kills");
		}
    }
}

/* Handling flag loss */
local void Flaglost(Arena *arena, Player *p)
{
}

/* Handling flag gain */
local void Flaggain(Arena *arena, Player *p)
{
    if (p->p_freq != JUGGER_FREQ)
    {
        Link *link;
        Player *g;
        pd->Lock();
        FOR_EACH_PLAYER(g)
        {
            if ((g->arena == arena) && (g->p_freq == JUGGER_FREQ))
            {
                game->SetFreq(g, HUMAN_FREQ); //put the former juggernaut on human freq
            }
        }
        pd->Unlock();
        game->SetFreq(p, JUGGER_FREQ);
        CheckLegalShip(arena);
        TransferFlag(p);
        
        Pdata *pdata = PPDATA(p, playerKey);
        Adata *adata = P_ARENA_DATA(p->arena, arenaKey);

        int left = adata->rkill - pdata->kills;
        chat->SendArenaSoundMessage(p->arena, 2, "%s found the flag and is now the new Juggernaut!", p->name);
        chat->SendArenaSoundMessage(p->arena, 2, "%s only needs %i more %s to win!", p->name, left, left == 1 ? "kill" : "kills");
        Check(p);
    }
}

/************************************************************************/
/*                          Player Commands                             */
/************************************************************************/

/* ?host help information */
local helptext_t host_help =
"Targets: none\n"
"Args: request, none, event\n"
"By default, ?host <msg> will send a message to any online moderator requesting\n"
"that an event be hosted. If the player using this command is a staff member,\n"
"and if no parameters are given, ?host will list the available events in that arena.\n"
"If an event is specified (e.g. ?host elim), it will attempt to start that event.\n";

/* ?host (staff message, event list, start an event) */
local void cHost(const char *command, const char *params, Player *p, const Target *target)
{
    /* Send a host message if the player isn't a staff member. */
    if (!capman->HasCapability(p,CAP_IS_STAFF))
    {
        if (IS_ALLOWED(chat->GetPlayerChatMask(p), MSG_MODCHAT))
	    {
            Arena *arena = p->arena;
            if (params)
            {
    	    	chat->SendModMessage("(Host) {%s} %s: %s", arena->name, p->name, params);
	       	    chat->SendMessage(p, "Message has been sent to online staff");
            }
            else
            {
                chat->SendMessage(p, "Invalid syntax. Please use ?host <arena/event> to request an event be hosted.");
            }
		    return;
	    }
    }

    /* Host list or an event if the player is a staff member. */
    if (!isalpha(params[0]) && params[0] != '-')
    {
        chat->SendMessage(p, "-------------------------------------------------------------------");
        chat->SendMessage(p, "| Jugger | Who will be the juggernaught to rule them all?         |");
        chat->SendMessage(p, "-------------------------------------------------------------------");
        chat->SendMessage(p, "Parameters: kills: -k(#)");
        chat->SendMessage(p, "            ships: -s(#)");
        chat->SendMessage(p, "      jugger ship: -j(#)");
        chat->SendMessage(p, "Example: ?start jugger -k(5) -s(1,2,3) -j(1)");
	}
	else
	{
        Begin(p, p->arena, params);
	}
}

/* ?stopevent help information */
local helptext_t stop_help =
"Targets: none\n"
"Args: none\n"
"Using ?stop or ?stopevent will end the game or event currently active in the arena.\n";

/* ?stopevent */
local void cStopEvent(const char *command, const char *params, Player *p, const Target *target)
{
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
    if (adata->started != 0)
    {
        chat->SendArenaSoundMessage(p->arena, 26, "Game of jugger aborted!");
        Stop(p->arena);
    }
    else
    {
        chat->SendMessage(p, "There does not appear to be a game going on here.");
    }
}

/* ?rules help information */
local helptext_t rules_help =
"Targets: none\n"
"Args: none\n"
"If a game is currently in play, this command will display the rules.\n";

/* Show the rules of the game to the player. */
local void cRules(const char *command, const char *params, Player *p, const Target *target)
{
	chat->SendMessage(p, "Who will be the juggernaught to rule them all?");
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_jugger[] = "(Devastation) v0.1 Hakaku";

EXPORT int MM_jugger(int action, Imodman *mm_, Arena *arena)
{
    if (action == MM_LOAD)
    {
        return MM_OK;
    }
    else if (action == MM_UNLOAD)
    {
        return MM_OK;
    }
    else if (action == MM_ATTACH)
    {
        mm = mm_;

		aman = mm->GetInterface(I_ARENAMAN, arena);
		capman = mm->GetInterface(I_CAPMAN, arena);
        chat = mm->GetInterface(I_CHAT, arena);
        cmd = mm->GetInterface(I_CMDMAN, arena);
        flags = mm->GetInterface(I_FLAGCORE, arena);
        game = mm->GetInterface(I_GAME, arena);
		ml = mm->GetInterface(I_MAINLOOP, arena);
		pd = mm->GetInterface(I_PLAYERDATA, arena);

        if (!aman || !chat || !cmd || !flags || !game || !ml || !pd)
		{
		    mm->ReleaseInterface(pd);
            mm->ReleaseInterface(ml);
            mm->ReleaseInterface(game);
            mm->ReleaseInterface(flags);
            mm->ReleaseInterface(cmd);
            mm->ReleaseInterface(chat);
            mm->ReleaseInterface(capman);
            mm->ReleaseInterface(aman);
            return MM_FAIL;
		}
        else
        {
            playerKey = pd->AllocatePlayerData(sizeof(Pdata));
			arenaKey  = aman->AllocateArenaData(sizeof(Adata));

            if ((!playerKey)  || (!arenaKey))
            {
                mm->ReleaseInterface(pd);
                mm->ReleaseInterface(ml);
                mm->ReleaseInterface(game);
                mm->ReleaseInterface(flags);
                mm->ReleaseInterface(cmd);
                mm->ReleaseInterface(chat);
				mm->ReleaseInterface(capman);
                mm->ReleaseInterface(aman);
                return MM_FAIL;
            }
            else
            {
				cmd->AddCommand("host", cHost, arena, host_help);
                cmd->AddCommand("start", cHost, arena, host_help);
                cmd->AddCommand("stopevent", cStopEvent, arena, stop_help);
                cmd->AddCommand("stop", cStopEvent, arena, stop_help);
                cmd->AddCommand("rules", cRules, arena, rules_help);

                return MM_OK;
            }
        }
    }
    else if (action == MM_DETACH)
    {
        pd->FreePlayerData(playerKey);
		aman->FreeArenaData(arenaKey);

        cmd->RemoveCommand("rules", cRules, arena);
        cmd->RemoveCommand("stop", cStopEvent, arena);
        cmd->RemoveCommand("stopevent", cStopEvent, arena);
        cmd->RemoveCommand("start", cHost, arena);
        cmd->RemoveCommand("host", cHost, arena);

        mm->ReleaseInterface(pd);
        mm->ReleaseInterface(ml);
        mm->ReleaseInterface(game);
        mm->ReleaseInterface(flags);
        mm->ReleaseInterface(cmd);
        mm->ReleaseInterface(chat);
		mm->ReleaseInterface(capman);
        mm->ReleaseInterface(aman);

        return MM_OK;
    }

    return MM_FAIL;
}
