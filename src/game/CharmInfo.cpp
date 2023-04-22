/*
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

#include "CharmInfo.h"

#include "WorldPacket.h"
#include "Player.h"
#include "Creature.h"
#include "PetAI.h"
#include "Spell.h"
#include "SpellMgr.h"

#include "MovementGenerator.h"

CharmInfo::CharmInfo(Unit* unit)
: m_unit(unit), m_CommandState(COMMAND_FOLLOW), m_petnumber(0), m_barInit(false)
{
    for (int i =0; i<4; ++i)
    {
        m_charmspells[i].spellId = 0;
        m_charmspells[i].active = ACT_DISABLED;
    }

    if (m_unit->GetTypeId() == TYPEID_UNIT)
    {
        m_oldReactState = ((Creature*)m_unit)->GetReactState();
        ((Creature*)m_unit)->SetReactState(REACT_PASSIVE);
    }
}

CharmInfo::~CharmInfo()
{
    if (m_unit->GetTypeId() == TYPEID_UNIT)
    {
        ((Creature*)m_unit)->SetReactState(m_oldReactState);
    }
}

void CharmInfo::InitPetActionBar()
{
    if (m_barInit)
        return;

    // the first 3 SpellOrActions are attack, follow and stay
    for (uint32 i = 0; i < 3; i++)
    {
        PetActionBar[i].Type = ACT_COMMAND;
        PetActionBar[i].SpellOrAction = COMMAND_ATTACK - i;

        PetActionBar[i + 7].Type = ACT_REACTION;
        PetActionBar[i + 7].SpellOrAction = COMMAND_ATTACK - i;
    }
    for (uint32 i=0; i < 4; i++)
    {
        PetActionBar[i + 3].Type = ACT_DISABLED;
        PetActionBar[i + 3].SpellOrAction = 0;
    }
    m_barInit = true;
}

void CharmInfo::InitEmptyActionBar(bool withAttack)
{
    if (m_barInit)
        return;

    for (uint32 x = 0; x < 10; ++x)
    {
        PetActionBar[x].Type = ACT_CAST;
        PetActionBar[x].SpellOrAction = 0;
    }
    if (withAttack)
    {
        PetActionBar[0].Type = ACT_COMMAND;
        PetActionBar[0].SpellOrAction = COMMAND_ATTACK;
    }
    m_barInit = true;
}

void CharmInfo::InitPossessCreateSpells()
{
    switch (m_unit->GetEntry()) // special cases
    {
    case 23055: // felguard degrader
        InitEmptyActionBar();
        PetActionBar[1].SpellOrAction = 40220;
        PetActionBar[2].SpellOrAction = 40219;
        PetActionBar[3].SpellOrAction = 40221;
        PetActionBar[4].SpellOrAction = 40497;
        PetActionBar[5].SpellOrAction = 40222;
        PetActionBar[6].SpellOrAction = 40658;
        return;
    case 23109: // vengeful spirit
        InitEmptyActionBar(false);
        PetActionBar[0].SpellOrAction = 40325;
        PetActionBar[2].SpellOrAction = 40157;
        PetActionBar[3].SpellOrAction = 40175;
        PetActionBar[4].SpellOrAction = 40314;
        PetActionBar[6].SpellOrAction = 40322;
        return;
    case 25653: // blue flight
        InitEmptyActionBar(false);
        PetActionBar[0].SpellOrAction = 45862;
        PetActionBar[1].SpellOrAction = 45856;
        PetActionBar[2].SpellOrAction = 45860;
        PetActionBar[4].SpellOrAction = 45848;
        return;
    case 21909: // arcano scoop
        InitEmptyActionBar();
        PetActionBar[5].SpellOrAction = 37919;
        PetActionBar[6].SpellOrAction = 37918;
        PetActionBar[3].SpellOrAction = 37851;
        return;
    }

    InitEmptyActionBar();

    if (m_unit->GetTypeId() == TYPEID_UNIT)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            uint32 spellid = ((Creature*)m_unit)->m_spells[i];
            if (SpellMgr::IsPassiveSpell(spellid))
                m_unit->CastSpell(m_unit, spellid, true);
            else
            {
                // add spell only if there are cooldown or global cooldown // TODO: find proper solution
                const SpellEntry * tmpSpellEntry = sSpellStore.LookupEntry(spellid);
                if (tmpSpellEntry && (tmpSpellEntry->RecoveryTime || tmpSpellEntry->StartRecoveryTime || tmpSpellEntry->CategoryRecoveryTime))
                    AddSpellToActionBar(0, spellid, ACT_CAST);
            }
        }
    }
}

void CharmInfo::InitCharmCreateSpells()
{
    if (m_unit->GetTypeId() == TYPEID_PLAYER)                //charmed players don't have spells
    {
        InitEmptyActionBar();
        return;
    }

    InitPetActionBar();

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 spellId = ((Creature*)m_unit)->m_spells[x];
        m_charmspells[x].spellId = spellId;

        if (!spellId)
            continue;

        if (SpellMgr::IsPassiveSpell(spellId))
        {
            m_unit->CastSpell(m_unit, spellId, true);
            m_charmspells[x].active = ACT_PASSIVE;
        }
        else
        {
            ActiveStates newstate;
            bool onlyselfcast = true;
            SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);

            if (spellInfo)
            {
                for (uint32 i = 0; i < 3; ++i)       //non existent spell will not make any problems as onlyselfcast would be false -> break right away
                {
                    if (spellInfo->EffectImplicitTargetA[i] != TARGET_UNIT_CASTER && spellInfo->EffectImplicitTargetA[i] != 0)
                    {
                        onlyselfcast = false;
                        break;
                    }
                }
            }
            else
                onlyselfcast = false;

            if (onlyselfcast || !SpellMgr::IsPositiveSpell(spellId))   //only self cast and spells versus enemies are autocastable
                newstate = ACT_DISABLED;
            else
                newstate = ACT_CAST;

            // add spell only if there are cooldown or global cooldown // TODO: find proper solution
            if (spellInfo && (spellInfo->RecoveryTime || spellInfo->StartRecoveryTime || spellInfo->CategoryRecoveryTime))
                AddSpellToActionBar(0, spellId, newstate);
        }
    }
}

bool CharmInfo::AddSpellToActionBar(uint32 oldid, uint32 newid, ActiveStates newstate)
{
    for (uint8 i = 0; i < 10; i++)
    {
        if ((PetActionBar[i].Type == ACT_DISABLED || PetActionBar[i].Type == ACT_ENABLED || PetActionBar[i].Type == ACT_CAST) && PetActionBar[i].SpellOrAction == oldid)
        {
            PetActionBar[i].SpellOrAction = newid;
            if (!oldid)
            {
                if (newstate == ACT_DECIDE)
                    PetActionBar[i].Type = ACT_DISABLED;
                else
                    PetActionBar[i].Type = newstate;
            }

            return true;
        }
    }
    return false;
}

void CharmInfo::ToggleCreatureAutocast(uint32 spellid, bool apply)
{
    if (SpellMgr::IsPassiveSpell(spellid))
        return;

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if (spellid == m_charmspells[x].spellId)
        {
            m_charmspells[x].active = apply ? ACT_ENABLED : ACT_DISABLED;
        }
    }
}

void CharmInfo::SetPetNumber(uint32 petnumber, bool statwindow)
{
    m_petnumber = petnumber;
    if (statwindow)
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, m_petnumber);
    else
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, 0);
}

void CharmInfo::HandleStayCommand()
{
    SetCommandState(COMMAND_STAY);

    m_unit->AttackStop();
    m_unit->InterruptNonMeleeSpells(false);

    m_unit->GetMotionMaster()->StopControlledMovement();
}

void CharmInfo::HandleFollowCommand()
{
    if (m_unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
        return;

    SetCommandState(COMMAND_FOLLOW);

    m_unit->AttackStop();
    m_unit->InterruptNonMeleeSpells(false);

    m_unit->GetMotionMaster()->MoveFollow(m_unit->GetCharmerOrOwner(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    if (Creature* pCharm = m_unit->ToCreature())
    {
        if (PetAI* petai = dynamic_cast<PetAI*>(pCharm->AI()))
            petai->clearEnemySet();
    }
}

void CharmInfo::HandleAttackCommand(uint64 targetGUID)
{
    Unit* pOwner = m_unit->GetCharmerOrOwner();
    // Can't attack if owner is pacified
    if (pOwner->HasAuraType(SPELL_AURA_MOD_PACIFY))
         return;

    // only place where pet can be player
    Unit *pTarget = pOwner->GetUnit(targetGUID);
    if (!pTarget)
        return;

    if (!m_unit->canAttack(pTarget, true))
         return;

    // Not let attack through obstructions
    if (sWorld.getConfig(CONFIG_PET_LOS) && !m_unit->IsWithinLOSInMap(pTarget))
        return;

    if (Creature* pCharm = m_unit->ToCreature())
    {
        if (PetAI* petai = dynamic_cast<PetAI*>(pCharm->AI()))
        {
            petai->ForcedAttackStart(pTarget);
        }
        else
            pCharm->AI()->AttackStart(pTarget);

        Pet *pPet = m_unit->ToPet();
        if (pPet && pPet->getPetType() == SUMMON_PET && roll_chance_i(10))
        {
            // 10% chance for special talk
            pPet->SendPetTalk(uint32(PET_TALK_ATTACK));
            return;
        }
    }
    else
    {
        if (m_unit->getVictimGUID() != targetGUID)
             m_unit->AttackStop();

        m_unit->Attack(pTarget, true);
    }

    m_unit->SendPetAIReaction(targetGUID);
}

void CharmInfo::HandleSpellActCommand(uint64 targetGUID, uint32 spellId)
{
    // do not cast unknown spells
    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return;

    Player* plCharmer = m_unit->GetCharmerOrOwnerPlayerOrPlayerItself();
    // Global Cooldown, stop cast
    if (plCharmer && spellInfo->StartRecoveryCategory > 0 &&
        plCharmer->GetCooldownMgr().HasGlobalCooldown(PETS_GCD_CATEGORY))
        return;

    for (uint32 i = 0; i < 3;i++)
         if (spellInfo->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC || spellInfo->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC || spellInfo->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC)
             return;

    // do not cast not learned spells
    if (!m_unit->HasSpell(spellId) || SpellMgr::IsPassiveSpell(spellId))
        return;

    uint64 charmerGUID = m_unit->GetCharmerGUID();

    Spell *spell = new Spell(m_unit, spellInfo, spellId == 33395, charmerGUID);

    Unit* pTarget = m_unit->GetUnit(targetGUID);

    SpellCastResult result = spell->CheckPetCast(pTarget);

    // auto turn to target unless possessed
    if (result == SPELL_FAILED_UNIT_NOT_INFRONT && !m_unit->isPossessed())
    {
        if (Unit *pTarget2 = targetGUID ? pTarget : spell->m_targets.getUnitTarget())
            m_unit->SetFacingToObject(pTarget2);

        result = SPELL_CAST_OK;
    }

    if (result == SPELL_CAST_OK)
    {
        Creature* pCreature = m_unit->ToCreature();
        if (Pet* pPet = m_unit->ToPet())
        {
            pPet->CheckLearning(spellId);
            if (pPet->getPetType() == SUMMON_PET && roll_chance_i(10))
                pPet->SendPetTalk(uint32(PET_TALK_SPECIAL_SPELL));
            else
                pPet->SendPetAIReaction(charmerGUID);
        }
        else
            m_unit->SendPetAIReaction(charmerGUID);

        Unit *pSpellTarget = spell->m_targets.getUnitTarget();

        if (pSpellTarget && !m_unit->isPossessed() && !m_unit->GetCharmerOrOwner()->IsFriendlyTo(pSpellTarget))
        {
            if (m_unit->GetVictim())
                m_unit->AttackStop();

            if (pCreature->IsAIEnabled)
                pCreature->AI()->AttackStart(pSpellTarget);
        }

        //m_unit->GetMotionMaster()->StopMovement();
        //m_unit->GetMotionMaster()->MovementExpired(false);

        spell->prepare(&(spell->m_targets));
    }
    else
    {
        if (m_unit->isPossessed())
        {
            WorldPacket data(SMSG_CAST_FAILED, (4+1+1));
            data << uint32(spellId);
            data << uint8(2);
            data << uint8(result);

            switch (result)
            {
                case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
                    data << uint32(spellInfo->RequiresSpellFocus);
                    break;
                case SPELL_FAILED_REQUIRES_AREA:
                    data << uint32(spellInfo->AreaId);
                    break;
            }

            Player *pPlayer = m_unit->GetCharmer()->ToPlayer();
            pPlayer->SendPacketToSelf(&data);
        }
        else
            m_unit->SendPetCastFail(spellId, result);


        spell->finish(false);
        delete spell;
    }
}
