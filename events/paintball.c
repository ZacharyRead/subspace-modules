 /**************************************************************
 * Event: Paintball (soccer)
 *
 * Handles everything to do with the Paintball (aka soccer or
 *  powerball) event for Devastation.
 *
 * Two goals, two teams. First team to get X amount of goals wins.
 *
 * Requirements:
 *  The map must have a goal on each side of the map.
 *  The arena must be configured to have two teams with opposing goals.
 *
 * To start the event, a moderator just needs to type ?start paintball
 * (some options are available). Typing ?stop will cancel the event.
 *
 * Based on a plugin originally designed by user XDOOM for
 * Deva-bot, recreated by Zachary Read for the ASSS server.
 *
 **************************************************************/

#include "asss.h"
#include "clientset.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Player data */
typedef struct Pdata
{
	int score;
	int kills;
	int deaths;
} Pdata;

local int playerKey;

/* Arena data */
typedef struct Adata
{
    int blue;          //blue team has this many goals
    int green;         //green team has this many goals
    int goals;         //number of goals a team needs to win
	int lockships;     //0 = no, 1 = ships are restricted
	int started;       //0 = no game, 1 = pending, 2 = started
	int defaultship;   //this is the default ship if someone happens to change to a restricted ship
} Adata;

local int arenaKey;

/* Interfaces */
local Imodman *mm;
local Iarenaman *aman;
local Iballs *balls;
local Icapman *capman;
local Iconfig *cfg;
local Ichat *chat;
local Icmdman *cmd;
local Iclientset *cs;
local Igame *game;
local Imainloop *ml;
local Iplayerdata *pd;

local override_key_t ok_Doors;

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
local void Stop(Arena* arena);

//callbacks
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq);
local void PlayerAction(Player *p, int action, Arena *arena);
local void Check(Arena *arena, Player *p, int bid, int x, int y);

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
    adata->blue = 0;
    adata->green = 0;
    adata->goals = 0;
    adata->lockships = 0;
    
    chat->SendMessage(host, "Game aborted: Invalid syntax. Please type '?start' for more help.");
    //chat->SendMessage(host, "Debug: %i", debug);
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
    //goals
    int goals;
    char *next, *string;
    string = getOption(params, 'g');
    
    if (string != next)
    {
        goals = atoi(string);
        if ((goals <= 15) && (goals > 0))
            adata->goals = goals;
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
        allships[ii] = 0; //reset everyship
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
                    
                    //chat->SendMessage(host, "Ship %s", first);

                    int legal = atoi(first) - 1;
                    if (legal == -1)
                        break;
                    
                    if (legal < 0 || legal > 7)
                    {
                        //chat->SendMessage(host, "Legal %i", legal);
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
    
    /* Close the doors */
    Player *d;
    Link *link;

    cs->ArenaOverride(arena, ok_Doors, 255);

    pd->Lock();
    FOR_EACH_PLAYER(d)
    {
        cs->SendClientSettings(d);
    }
    pd->Unlock();
    
    chat->SendArenaSoundMessage(arena, 2, "Paintball will start in 10 seconds, get ready!");
    if (adata->lockships)
        chat->SendArenaMessage(arena, "Allowed ships: %s", string);
    
    CheckLegalShip(arena);
    
    adata->started = 1;
    /* Start the timer */
    ml->SetTimer(TimeUp, 1000, 1000, host, NULL);
}

/* Set as legal ship  */
local void LegalShip(int ship, Arena *arena)
{
    //chat->SendArenaMessage(arena, "Legalship : %i", ship);
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
        if ((g->arena == arena) && g->p_ship != SHIP_SPEC)
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
    }
    pd->Unlock();
}

/* After 10 seconds have passed */
local int TimeUp(void *p)
{
    Player *host = p;
    Arena *arena = host->arena;
    
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    if (adata->started == 0)
        return 0;

    /* Open the Doors and warp players */
    Player *g;
    Link *link;

    cs->ArenaOverride(arena, ok_Doors, 0);

    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
        cs->SendClientSettings(g);

        Target target;
        target.type = T_PLAYER;
        target.u.p = g;
        game->GivePrize(&target, 7, 1);
    }
    pd->Unlock();

    int goals = adata->goals;

    CheckLegalShip(arena);

    chat->SendArenaSoundMessage(arena, 104, "Paintball has started! First team to get %i %s wins!", goals, goals == 1 ? "goal" : "goals");
    chat->SendArenaMessage(arena, "Team 0: EVENS (Blue Team), Team 1: ODDS (Green Team)");

    adata->started = 2;

    mm->RegCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->RegCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->RegCallback(CB_GOAL, Check, arena);
    
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
    if (i == 1 || i == 0)
    {
        chat->SendArenaSoundMessage(arena, 1, "Game stopped. There were not enough players.");
        Stop(arena);
    }
}

/* End the current game */
local void Stop(Arena* arena)
{
    /* Close the doors */
    Player *p;
    Link *link;

    cs->ArenaOverride(arena, ok_Doors, 255);

    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
        /* Send new door settings */
        cs->SendClientSettings(p);
        /* Warp all players */
        Target target;
        target.type = T_PLAYER;
        target.u.p = p;
        game->GivePrize(&target, 7, 1);
    }
    pd->Unlock();

    /* Reset arena data */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    adata->started = 0;
    adata->blue = 0;
    adata->green = 0;
    adata->goals = 0;
    adata->lockships = 0;

    /* Clear timers and unregister callbacks */
    ml->ClearTimer(TimeUp, arena);
    mm->UnregCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->UnregCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->UnregCallback(CB_GOAL, Check, arena);
}

/************************************************************************/
/*                           Game Callbacks                             */
/************************************************************************/

/* Check if a player spectates or changes ship during the game. */
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq)
{
    if (p->p_ship == SHIP_SPEC)
    {
        LCheck(p->arena, p);
    }
    else
    {
        Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
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
    if ((action == PA_DISCONNECT) || (action == PA_LEAVEARENA))
        LCheck(arena, p);

    /* Send a new player the status of the game. */
    if ((action == PA_ENTERARENA) && (p->arena == arena))
    {
        Adata *adata = P_ARENA_DATA(arena, arenaKey);
        int wingoals = adata->goals;
        chat->SendSoundMessage(p, 26, "We are playing Paintball to %i %s by a team.", wingoals, wingoals == 1 ? "goal" : "goals");
    }
}

/* Check the current score. */
local void Check(Arena *arena, Player *p, int bid, int x, int y)
{
    /* Use our own soccer settings */
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
    int wingoals = adata->goals;

    if (p->p_freq == 0)
    {
        adata->blue++;
        chat->SendArenaSoundMessage(p->arena, 2, "Blue Team has scored!");
        chat->SendArenaMessage(p->arena, "SCORE: Blue: %i Green: %i", adata->blue, adata->green);
    }
    else if (p->p_freq == 1)
    {
        adata->green++;
        chat->SendArenaSoundMessage(p->arena, 2, "Green Team has scored!");
        chat->SendArenaMessage(p->arena, "SCORE: Blue: %i Green: %i", adata->blue, adata->green);
    }

    if (adata->blue == wingoals)
    {
        chat->SendArenaSoundMessage(p->arena, 5, "Blue Team (freq 0) wins Paintball!");
        balls->EndGame(arena);
        Stop(p->arena);
    }
    else if (adata->green == wingoals)
    {
        chat->SendArenaSoundMessage(p->arena, 5, "Green Team (freq 1) wins Paintball!");
        balls->EndGame(arena);
        Stop(p->arena);
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
        chat->SendMessage(p, "| Paintball | Two teams face off in a game of paintball (soccer). |");
        chat->SendMessage(p, "-------------------------------------------------------------------");
        chat->SendMessage(p, "Parameters: goals: -g(#)");
        chat->SendMessage(p, "            ships: -s(#)");
        chat->SendMessage(p, "Example: ?start paintball -g(5) -s(1,2,3)");
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
        chat->SendArenaSoundMessage(p->arena, 26, "Game of paintball aborted!");
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
	chat->SendMessage(p, "Two teams face off in a game of paintball (aka soccer, powerball, deathball).");
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_paintball[] = "(Devastation) v0.1 Hakaku";

EXPORT int MM_paintball(int action, Imodman *mm_, Arena *arena)
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
		balls = mm->GetInterface(I_BALLS, arena);
		capman = mm->GetInterface(I_CAPMAN, arena);
        cfg = mm->GetInterface(I_CONFIG, arena);
        chat = mm->GetInterface(I_CHAT, arena);
        cmd = mm->GetInterface(I_CMDMAN, arena);
        cs = mm->GetInterface(I_CLIENTSET, arena);
        game = mm->GetInterface(I_GAME, arena);
		ml = mm->GetInterface(I_MAINLOOP, arena);
		pd = mm->GetInterface(I_PLAYERDATA, arena);

        if (!aman || !balls || !cfg || !chat || !cmd || !cs || !game || !ml || !pd)
		{
		    mm->ReleaseInterface(pd);
            mm->ReleaseInterface(ml);
            mm->ReleaseInterface(game);
            mm->ReleaseInterface(cs);
            mm->ReleaseInterface(cmd);
            mm->ReleaseInterface(chat);
            mm->ReleaseInterface(cfg);
            mm->ReleaseInterface(capman);
            mm->ReleaseInterface(balls);
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
                mm->ReleaseInterface(cs);
                mm->ReleaseInterface(cmd);
                mm->ReleaseInterface(chat);
                mm->ReleaseInterface(cfg);
				mm->ReleaseInterface(capman);
                mm->ReleaseInterface(balls);
                mm->ReleaseInterface(aman);
                return MM_FAIL;
            }
            else
            {
			    ok_Doors = cs->GetOverrideKey("Door", "Doormode");

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
        mm->ReleaseInterface(cs);
        mm->ReleaseInterface(cmd);
        mm->ReleaseInterface(chat);
        mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(capman);
        mm->ReleaseInterface(balls);
        mm->ReleaseInterface(aman);

        return MM_OK;
    }

    return MM_FAIL;
}
