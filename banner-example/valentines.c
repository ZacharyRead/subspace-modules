 /**************************************************************
 * Valentines Banner Module
 *
 * This module provides a proof-of-concept on how you can
 * change player banners from the server-level. Specifically,
 * it will set the banner of all players on the zone to a
 * heart banner.
 *
 * By: Zachary Read
 *
 * ************************************************************/

#include <stdlib.h>
#include <string.h>

#include "asss.h"
#include "banners.h"

/* Interfaces */
local Imodman *mm;
local Iplayerdata *pd;
local Iarenaman *aman;
local Ibanners *banners;

local int hextomem(byte *dest, const char *text, int bytes)
{
    int i;
    for (i = 0; i < bytes; i++)
    {
        byte d = 0;
        const char c1 = *text++;
        const char c2 = *text++;

        if (c1 >= '0' && c1 <= '9')
            d = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f')
            d = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F')
            d = c1 - 'A' + 10;
        else return FALSE;

        d <<= 4;

        if (c2 >= '0' && c2 <= '9')
            d |= c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f')
            d |= c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F')
            d |= c2 - 'A' + 10;
        else return FALSE;

        *dest++ = d;
    }
    return TRUE;
}

/************************************************************************/
/*                           Game Callbacks                             */
/************************************************************************/

/* When a player enters the arena */
local void PlayerAction(Player *p, int action, Arena *arena)
{
    if (!IS_HUMAN(p))
        return;
    
    if (action == PA_ENTERARENA)
    {
        Banner banner;
        const char *t;
        t = "000000222200002222000000000022222222222222220000000022222222222222220000000022222222222222220000000000222222222222000000000000002222222200000000000000000022220000000000000000000000000000000000";
        
        if (hextomem(banner.data, t, sizeof(banner.data)))
            banners->SetBanner(p, &banner, TRUE);
    }
}

/************************************************************************/
/*                           Module Init                                */
/************************************************************************/

EXPORT const char info_doors[] = "Valentines Banners v0.1";

EXPORT int MM_valentines(int action, Imodman *mm_, Arena *arena)
{
    if (action == MM_LOAD)
    {
        mm = mm_;
        
        banners = mm->GetInterface(I_BANNERS, ALLARENAS);
        pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
        aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
        
        if (!banners || !pd || !aman)
            return MM_FAIL;
        
        mm->RegCallback(CB_PLAYERACTION, PlayerAction, ALLARENAS);
        
        return MM_OK;
    }
    else if (action == MM_UNLOAD)
    {
        mm->UnregCallback(CB_PLAYERACTION, PlayerAction, ALLARENAS);
        
        mm->ReleaseInterface(aman);
        mm->ReleaseInterface(pd);
        mm->ReleaseInterface(banners);
        
        return MM_OK;
    }
    return MM_FAIL;
}
