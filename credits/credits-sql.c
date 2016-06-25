 /**************************************************************
 * Credits Module
 *
 * The Credits Module handles all credits-related events,
 * including adding, removing, storing and resetting credits.
 *
 * This module was originally developed for the zone Devastation.
 * Jackpot-related dependencies have been removed.
 *
 * Started by user Hallowed be thy name, expanded by Zachary Read.
 *
 **************************************************************/

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "asss.h"
#include "reldb.h"
#include "fg_wz.h"
#include "credits.h"
#include "flagcore.h"
#include "balls.h"

/* Interfaces */
local Imodman *mm;
local Ireldb *db;
local Ichat *chat;
local Icmdman *cmd;
local Iconfig *cfg;
local Iplayerdata *pd;
local Iarenaman *aman;
local Imainloop *ml;
local Iballs *balls;


#define CREATE_CREDS_TABLE \
" CREATE TABLE IF NOT EXISTS `players` (" \
"   `id` int(11) NOT NULL auto_increment," \
"   `name` varchar(24) NOT NULL default ''," \
"   `credits` int(11) NOT NULL default '0'," \
"   `kills` int(8) NOT NULL default'0'," \
"   `deaths` int(8) NOT NULL default'0'," \
"   `points` int(11) NOT NULL default'0'," \
"   PRIMARY KEY  (`id`)" \
" );"

/* Player Data */
typedef struct Pdata
{
	unsigned long credits;
	char new;
	char newplayer;
} Pdata;

local int playerKey;

local float taxRate;

local void addCredits(Player *p, unsigned long creds);

/************************************************************************/
/*				   Main Database Interaction						  */
/************************************************************************/

local void init_db(void)
{
	//make sure the logins table exists
	db->Query(NULL, NULL, 0, CREATE_CREDS_TABLE);
}

local void db_loadcb(int status, db_res *res, void *clos)
{
	Player *p = (Player*)clos;
	Pdata *data = PPDATA(p, playerKey);

	int results = db->GetRowCount(res);

	db_row *row;
	row = db->GetRow(res);

	if (results > 0)
	{
		//Played in here before. Get his creds!
		data->credits = atoi(db->GetField(row, 0));
	}
	else
	{
		//We've got a new player.
		int inicreds = cfg->GetInt(GLOBAL, "Welcome", "InitialCredits", 10000000);
		
		db->Query(NULL, NULL, 0,"insert into players (name,credits) VALUES (?,#)", p->name, inicreds);
		data->credits = inicreds;
		data->new = 1;
	}
}


local void loadPlayer(Player *p)
{
	db->Query(db_loadcb, p, 1, "select `credits` from players where name=?", p->name);
}


local void savePlayer(Player *p)
{
	Pdata *data = PPDATA(p, playerKey);
	db->Query(NULL,NULL,0,"UPDATE `players` SET `credits` = '#' WHERE `name` = ?", data->credits, p->name);

	int connected = db->GetStatus(); 
	if (!connected) 
	{ 
		chat->SendModMessage("<credits> Attempted to store %s's credits (%lu), but database appears to be offline.", p->name, data->credits); 
	}
}

local void loadAllPlayers()
{
	Player *p;
	Link *link;
	pd->Lock();
	FOR_EACH_PLAYER(p)
	{
		loadPlayer(p);
	}
	pd->Unlock();
}

local void cPlayerAction(Player *p, int action, Arena *arena)
{
	if (!IS_HUMAN(p))
		return;

	if (action == PA_CONNECT)
	{
		loadPlayer(p);
	}

	if (action == PA_DISCONNECT)
	{
		savePlayer(p);
	}

	if (action == PA_ENTERARENA)
	{
		Pdata *data = PPDATA(p, playerKey);
		if (data->new == 1)
		{
			//We'll now send a welcome message
			const char *msg = cfg->GetStr(GLOBAL, "Welcome", "Message");
			chat->SendMessage(p, "Welcome %s, %s",p->name,msg);
			data->new = 0;
			data->newplayer = 1;
		}
	}
}

/************************************************************************/
/*						  Giving out Credits						  */
/************************************************************************/
local void cKill(Arena *arena, Player *k, Player *p, int bounty, int flags, int pts, int green)
{
	addCredits(k, (unsigned long)((k->position.bounty*10)+(p->position.bounty*30))); //bounty*9 ... 13
}

local void cGoal(Arena *arena, Player *p, int bid, int x, int y)
{
	addCredits(p, (unsigned long)(150000));
}

/************************************************************************/
/*						  Interface Functions						 */
/************************************************************************/

local unsigned long getCredits(Player *p)
{
	if (!IS_HUMAN(p))
		return 0;
	Pdata *data = PPDATA(p, playerKey);
	return data->credits;
}

local void setCredits(Player *p, unsigned long creds)
{
	if (!IS_HUMAN(p))
		return;
	Pdata *data = PPDATA(p, playerKey);
	unsigned long old = data->credits;
	data->credits = creds;
	savePlayer(p);
	DO_CBS(CB_CREDITS, p->arena, CreditFunc, (p, old));
}

local void resetCredits(Player *p)
{
	if (!IS_HUMAN(p))
		return;
	Pdata *data = PPDATA(p, playerKey);
	unsigned long old = data->credits;
	data->credits = cfg->GetInt(GLOBAL, "Welcome", "InitialCredits", 10000000);
	savePlayer(p);
	DO_CBS(CB_CREDITS, p->arena, CreditFunc, (p, old));
}

local void addCredits(Player *p, unsigned long creds)
{
	if (!IS_HUMAN(p))
		return;
	Pdata *data = PPDATA(p, playerKey);
	unsigned long old = data->credits;
	data->credits += creds;
	savePlayer(p);
	DO_CBS(CB_CREDITS, p->arena, CreditFunc, (p, old));
}

local void removeCredits(Player *p, unsigned long creds)
{
	if (!IS_HUMAN(p))
		return;
	Pdata *data = PPDATA(p, playerKey);
	int old = data->credits;
	data->credits -= creds;
	savePlayer(p);
	DO_CBS(CB_CREDITS, p->arena, CreditFunc, (p, old));
}

local void updateDB()
{
	Player *p;
	Link *link;
	pd->Lock();
	FOR_EACH_PLAYER(p)
	{
		savePlayer(p);
	}
	pd->Unlock();
}

local Icredits interface =
{
	INTERFACE_HEAD_INIT(I_CREDITS, "Icredits")
	getCredits,	setCredits,	addCredits,	removeCredits,
	savePlayer,	updateDB
};


/************************************************************************/
/*						  Player Commands							 */
/************************************************************************/
local helptext_t credits_help =
"Targets: Player, none\n"
"Args: none\n"
"If no target is selected, the ?credits command will list your own credits.\n"
"Otherwise, private message a player ?credits in order to view how many\n"
"credits they currently have.\n";

local void cCreds(const char *command, const char *params, Player *p, const Target *target)
{   
	int si = 0; 
	if ((!params) || (strlen(params) <= 1))
		si = 1;

	if ((si == 0) && (target->type != T_PLAYER))
	{
		Player *find = NULL;
		find = pd->FindPlayer(params);

		if (find)
		{
			Pdata *fd = PPDATA(find, playerKey);
			int fcreds = fd->credits;

			if (p == find)
				chat->SendMessage(p, "You have %i (%.2fM) credits on your account.", fcreds, (float)fcreds/1000000);
			else
				chat->SendMessage(p, "%s has %i (%.2fM) credits on his or her account.", find->name, fcreds, (float)fcreds/1000000);
		}
		else
			chat->SendMessage(p, "Unable to locate player %s.", params);
		return;
	}
	else
	{
		Player *from = p;
		if (target->type == T_PLAYER)
			from = target->u.p;

		Pdata *data = PPDATA(from, playerKey);
		int creds = data->credits;

		if (p == from)
			chat->SendMessage(p, "You have %i (%.2fM) credits on your account.", creds, (float)creds/1000000);
		else
			chat->SendMessage(p, "%s has %i (%.2fM) credits on his or her account.", from->name, creds, (float)creds/1000000);
	}
}

local long standardParse(Player *p, const char *params, const Target *target)
{
	if (target->type == T_PLAYER)
	{
		/*
		Player *t = target->u.p;

		if (p == t)
		{
			chat->SendMessage(p, "You can't set your own credits!");
			return -1;
		}
		*/

		char *next;
		if (params[0] == '-')
		{
			chat->SendMessage(p, "No way!");
		}
		else
		{
			unsigned long new = strtol(params, &next, 0);
			if (next != params)
			{
				return new;
			}
			else
			{
				chat->SendMessage(p, "Please enter a valid number.");
			}
		}
	}

	return -1;
}

local helptext_t setcredits_help =
"Targets: Player\n"
"Args: none\n"
"Private message a player ?setcredits <amount> in order\n"
"To specify a new value.\n";

local void cSetCreds(const char *command, const char *params, Player *p, const Target *target)
{
	long new = standardParse(p,params,target);
	if (new != -1) /* -1 being the value returned by standardParse */
	{
		if (new < 0)
			new = 0; /* If a value ends up negative, make it 0 */

		Player *t = target->u.p;
		setCredits(t, new);
		chat->SendMessage(p, "Set credits of %s to %li.",t->name,new);
		chat->SendMessage(t, "Your credits got set to %li -%s",new,p->name);
	}
}

local helptext_t resetcredits_help =
"Targets: Player\n"
"Args: none\n"
"Private message a player ?resetcredits in order\n"
"To reset their credits to the default amount.\n";

local void cResetCreds(const char *command, const char *params, Player *p, const Target *target)
{
	Player *t = target->u.p;
	resetCredits(t);
	chat->SendMessage(p, "Reset %s's credits.",t->name);
}

local helptext_t addcredits_help =
"Targets: Player\n"
"Args: none\n"
"Private message a player ?addcredits <amount> in order\n"
"To give extra credits to that player.\n";

local void cAddCreds(const char *command, const char *params, Player *p, const Target *target)
{
	long new = standardParse(p,params,target);
	if (new != -1)
	{
		Player *t = target->u.p;
		addCredits(t, new);
		chat->SendMessage(p, "Increased %s's credits by %li",t->name,new);
		chat->SendMessage(t, "Your credits got increased by %li -%s",new,p->name);
	}
	else if (target->type == T_ARENA)
	{
		char *next;
		if (params[0] == '-')
		{
			chat->SendMessage(p, "No way!");
		}
		unsigned long new = strtol(params, &next, 0);
		if (next != params)
		{
			Player *pl; Link *link;
			pd->Lock();
			FOR_EACH_PLAYER(pl)
			{
				if (p->arena == target->u.arena)
				{
					addCredits(pl, new);
					chat->SendMessage(pl, "Your credits got increased by %li -%s",new,p->name);
				}
			}
			pd->Unlock();
		}
		else
		{
			chat->SendMessage(p, "Please enter a valid number.");
		}
	}
}

local helptext_t giveall_help =
"Targets: none\n"
"Args: none\n"
"Gives all players in the arena credits.\n";

local void cGiveAll(const char *command, const char *params, Player *p, const Target *target)
{
	char *next;
	if (params[0] == '-')
	{
		chat->SendMessage(p, "No way!");
	}
	unsigned long new = strtol(params, &next, 0);
	if (next != params)
	{
		Player *pl;
		Link *link;
		pd->Lock();
		FOR_EACH_PLAYER(pl)
		{
			if (pl->arena == p->arena)
			{
				addCredits(pl, new);
				chat->SendMessage(pl, "Your credits got increased by %li -%s",new,p->name);
			}
		}
		pd->Unlock();
	}
	else
	{
		chat->SendMessage(p, "Please enter a valid number.");
	}
}

local helptext_t removecreds_help =
"Targets: Player\n"
"Args: none\n"
"Private message a player ?removecredits <amount> in order\n"
"To remove a specified amount of credits.\n";

local void cRemoveCreds(const char *command, const char *params, Player *p, const Target *target)
{
	long new = standardParse(p,params,target);
	if (new != -1)
	{
		Player *t = target->u.p;
		removeCredits(t, new);
		chat->SendMessage(p, "Increased %s's credits by %li",t->name,new);
		chat->SendMessage(t, "Your credits got increased by %li -%s",new,p->name);
	}

	else if (target->type == T_ARENA)
	{
		char *next;
		if (params[0] == '-')
		{
			chat->SendMessage(p, "No way!");
		}
		unsigned long new = strtol(params, &next, 0);
		if (next != params)
		{
			Player *pl; Link *link;
			pd->Lock();
			FOR_EACH_PLAYER(pl)
			{
				if (p->arena == target->u.arena)
				{
					removeCredits(pl, new);
					chat->SendMessage(pl, "Your credits got increased by %li. -%s",new,p->name);
				}
			}
			pd->Unlock();
		}
		else
		{
			chat->SendMessage(p, "Please enter a valid number.");
		}
	}
}

//Perform the donation
local void donation(Player *p, const Target *target, unsigned long new)
{
	if (new <= 0)
	{
		chat->SendMessage(p, "Invalid amount specified.");
	}
	Player *t = target->u.p;
	if (p == t)
	{
		chat->SendMessage(p, "You can't donate to yourself!");
		return;
	}

	Pdata *data = PPDATA(p, playerKey);
	if (data->newplayer == 1)
	{
		chat->SendMessage(p, "Sorry, this command has been temporarily disabled.");
		chat->SendModMessage("New Player %s attempted to donate his or her credits to %s.", p->name, t->name);
		return;
	}

	if (!getCredits(p))
	{
		chat->SendMessage(p, "You cannot donate credits because your credits are not available. Try re-entering the zone as the database may have been offline.");
		return;
	}

	long tax = new * taxRate;
	long total = tax + new;

	if (total > getCredits(p))
	{
		chat->SendMessage(p, "You don't have enough credits! Donating %li credits entails a tax of %li credits, totalling %li credits", new, tax, total);
		return;
	}
	if (new < 0)
	{
		chat->SendMessage(p, "No way!");
		return;
	}
	else
	{
		removeCredits(p, total);
		addCredits(t, new);
		chat->SendMessage(p, "You donated %li credits to %s.", new, t->name);
		chat->SendMessage(t, "%s donated you %li credits.", p->name, new);
	}
}

local helptext_t donate_help =
"Targets: Player\n"
"Args: none\n"
"Private message a player ?donate <amount> in order\n"
"To donate some of your own credits to that player.\n";

local void cDonate(const char *command, const char *params, Player *p, const Target *target)
{
	char *next;
	unsigned long new = strtol(params, &next, 0);
	if (target->type == T_FAKE)
	{
		chat->SendMessage(p, "Could not donate, the target is not a real player.");
		return;
	}
	else if ((params) && (target->type != T_PLAYER)) //assume ?donate Bob:5
	{
		// Number of words we still want to display.
		int words = 1, i = 0;
		
		char username[MAXPACKET];
		
		// Loop through entire summary.
		for (i = 0; i < strlen(params); i++)
		{
			// Increment words on a space.
			if (params[i] == ':')
			{
				words--;
				strncpy(username, params, i);
				username[i] = '\0';
			}
			// If we have no more words to display, return the substring.
			if (words == 0)
			{
				break;
			}
		}

		if (words)
		{
			chat->SendMessage(p, "Syntax: ?donate PLAYER:CREDITS");
			return;
		}
		
		Player *find = NULL;
		find = pd->FindPlayer(username);

		if (find)
		{
			int result = atoi(params + (i + 1));
			
			Target t; 
			t.type = T_PLAYER; 
			t.u.p = find; 
			
			donation(p, &t, result);
		}
		else
		{
			chat->SendMessage(p, "Unable to locate player %s.", username);
		}
	}
	else //assume /?donate 5
	{ 
		donation(p, target, new);
	}
}

local helptext_t destroy_help =
"Targets: none\n"
"Args: -c, -s\n"
"Using ?destroy -c will clear the entire credits database"
"and return the top player. Using ?destroy -s will clear the"
"player scores table and return the top players of certain"
"sections.\n";

local void cDestroy(const char *command, const char *params, Player *p, const Target *target)
{
	int connected = db->GetStatus();
	if (!connected)
	{
		chat->SendMessage(p,"Could not complete request, database appears to be offline.");
	}
	else
	{
		if (strstr(params, "-c"))
		{
			/* Top 5 credits */
			chat->SendMessage(p,"CREDITS TOP 5");
			db->Query(NULL, NULL, 0,"SELECT name,credits FROM players ORDER BY credits DESC LIMIT 5");

			/* DESTROY player credits */
			db->Query(NULL, NULL, 0,"TRUNCATE TABLE players");
			
			Player *g;
			Link *link;
			pd->Lock();
			FOR_EACH_PLAYER(g)
			{
				resetCredits(g);
			}
			pd->Unlock();
		}
		if (strstr(params, "-s"))
		{   
			/* Top 5 wins */
			chat->SendMessage(p,"WINS TOP 5");
			db->Query(NULL, NULL, 0,"SELECT playername,wins FROM scores ORDER BY wins DESC LIMIT 5");
			
			/* Top 5 losses */
			chat->SendMessage(p,"LOSSES TOP 5");
			db->Query(NULL, NULL, 0,"SELECT playername,losses FROM scores ORDER BY losses DESC LIMIT 5");

			/* Top 5 points */
			chat->SendMessage(p,"TOTALPOINTS TOP 5");
			db->Query(NULL, NULL, 0,"SELECT playername,totalpoints FROM scores ORDER BY totalpoints DESC LIMIT 5");

			/* Top 5 ratio */
			chat->SendMessage(p,"RATIO TOP 3");
			db->Query(NULL, NULL, 0,"SELECT playername,ratio FROM scores ORDER BY ratio DESC LIMIT 3");

			/* DESTROY scores */
			db->Query(NULL, NULL, 0,"TRUNCATE TABLE scores");
		}
	}
}

/* Timer : every 5 minutes, save every player's credits. */
local int saveall(void *arena_)
{
	updateDB();
	
	int connected = db->GetStatus();
	if (!connected)
	{
		chat->SendModMessage("<credits> Attempting to store player credits, but database appears to be offline.");
	}
	
	return 1;
}

/* Start timer when the arena is created. */
local void ArenaAction(Arena *arena, int action)
{
	if (action == AA_CREATE)
	{
		/* Start Timers */
		ml->SetTimer(saveall, 30000, 30000, arena, arena);
	}
	else if (action == AA_DESTROY)
	{
		ml->ClearTimer(saveall, arena);
	}
}

/************************************************************************/
/*							Module Init							   */
/************************************************************************/

EXPORT const char info_credits[] = "Credits v1.45";

EXPORT int MM_credits(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;

		db = mm->GetInterface(I_RELDB, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		balls = mm->GetInterface(I_BALLS, ALLARENAS);

		if (!db || !chat || !cmd || !cfg || !pd || !aman || !ml || !balls)
			return MM_FAIL;
		else
		{
			playerKey = pd->AllocatePlayerData(sizeof(Pdata));
			if (!playerKey)
			{
				mm->ReleaseInterface(balls);
				mm->ReleaseInterface(ml);
				mm->ReleaseInterface(aman);
				mm->ReleaseInterface(pd);
				mm->ReleaseInterface(cfg);
				mm->ReleaseInterface(cmd);
				mm->ReleaseInterface(chat);
				mm->ReleaseInterface(db);
				return MM_FAIL;
			}
			else
			{
				init_db();
				loadAllPlayers();

				mm->RegCallback(CB_KILL, cKill, ALLARENAS);
				mm->RegCallback(CB_GOAL, cGoal, ALLARENAS);
				mm->RegCallback(CB_PLAYERACTION, cPlayerAction, ALLARENAS);
				mm->RegCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);

				cmd->AddCommand("creds", cCreds, ALLARENAS, credits_help);
				cmd->AddCommand("setcreds", cSetCreds, ALLARENAS, setcredits_help);
				cmd->AddCommand("addcreds", cAddCreds, ALLARENAS, addcredits_help);
				cmd->AddCommand("removecreds", cRemoveCreds, ALLARENAS, removecreds_help);
				cmd->AddCommand("donate", cDonate, ALLARENAS, donate_help);
				cmd->AddCommand("credits", cCreds, ALLARENAS, credits_help);
				cmd->AddCommand("setcredits", cSetCreds, ALLARENAS, setcredits_help);
				cmd->AddCommand("addcredits", cAddCreds, ALLARENAS, addcredits_help);
				cmd->AddCommand("removecredits", cRemoveCreds, ALLARENAS, removecreds_help);
				cmd->AddCommand("resetcreds", cResetCreds, ALLARENAS, resetcredits_help);
				cmd->AddCommand("resetcredits", cResetCreds, ALLARENAS, resetcredits_help);
				cmd->AddCommand("givecredits", cAddCreds, ALLARENAS, addcredits_help);
				cmd->AddCommand("givecreds", cAddCreds, ALLARENAS, addcredits_help);
				cmd->AddCommand("cgive", cAddCreds, ALLARENAS, addcredits_help);
				cmd->AddCommand("giveall", cGiveAll, ALLARENAS, giveall_help);
				cmd->AddCommand("destroy", cDestroy, ALLARENAS, destroy_help);
				
				mm->RegInterface(&interface, ALLARENAS);
				taxRate = cfg->GetInt(GLOBAL, "Credits", "TaxRate", 2)/100;

				return MM_OK;
			}
		}
	}
	else if (action == MM_UNLOAD)
	{
		updateDB();

		if (mm->UnregInterface(&interface, ALLARENAS))
		{
			return MM_FAIL;
		}
		
		ml->ClearTimer(saveall, NULL);

		cmd->RemoveCommand("removecredits", cRemoveCreds, ALLARENAS);
		cmd->RemoveCommand("addcredits", cAddCreds, ALLARENAS);
		cmd->RemoveCommand("setcredits", cSetCreds, ALLARENAS);
		cmd->RemoveCommand("credits", cCreds, ALLARENAS);
		cmd->RemoveCommand("donate", cDonate, ALLARENAS);
		cmd->RemoveCommand("removecreds", cRemoveCreds, ALLARENAS);
		cmd->RemoveCommand("addcreds", cAddCreds, ALLARENAS);
		cmd->RemoveCommand("setcreds", cSetCreds, ALLARENAS);
		cmd->RemoveCommand("creds", cCreds, ALLARENAS);
		cmd->RemoveCommand("resetcreds", cResetCreds, ALLARENAS);
		cmd->RemoveCommand("resetcredits", cResetCreds, ALLARENAS);
		cmd->RemoveCommand("givecredits", cAddCreds, ALLARENAS);
		cmd->RemoveCommand("givecreds", cAddCreds, ALLARENAS);
		cmd->RemoveCommand("cgive", cAddCreds, ALLARENAS);
		cmd->RemoveCommand("giveall", cGiveAll, ALLARENAS);
		cmd->RemoveCommand("destroy", cDestroy, ALLARENAS);

		mm->UnregCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);
		mm->UnregCallback(CB_PLAYERACTION, cPlayerAction, ALLARENAS);
		mm->UnregCallback(CB_KILL, cKill, ALLARENAS);
		mm->UnregCallback(CB_GOAL, cGoal, ALLARENAS);

		pd->FreePlayerData(playerKey);

		mm->ReleaseInterface(balls);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(db);

		return MM_OK;
	}

	return MM_FAIL;
}
