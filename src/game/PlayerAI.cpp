/*
 * Copyright (C) 2005-2008 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2008-2017 Hellground <http://wow-hellground.com/>
 *
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

#include "PlayerAI.h"
#include "Player.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"

class Player;
struct SpellEntry;

void PlayerAI::Remove()
{
    me->SetAI(nullptr);
    delete this;
}

Unit* PlayerAI::SelectNewVictim()
{
    std::list<Player*> targets;
    Hellground::AnyPlayerInObjectRangeCheck u_check(me, 100);
    Hellground::ObjectListSearcher<Player, Hellground::AnyPlayerInObjectRangeCheck> searcher(targets, u_check);

    Cell::VisitAllObjects(me, searcher, 100);
    // no appropriate targets, try non player
    if (targets.empty())
        return me->SelectNearbyTarget(100, nullptr, false, true, true);

    // if there are any non-charmed players around attack them first
    bool anyenemy = false;
    for (std::list<Player*>::iterator itr = targets.begin(); itr != targets.end(); itr++)
    {
        if (!(*itr)->IsFriendlyTo(me))
        {
            anyenemy = true;
            break;
        }
    }
    if (anyenemy) // non charmed found, remove friendly to us
    {
        for (std::list<Player*>::iterator itr = targets.begin(); itr != targets.end();)
        {
            if ((*itr)->IsFriendlyTo(me))
                itr = targets.erase(itr);
            else
                itr++;
        }
    }

    // select random
    uint32 rIdx = urand(0, targets.size() - 1);
    std::list<Player*>::const_iterator tcIter = targets.begin();
    for (uint32 i = 0; i < rIdx; ++i)
        ++tcIter;

    return *tcIter;
}

bool PlayerAI::UpdateVictim(float range)
{
    if (Unit *charmer = me->GetCharmer())
    {
        if (charmer->IsAlive())
        {
            if (!me->GetVictim() || !me->GetVictim()->IsAlive())
            {
                if (Unit *victim = SelectNewVictim())
                    AttackStart(victim);
                else
                    if (charmer->GetVictim())
                        AttackStart(charmer->GetVictim());
            }
        }
        else
        {
            me->RemoveCharmAuras();
            return false;
        }
    }
    else
    {
        me->RemoveCharmAuras();
        return false;
    }

    if (Unit *target = me->GetVictim())
        me->SetInFront(target);

    return me->GetVictim();
}

void WarriorAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;

    if (TC_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), TCSpell, false))
        {
            me->CastSpell(me->GetVictim(), TCSpell, false);
            TC_Timer = TCSpell->RecoveryTime ? TCSpell->RecoveryTime : 10000;
        }
    }

    if (Bloodrage_Timer.Expired(diff))
    {
        if (CanCast(me, BloodrageSpell, false))
        {
            me->CastSpell(me, BloodrageSpell, false);
            Bloodrage_Timer = 60000;
        }
    }


    if (Demo_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), DemoSpell, false))
        {
            me->CastSpell(me->GetVictim(), DemoSpell, false);
            Demo_Timer = DemoSpell->RecoveryTime ? DemoSpell->RecoveryTime : 12000;
        }
    }

}

void HunterAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;

    if (Steady_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), SteadySpell, false))
        {
            me->CastSpell(me->GetVictim(), SteadySpell, false);
            Steady_Timer = 2000 + urand(0, 4000);
        }
    }


    if (Arcane_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), ArcaneSpell, false))
        {
            me->CastSpell(me->GetVictim(), ArcaneSpell, false);
            Arcane_Timer = 6000 + urand(0, 6000);
        }
    }



    if (Multi_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), MultiSpell, false))
        {
            me->CastSpell(me->GetVictim(), MultiSpell, false);
            Multi_Timer = MultiSpell->RecoveryTime ? MultiSpell->RecoveryTime : 12000;
        }
    }



    if (Volley_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), VolleySpell, false))
        {
            me->CastSpell(me->GetVictim(), VolleySpell, false);
            Volley_Timer = VolleySpell->RecoveryTime ? VolleySpell->RecoveryTime : 10000;
        }
    }



    if (Rapid_Timer.Expired(diff))
    {
        if (CanCast(me, RapidSpell, false))
        {
            me->CastSpell(me, RapidSpell, false);
            Rapid_Timer = 60000;
        }
    }


    if (Bestial_Timer.Expired(diff))
    {
        if (CanCast(me, BestialSpell, false))
        {
            me->CastSpell(me, BestialSpell, false);
            Bestial_Timer = 120000;
        }
    }



    if (Auto_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), AutoSpell, false))
        {
            me->CastSpell(me->GetVictim(), AutoSpell, false);
            Auto_Timer = 4000;
        }
    }

}

void PaladinAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;


    if (Avenging_Timer.Expired(diff))
    {
        if (CanCast(me, AvengingSpell, false))
        {
            me->CastSpell(me, AvengingSpell, false);
            Avenging_Timer = 300000;
        }
    }


    
    if (crusader != false && Crusader_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), CrusaderSpell, false))
        {
            me->CastSpell(me->GetVictim(), CrusaderSpell, false);
            Crusader_Timer = 6000 + urand(0, 6000);
        }
    }



    if (Consecration_Timer.Expired(diff))
    {
        if (CanCast(me, ConsecrationSpell, false))
        {
            me->CastSpell(me, ConsecrationSpell, false);
            Consecration_Timer = ConsecrationSpell->RecoveryTime ? ConsecrationSpell->RecoveryTime : 12000;
        }
    }



    if (Judgement_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), JudgementSpell, false))
        {
            me->CastSpell(me->GetVictim(), JudgementSpell, false);
            Judgement_Timer = JudgementSpell->RecoveryTime ? JudgementSpell->RecoveryTime : 30000;
        }
    }



    if (shock != false && Shock_Timer.Expired(diff))
    {
        if (CanCast(me->GetCharmer(), ShockSpell, false))
        {
            me->CastSpell(me->GetCharmer(), ShockSpell, false);
            Shock_Timer = 10000 + urand(0, 10000);
        }
    }



    if (Flash_Timer.Expired(diff))
    {
        if (CanCast(me->GetCharmer(), FlashSpell, false))
        {
            me->CastSpell(me->GetCharmer(), FlashSpell, false);
            Flash_Timer = urand(4000, 10000);
        }
    }

}

void WarlockAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;



    if (Fear_Timer.Expired(diff))
    {
        if (CanCast(me, FearSpell, false))
        {
            me->CastSpell(me, FearSpell, false);
            Fear_Timer = FearSpell->RecoveryTime ? FearSpell->RecoveryTime : 15000;
        }
    }



    if (DOT_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), DOTSpell, false))
        {
            me->CastSpell(me->GetVictim(), DOTSpell, false);
            DOT_Timer = 15000;
        }
    }


    if (AOE_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), AOESpell, false))
        {
            me->CastSpell(me->GetVictim(), AOESpell, false);
            AOE_Timer = AOESpell->RecoveryTime ? AOESpell->RecoveryTime : 10000;
        }
    }


    if (NormalSpell_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), NormalSpell, false))
        {
            me->CastSpell(me->GetVictim(), NormalSpell, false);
            NormalSpell_Timer = NormalSpell->RecoveryTime;
        }
    }

}

void DruidAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;

    if (Heal_Timer.Expired(diff) && me->HasAura(TREE, 0))
    {
        int heal = urand(0, 2);
        if (heal == 0 && CanCast(me->GetCharmer(), Heal1Spell, false))
        {
            me->CastSpell(me->GetCharmer(), Heal1Spell, false);
            Heal_Timer = 9000;
        }
        else if (heal == 1 && CanCast(me->GetCharmer(), Heal2Spell, false))
        {
            me->CastSpell(me->GetCharmer(), Heal2Spell, false);
            Heal_Timer = 6000;
        }
        else if (heal == 2 && CanCast(me->GetCharmer(), Heal3Spell, false))
        {
            me->CastSpell(me->GetCharmer(), Heal3Spell, false);
            Heal_Timer = 12000;
        }
    }



    if (Dmg_Timer.Expired(diff) && me->HasAura(MOONKIN, 0))
    {
        int dmg = urand(0, 2);
        if (dmg == 0 && CanCast(me->GetVictim(), Dmg1Spell, false))
        {
            me->CastSpell(me->GetVictim(), Dmg1Spell, false);
            Dmg_Timer = 3000 + urand(0, 2000);
        }
        else if (dmg == 1 && CanCast(me->GetVictim(), Dmg2Spell, false))
        {
            me->CastSpell(me->GetVictim(), Dmg2Spell, false);
            Dmg_Timer = 3000 + urand(0, 3000);
        }
        else if (dmg == 2 && CanCast(me->GetVictim(), Dmg3Spell, false))
        {
            me->CastSpell(me->GetVictim(), Dmg3Spell, false);
            Dmg_Timer = 3000 + urand(0, 2000);
        }
    }


    if (feral != false && MangleB_Timer.Expired(diff) && me->HasAura(DIREBEAR, 0))
    {
        if (CanCast(me->GetVictim(), MangleBSpell, false))
        {
            me->CastSpell(me->GetVictim(), MangleBSpell, false);
            MangleB_Timer = 7000;
        }
    }


    if (feral != false && Demo_Timer.Expired(diff) && me->HasAura(DIREBEAR, 0))
    {
        if (CanCast(me, DemoSpell, false))
        {
            me->CastSpell(me, DemoSpell, false);
            Demo_Timer = 9000;
        }
    }



    if (feral != false && MangleC_Timer.Expired(diff) && me->HasAura(CAT, 0))
    {
        if (CanCast(me->GetVictim(), MangleCSpell, false))
        {
            me->CastSpell(me->GetVictim(), MangleCSpell, false);
            MangleC_Timer = 6000;
        }
    }


}

void RogueAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;


    if (blade != false && Flurry_Timer.Expired(diff))
    {
        if (CanCast(me, FlurrySpell, false))
        {
            me->CastSpell(me, FlurrySpell, false);
            Flurry_Timer = 120000;
        }
    }



    if (Gouge_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), GougeSpell, false))
        {
            me->CastSpell(me->GetVictim(), GougeSpell, false);
            Gouge_Timer = urand(10000, 30000);
        }
    }



    if (Sinister_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), SinisterSpell, false))
        {
            me->CastSpell(me->GetVictim(), SinisterSpell, false);
            Sinister_Timer = urand(2000, 4500);
        }
    }

}

void ShamanAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;

    if (BL_Timer.Expired(diff))
    {
        if (CanCast(me, BLSpell, false))
        {
            me->CastSpell(me->GetCharmer(), BLSpell, false);
            BL_Timer = BLSpell->RecoveryTime;
        }
    }



    if (Shield_Timer.Expired(diff))
    {
        if (!heal && (CanCast(me, ShieldSpell, false)))
        {
            me->CastSpell(me, ShieldSpell, false);
            Shield_Timer = 30000;
        }
        else if (CanCast(me->GetCharmer(), ShieldSpell, false))
        {
            me->CastSpell(me->GetCharmer(), ShieldSpell, false);
            Shield_Timer = 30000;
        }
    }



    if (Heal_Timer.Expired(diff))
    {
        if (CanCast(me->GetCharmer(), HealSpell, false))
        {
            me->CastSpell(me->GetCharmer(), HealSpell, false);
            Heal_Timer = 10000;
        }
    }



    if (Lightning_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), LightningSpell, false))
        {
            me->CastSpell(me->GetVictim(), LightningSpell, false);
            Lightning_Timer = LightningSpell->RecoveryTime;
        }
    }

}

void PriestAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;


    if ((vampiric == true) && Vampiric_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), VampiricSpell, false))
        {
            me->CastSpell(me->GetVictim(), VampiricSpell, false);
            Vampiric_Timer = 30000;
        }
    }



    if (DmgSpell_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), DmgSpell, false))
        {
            me->CastSpell(me->GetVictim(), DmgSpell, false);
            DmgSpell_Timer = 2500;
            if (vampiric == true) DmgSpell_Timer = 3500;
        }
    }



    if (Flash_Timer.Expired(diff))
    {
        if (CanCast(me->GetCharmer(), FlashSpell, false))
        {
            me->RemoveAurasDueToSpell(15473);
            me->CastSpell(me->GetCharmer(), FlashSpell, false);
            if (!vampiric)
                Flash_Timer = 3000;
            else
                Flash_Timer = 10000;
        }
    }


    if (holynova && Nova_Timer.Expired(diff))
    {
        if (CanCast(me, NovaSpell, false))
        {
            me->CastSpell(me, NovaSpell, false);
            Nova_Timer = 5000;
        }
    }



    if (DOTSpell_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), DOTSpell, false))
        {
            me->CastSpell(me->GetVictim(), DOTSpell, false);
            DOTSpell_Timer = 15000;
            if (vampiric == true) DOTSpell_Timer = 3000;
        }
    }



    if (PWShield_Timer.Expired(diff))
    {
        if (CanCast(me->GetCharmer(), PWShieldSpell, false))
        {
            me->CastSpell(me->GetCharmer(), PWShieldSpell, false);
            PWShield_Timer = 16000;
        }
    }

}

void MageAI::UpdateAI(const uint32 diff)
{
    if (!UpdateVictim())
        return;

    if (!Special && (me->GetPower(POWER_MANA) * 100 / me->GetMaxPower(POWER_MANA) < 20))
    {
        if (CanCast(me, SpecialSpell, false))
        {
            me->CastSpell(me, SpecialSpell, false);
            Special = true;
        }
    }


    if (MassiveAOE_Timer.Expired(diff))
    {
        if (Unit *target = me->SelectNearbyTarget(25.0))
            if (CanCast(target, MassiveAOESpell, false))
            {
                me->CastSpell(target, MassiveAOESpell, false);
                MassiveAOE_Timer = 20000 + rand() % 7000;
            }
    }



    if (ConeSpell_Timer.Expired(diff))
    {
        if (CanCast(me, ConeSpell, false))
        {
            me->CastSpell(me->GetVictim(), ConeSpell, false);
            ConeSpell_Timer = ConeSpell->RecoveryTime;
        }
    }



    if (AOESpell_Timer.Expired(diff))
    {
        if (CanCast(me, AOESpell, false))
        {
            me->CastSpell(me, AOESpell, false);
            AOESpell_Timer = AOESpell->RecoveryTime ? AOESpell->RecoveryTime : 6000;
        }
    }



    if (NormalSpell_Timer.Expired(diff))
    {
        if (CanCast(me->GetVictim(), NormalSpell, false))
        {
            me->CastSpell(me->GetVictim(), NormalSpell, false);
            NormalSpell_Timer = NormalSpell->RecoveryTime;
        }
    }

}
