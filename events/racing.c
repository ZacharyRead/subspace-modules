 /**************************************************************
 * Event: Racing
 *
 * Handles everything to do with the Racing events
 *  for Devastation.
 *
 * Requirements:
 *   The map needs to have a defined region named "finish".
 *   The map can also have regions named "rocket", which will grant
 *     a rocket to any player that passes through it.
 *   The arena's default spawn point must be in a closed off area,
 *     with the start line being created with doors.
 *
 * To start a race, a moderator just needs to type ?start race
 * (some options are available). After an amount of time,
 * doors will be opened. Typing ?stop will cancel the event.
 *
 * Based on a plugin originally designed by XDOOM for
 * Deva-bot, recreated by Zachary Read for the ASSS server.
 *
 **************************************************************/

#include "asss.h"
#include "clientset.h"
#include "reldb.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Player data */
typedef struct Pdata
{
    int won;
    int bestime; //in seconds
} Pdata;

local int playerKey;

/* Arena data */
typedef struct Adata
{
    int started;
    int lockships;
    int defaultship;
    int mystery;
    int starttime;
    int bestime; //in seconds
    //const char * bestname;
    int bestship;
    //const char * bestdate;
    int finished;
} Adata;

local int arenaKey;

/* Interfaces */
local Imodman *mm;
local Iarenaman *aman;
local Icapman *capman;
local Iconfig *cfg;
local Ichat *chat;
local Icmdman *cmd;
local Iclientset *cs;
local Igame *game;
local Imainloop *ml;
local Imapdata *mapdata;
local Iplayerdata *pd;
local Ireldb *db;

#define CREATE_RACESTATS_TABLE \
" CREATE TABLE IF NOT EXISTS `racestats` (" \
"  `time` int(11) NOT NULL default '0'," \
"  `name` varchar(24) NOT NULL default ''," \
"  `ship` int(10) NOT NULL default '0'," \
"  `arena` char(24) NOT NULL default ''," \
"  `date` timestamp NOT NULL," \
"  PRIMARY KEY  (`time`)" \
");"

local override_key_t ok_Doors;

local int allships[7];

/************************************************************************/
/*                              Prototypes                              */
/************************************************************************/

//interface functions
local char* getOption(const char *string, char param);
local int getEmptyOption(const char *string, char param);

//game functions
local void Abort(Arena *arena, Player *host, int debug);
local void Begin(Player *host, Arena *arena, const char *params);
local void LegalShip(int ship, Arena *arena);
local void CheckLegalShip(Arena *arena);
local int TimeUp(void *p);
local int RocketArea(void *p);
local void LCheck(Arena *arena, Player *pe);
local void Stop(Arena* arena);

//callbacks
local void ShipFreqChange(Player *p, int newship, int oldship, int newfreq, int oldfreq);
local void PlayerAction(Player *p, int action, Arena *arena);
local char *suffix(int placement);
local void EnterRegion(Player *p, Region *rgn, int x, int y, int entering);

/************************************************************************/
/*                   Main Database Interaction                          */
/************************************************************************/

local void init_db(void)
{
    //make sure the racestats table exists
    db->Query(NULL, NULL, 0, CREATE_RACESTATS_TABLE);
}

local void db_gettop(int status, db_res *res, void *clos)
{
    Player *p = (Player*)clos;
    
    int results = db->GetRowCount(res);
    
    db_row *row;
    row = db->GetRow(res); //get first
    
    if (results < 1)
    {
        //do nothing
    }
    else
    {
        int seconds = atoi(db->GetField(row, 0));
        int ship = atoi(db->GetField(row, 2)) + 1;
        
        Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
        adata->bestime = seconds;
        adata->bestship = ship;
        //adata->bestname = db->GetField(row, 1);
        //adata->bestdate = db->GetField(row, 4);
    }
}

/* Store the player's score into the database */
local void SetScore(Player *p, float time)
{
    int ctime = (int)time;
    //TODO: FIXME
    //use pdata, store time, and compare
    db->Query(db_gettop, p, 1, "SELECT * FROM `racestats` WHERE arena=? AND time=(SELECT MIN(time) FROM racestats WHERE arena=?);", p->arena->basename, p->arena->basename);
    db->Query(NULL,NULL,0,"INSERT INTO `racestats` VALUES(#,?,#,?,NOW());", ctime, p->name, (int)p->p_ship, p->arena->basename);
        
    Pdata *pdata = PPDATA(p, playerKey);
    if (ctime)
    {
        if (ctime < pdata->bestime || !pdata->bestime)
        {
            pdata->bestime = ctime;
            Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
        
            if (!adata->bestime)
            {
                chat->SendArenaSoundMessage(p->arena, 7, "%s sets the bar for this track with %.3f seconds on the clock!",
                    p->name, time / 1000);
                
                adata->bestime = ctime;
                adata->bestship = p->p_ship;
                //adata->bestname = p->name;
                //adata->bestdate = "today";
            }
            else if (ctime < adata->bestime)
            {
                float diff = adata->bestime - ctime;
                chat->SendArenaSoundMessage(p->arena, 7, "%s broke the track record by %.3f seconds! Previous record was %.3f seconds (ship %i).",
                    p->name, diff / 1000, (float)adata->bestime / 1000, adata->bestship); //set by %s (%s) in ship %i.
                
                adata->bestime = ctime;
                adata->bestship = p->p_ship;
                //adata->bestname = p->name;
                //adata->bestdate = "today";
            }
        }
    }
}

/* Returned result from ?best */
local void db_best(int status, db_res *res, void *clos)
{
    Player *p = (Player*)clos;

    int results = db->GetRowCount(res);

    db_row *row;
    row = db->GetRow(res); //get first
    
    if (results < 1)
    {
        chat->SendMessage(p, "You've never raced in here.");
    }
    else
    {
        int seconds = atoi(db->GetField(row, 0));
        int ship = atoi(db->GetField(row, 2)) + 1;
        chat->SendMessage(p, "Your best record: %.3f seconds using ship %i on %s",
            (float)seconds / 1000, ship, db->GetField(row, 4));
        
        Pdata *pdata = PPDATA(p, playerKey);
        pdata->bestime = seconds;
    }
}

/* Returned result from ?trackbest */
local void db_tbest(int status, db_res *res, void *clos)
{
    Player *p = (Player*)clos;

    int results = db->GetRowCount(res);

    db_row *row;
    row = db->GetRow(res); //get first
    
    if (results < 1)
    {
        chat->SendMessage(p, "No record found.");
    }
    else
    {
        int seconds = atoi(db->GetField(row, 0));
        int ship = atoi(db->GetField(row, 2)) + 1;
        chat->SendMessage(p, "Top Record: %.3f seconds, set by %s using ship %i on %s",
            (float)seconds / 1000, db->GetField(row, 1), ship, db->GetField(row, 4));
        
        Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
        adata->bestime = seconds;
        adata->bestship = ship;
        //adata->bestname = db->GetField(row, 1);
        //adata->bestdate = db->GetField(row, 4);
    }
}

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

local int getEmptyOption(const char *string, char param)
{
    if (!param)
        return 0;

    char search[3] = "-d\0";
    search[1] = param;

    char *buf = strstr(string,search);
    if (buf)
        return 1;
    else
        return 0;
}


/************************************************************************/
/*                            Game Functions                           */
/************************************************************************/

local void Abort(Arena *arena, Player *host, int debug)
{
    /* Reset values */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    adata->started = 0;
    adata->lockships = 0;
    adata->defaultship = 0;
    adata->mystery = 0;
    adata->starttime = 0;
    adata->finished = 0;

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
    {
        adata->started = 1;
    }		
    /* Check top score */
    db->Query(db_gettop, host, 1, "SELECT * FROM `racestats` WHERE arena=? AND time=(SELECT MIN(time) FROM racestats WHERE arena=?);", host->arena->basename, host->arena->basename);

    /* Get Game Options */
    //mystery mode
    if (getEmptyOption(params, 'm'))
        adata->mystery = 1;
    else
        adata->mystery = 0;

    //ships
    char *next, *string;
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
		if (d->arena == arena)
		{
			cs->SendClientSettings(d);

			Target target;
			target.type = T_PLAYER;
			target.u.p = d;
			game->GivePrize(&target, PRIZE_WARP, 1);
			game->ShipReset(&target);
		}
    }
    pd->Unlock();

    /* Get the name of the arena */
    const char *longname;
    longname = cfg->GetStr(arena->cfg, "Race", "LongName");
    if ((!longname) || (strlen(longname) < 2))
        longname = arena->name;

    chat->SendArenaSoundMessage(arena, 2, "%s is starting in 20 seconds! Enter if you want to play!", longname);
    if (adata->lockships)
        chat->SendArenaMessage(arena, "Allowed ships: %s", string);
    if (adata->mystery)
        chat->SendArenaMessage(arena, "Mystery mode activated! Everyone gets cloak and stealth!");

    CheckLegalShip(arena);

    adata->started = 1;
    /* Start the timer */
    ml->SetTimer(TimeUp, 2000, 2000, host, arena);
}

/* Set as legal ship  */
local void LegalShip(int ship, Arena *arena)
{
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

/* After 20 seconds have passed */
local int TimeUp(void *p)
{
    Player *host = p;
    Arena *arena = host->arena;

    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    if (adata->started == 0)
        return 0;

    /* Open the Doors and prize the players */
    Player *g;
    Link *link;

    cs->ArenaOverride(arena, ok_Doors, 0);

    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
		if (g->arena == arena)
		{
			cs->SendClientSettings(g);

			if (g->p_ship != SHIP_SPEC)
			{
				Target target;
				target.type = T_PLAYER;
				target.u.p = g;
				
				game->ShipReset(&target); 
				game->GivePrize(&target, PRIZE_ROCKET, 1);

				if (adata->mystery)
				{
					game->GivePrize(&target, PRIZE_CLOAK, 1);
					game->GivePrize(&target, PRIZE_STEALTH, 1);
				}

				Pdata *pdata = PPDATA(g, playerKey);
				pdata->won = 0;
			}
		}
    }
    pd->Unlock();


    CheckLegalShip(arena);

    chat->SendArenaSoundMessage(arena, 104, "Race started.");
    adata->starttime = current_millis();

    adata->started = 2;

    mm->RegCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->RegCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->RegCallback(CB_REGION, EnterRegion, arena);
    
    ml->SetTimer(RocketArea, 200, 200, host, arena);

    return 0;
}

/* If a player is in a rocket area, prize him */
local int RocketArea(void *p)
{
    Player *host = p;
    Region *rgn;

    rgn = mapdata->FindRegionByName(host->arena, "rocket");
    
    if (!rgn)
        return 0;
    
    Player *g;
    Link *link;
    
    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
		if (g->arena == host->arena)
		{
			int x = g->position.x >> 4;
			int y = g->position.y >> 4;
			if ((g->p_ship != SHIP_SPEC) && (mapdata->Contains(rgn, x, y)))
			{
				Target target;
				target.type = T_PLAYER;
				target.u.p = g;
				game->GivePrize(&target, PRIZE_ROCKET, 1);
			}
		}
    }
    pd->Unlock();
    
    return 1;
}

/* Check if players have left the arena or specced. */
local void LCheck(Arena *arena, Player *pe)
{
    Player *p = NULL;
    Link *link;
    int i = 0;
    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
        if ((p->arena == arena) && (p->p_ship != SHIP_SPEC) && (p != pe))
        {
            i++;
        }
    }
    pd->Unlock();
    if (i == 0)
    {
        chat->SendArenaSoundMessage(arena, 1, "Game stopped. There were not enough players.");
        Stop(arena);
    }
}

/* End the current game */
local void Stop(Arena* arena)
{
    Player *p;
    Link *link;
    
    cs->ArenaOverride(arena, ok_Doors, 255);

    pd->Lock();
    FOR_EACH_PLAYER(p)
    {
		if (p->arena == arena)
		{
			/* Send new door settings */
			cs->SendClientSettings(p);
        
        /* Warp all players */
//        Target target;
//        target.type = T_PLAYER;
//        target.u.p = p;
//        game->GivePrize(&target, 7, 1);
		}
    }
    pd->Unlock();

    /* Reset arena data */
    Adata *adata = P_ARENA_DATA(arena, arenaKey);
    adata->started = 0;
    adata->lockships = 0;
    adata->defaultship = 0;
    adata->mystery = 0;
    adata->starttime = 0;
    adata->finished = 0;

    /* Clear timers and unregister callbacks */
    ml->ClearTimer(TimeUp, arena);
    ml->ClearTimer(RocketArea, arena);
    mm->UnregCallback(CB_PLAYERACTION, PlayerAction, arena);
    mm->UnregCallback(CB_SHIPFREQCHANGE, ShipFreqChange, arena);
    mm->UnregCallback(CB_REGION, EnterRegion, arena);
}

/************************************************************************/
/*                           Game Callbacks                             */
/************************************************************************/

/* Check if a player spectates the game. */
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
    if (action == PA_ENTERARENA)
    {
        db->Query(db_best, p, 1, "SELECT * FROM `racestats` WHERE name=? AND arena=? AND time=(SELECT MIN(time) FROM racestats WHERE name=? AND arena=?);",
            p->name, p->arena->basename, p->name, p->arena->basename);
        
        db->Query(db_gettop, p, 1, "SELECT * FROM `racestats` WHERE arena=? AND time=(SELECT MIN(time) FROM racestats WHERE arena=?);", p->arena->basename, p->arena->basename);
    }
}

/* Get the suffix (e.g. 1st, 2nd, 3rd, 4th) */
local char *suffix(int placement)
{
    if (placement == 1)
        return "st";
    else if (placement == 2)
        return "nd";
    else if (placement == 3)
        return "rd";
    else
        return "th";
}

/* When a player crosses the finish line */
local void EnterRegion(Player *p, Region *rgn, int x, int y, int entering)
{
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
    const char *rname = mapdata->RegionName(rgn);
    const char *s = "finish";
    Pdata *pdata = PPDATA(p, playerKey);
    
    if (!adata->started)
    {
        return;
    }
    
    if (!IS_STANDARD(p) || p->p_ship == SHIP_SPEC || pdata->won)
    {
        return;
    }
    
    if (strcmp(rname, s) != 0)
    {
        return;
    }
    
    
    pdata->won = 1;
    
    adata->finished++;
    float time = (current_millis() - adata->starttime);
    chat->SendArenaSoundMessage(p->arena, 103, "%s reached the finish line %i%s with a time of %.3f seconds!", 
        p->name, 
        adata->finished, 
        suffix(adata->finished), 
        time / 1000);
    SetScore(p, time);
    
    Link *link;
    Player *g;
    int remaining = 0;
    
    pd->Lock();
    FOR_EACH_PLAYER(g)
    {
        if (g->arena == p->arena)
        {
        	Pdata *gdata = PPDATA(g, playerKey);
        	if (g->p_ship != SHIP_SPEC && gdata->won == 0)
        		remaining++;
        }
    }
    pd->Unlock();
    
    if (!remaining)
    {
        Stop(p->arena);
        chat->SendArenaMessage(p->arena,"Race over!");
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
        chat->SendMessage(p, "| Race      | First player to the finish line wins.               |");
        chat->SendMessage(p, "-------------------------------------------------------------------");
        chat->SendMessage(p, "Parameters:  ships: -s(#)");
        chat->SendMessage(p, "      mystery mode: -m");
        chat->SendMessage(p, "Example: ?start race -s(1,4,5) -m");
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
        chat->SendArenaSoundMessage(p->arena, 26, "Current race aborted!");
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
    chat->SendMessage(p, "First player to the finish line wins.");
}

/* ?time help information */
local helptext_t time_help =
"Targets: none\n"
"Args: none\n"
"Displays the time that has passed since the start of the race.\n";

/* Show the time elapsed to the player. */
local void cTime(const char *command, const char *params, Player *p, const Target *target)
{
    Adata *adata = P_ARENA_DATA(p->arena, arenaKey);
    if (adata->started == 2)
    {
        float time = current_millis() - adata->starttime;
        chat->SendMessage(p, "Time passed: %.01f seconds", time/1000);
    }
    else
        chat->SendMessage(p, "There is no race currently started.");
}

/* ?best help information */
local helptext_t best_help =
"Targets: none\n"
"Args: none\n"
"Displays your best track record for this arena.\n";

//best
local void cBest(const char *command, const char *params, Player *p, const Target *target)
{
    if (target->type == T_PLAYER)
    {
        Player *t = target->u.p;
        db->Query(db_best, p, 1, "SELECT * FROM `racestats` WHERE name=? AND arena=? AND time=(SELECT MIN(time) FROM racestats WHERE name=? AND arena=?);",
            t->name, p->arena->basename, t->name, p->arena->basename);
    }
    else
    {
        db->Query(db_best, p, 1, "SELECT * FROM `racestats` WHERE name=? AND arena=? AND time=(SELECT MIN(time) FROM racestats WHERE name=? AND arena=?);",
            p->name, p->arena->basename, p->name, p->arena->basename);
    }
}

/* ?trackbest help information */
local helptext_t trackbest_help =
"Targets: none\n"
"Args: none\n"
"Displays the overall best track record for this arena.\n";

//trackbest
local void cTrackBest(const char *command, const char *params, Player *p, const Target *target)
{
    db->Query(db_tbest, p, 1, "SELECT * FROM `racestats` WHERE arena=? AND time=(SELECT MIN(time) FROM racestats WHERE arena=?);",
        p->arena->basename, p->arena->basename);
}

/************************************************************************/
/*                            Module Init                               */
/************************************************************************/

EXPORT const char info_racing[] = "(Devastation) v0.1 Hakaku";

EXPORT int MM_racing(int action, Imodman *mm_, Arena *arena)
{
    if (action == MM_LOAD)
    {
        mm = mm_;

        aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
        capman = mm->GetInterface(I_CAPMAN, ALLARENAS);
        cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
        chat = mm->GetInterface(I_CHAT, ALLARENAS);
        cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
        cs = mm->GetInterface(I_CLIENTSET, ALLARENAS);
        game = mm->GetInterface(I_GAME, ALLARENAS);
        ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
        mapdata = mm->GetInterface(I_MAPDATA, ALLARENAS);
        pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
        db = mm->GetInterface(I_RELDB, ALLARENAS);

        if (!aman || !cfg || !chat || !cmd || !cs || !game || !ml || !mapdata || !pd || !db)
        {
            mm->ReleaseInterface(db);
            mm->ReleaseInterface(pd);
            mm->ReleaseInterface(mapdata);
            mm->ReleaseInterface(ml);
            mm->ReleaseInterface(game);
            mm->ReleaseInterface(cs);
            mm->ReleaseInterface(cmd);
            mm->ReleaseInterface(chat);
            mm->ReleaseInterface(cfg);
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
                mm->ReleaseInterface(db);
                mm->ReleaseInterface(pd);
                mm->ReleaseInterface(mapdata);
                mm->ReleaseInterface(ml);
                mm->ReleaseInterface(game);
                mm->ReleaseInterface(cs);
                mm->ReleaseInterface(cmd);
                mm->ReleaseInterface(chat);
                mm->ReleaseInterface(cfg);
                mm->ReleaseInterface(capman);
                mm->ReleaseInterface(aman);
                return MM_FAIL;
            }
            else
            {
                init_db();
                return MM_OK;
            }
        }
    }
    else if (action == MM_UNLOAD)
    {
        pd->FreePlayerData(playerKey);
        aman->FreeArenaData(arenaKey);

        mm->ReleaseInterface(db);
        mm->ReleaseInterface(pd);
        mm->ReleaseInterface(mapdata);
        mm->ReleaseInterface(ml);
        mm->ReleaseInterface(game);
        mm->ReleaseInterface(cs);
        mm->ReleaseInterface(cmd);
        mm->ReleaseInterface(chat);
        mm->ReleaseInterface(cfg);
        mm->ReleaseInterface(capman);
        mm->ReleaseInterface(aman);

        return MM_OK;
    }
    else if (action == MM_ATTACH)
    {
        ok_Doors = cs->GetOverrideKey("Door", "Doormode");

        cmd->AddCommand("host", cHost, arena, host_help);
        cmd->AddCommand("start", cHost, arena, host_help);
        cmd->AddCommand("stopevent", cStopEvent, arena, stop_help);
        cmd->AddCommand("stop", cStopEvent, arena, stop_help);
        cmd->AddCommand("rules", cRules, arena, rules_help);
        cmd->AddCommand("time", cTime, arena, time_help);
        cmd->AddCommand("best", cBest, arena, best_help);
        cmd->AddCommand("trackbest", cTrackBest, arena, trackbest_help);
        
        return MM_OK;
    }
    else if (action == MM_DETACH)
    {
        ml->ClearTimer(TimeUp, arena);
        ml->ClearTimer(RocketArea, arena);
        
        cmd->RemoveCommand("trackbest", cTrackBest, arena);
        cmd->RemoveCommand("best", cBest, arena);
        cmd->RemoveCommand("time", cTime, arena);
        cmd->RemoveCommand("rules", cRules, arena);
        cmd->RemoveCommand("stop", cStopEvent, arena);
        cmd->RemoveCommand("stopevent", cStopEvent, arena);
        cmd->RemoveCommand("start", cHost, arena);
        cmd->RemoveCommand("host", cHost, arena);
        
        return MM_OK;
    }
    return MM_FAIL;
}
