/* Copyright (C) 2006 - 2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* ScriptData
SDName: Guard_AI
SD%Complete: 90
SDComment:
SDCategory: Guards
EndScriptData */

#include "precompiled.h"
#include "guard_ai.h"

// **** This script is for use within every single guard to save coding time ****

#define GENERIC_CREATURE_COOLDOWN 5000

void guardAI::Reset()
{
    GlobalCooldown = 0;
    BuffTimer = 0;                                          //Rebuff as soon as we can
}

void guardAI::EnterCombat(Unit *who)
{
    if (m_creature->GetEntry() == 15184)
    {
        switch(rand()%3)
        {
        case 0:
            DoSay("Taste blade, mongrel!", LANG_UNIVERSAL,NULL);
            break;
        case 1:
            DoSay("Please tell me that you didn't just do what I think you just did. Please tell me that I'm not going to have to hurt you...", LANG_UNIVERSAL,NULL);
            break;
        case 2:
            DoSay("As if we don't have enough problems, you go and create more!", LANG_UNIVERSAL,NULL);
            break;
        }
    }

    if (SpellEntry const *spell = m_creature->reachWithSpellAttack(who))
        DoCastSpell(who, spell);
}

void guardAI::JustDied(Unit *Killer)
{
    //Send Zone Under Attack message to the LocalDefense and WorldDefense Channels
    if (Player* pKiller = Killer->GetCharmerOrOwnerPlayerOrPlayerItself())
        m_creature->SendZoneUnderAttackMessage(pKiller);
}

void guardAI::UpdateAI(const uint32 diff)
{
    //Always decrease our global cooldown first
    if (GlobalCooldown > diff)
        GlobalCooldown -= diff;
    else GlobalCooldown = 0;

    //Buff timer (only buff when we are alive and not in combat
    if (m_creature->isAlive() && !m_creature->isInCombat())
        if (BuffTimer < diff )
    {
        //Find a spell that targets friendly and applies an aura (these are generally buffs)
        SpellEntry const *info = SelectSpell(m_creature, -1, -1, SELECT_TARGET_ANY_FRIEND, 0, 0, 0, 0, SELECT_EFFECT_AURA);

        if (info && !GlobalCooldown)
        {
            //Cast the buff spell
            DoCastSpell(m_creature, info);

            //Set our global cooldown
            GlobalCooldown = GENERIC_CREATURE_COOLDOWN;

            //Set our timer to 10 minutes before rebuff
            BuffTimer = 600000;
        }                                                   //Try agian in 30 seconds
        else
            BuffTimer = 30000;
    }
    else
        BuffTimer -= diff;

    //Return since we have no target
    if (!UpdateVictim())
        return;

    DoMeleeAttackIfReady();
}

