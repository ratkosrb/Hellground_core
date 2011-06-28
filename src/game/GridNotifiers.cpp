/*
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * Copyright (C) 2008 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "GridNotifiers.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Item.h"
#include "Map.h"
#include "Transports.h"
#include "ObjectAccessor.h"
#include "SpellAuras.h"

using namespace Trinity;

void VisibleChangesNotifier::Visit(CameraMapType &m)
{
    for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        iter->getSource()->UpdateVisibilityOf(&i_object);
}

void VisibleNotifier::Notify()
{
    Player& player = *i_camera.GetOwner();
    // at this moment i_clientGUIDs have guids that not iterate at grid level checks
    // but exist one case when this possible and object not out of range: transports
    if(Transport* transport = player.GetTransport())
    {
        for(Transport::PlayerSet::const_iterator itr = transport->GetPassengers().begin();itr!=transport->GetPassengers().end();++itr)
        {
            if(i_clientGUIDs.find((*itr)->GetGUID())!=i_clientGUIDs.end())
            {
                // ignore far sight case
                (*itr)->UpdateVisibilityOf(*itr, &player);
                player.UpdateVisibilityOf(&player, *itr, i_data, i_visibleNow);
                i_clientGUIDs.erase((*itr)->GetGUID());
            }
        }
    }

    // generate outOfRange for not iterate objects
    i_data.AddOutOfRangeGUID(i_clientGUIDs);
    for (Player::ClientGUIDs::iterator itr = i_clientGUIDs.begin(); itr != i_clientGUIDs.end(); ++itr)
        player.m_clientGUIDs.erase(*itr);

    if (i_data.HasData())
    {
        // send create/outofrange packet to player (except player create updates that already sent using SendUpdateToPlayer)
        WorldPacket packet;
        i_data.BuildPacket(&packet);
        player.GetSession()->SendPacket(&packet);

        // send out of range to other players if need
        Player::ClientGUIDs const& oor = i_data.GetOutOfRangeGUIDs();
        for (Player::ClientGUIDs::const_iterator iter = oor.begin(); iter != oor.end(); ++iter)
        {
            if (!IS_PLAYER_GUID(*iter))
                continue;

            if (Player* plr = ObjectAccessor::FindPlayer(*iter))
                plr->UpdateVisibilityOf(plr->GetCamera().GetBody(), &player);
        }
    }

    // Now do operations that required done at object visibility change to visible

    // send data at target visibility change (adding to client)
    for (std::set<WorldObject*>::const_iterator vItr = i_visibleNow.begin(); vItr != i_visibleNow.end(); ++vItr)
    {
        // target aura duration for caster show only if target exist at caster client
        if ((*vItr) != &player && (*vItr)->isType(TYPEMASK_UNIT))
            player.SendAuraDurationsForTarget((Unit*)(*vItr));

        // non finished movements show to player
        if ((*vItr)->GetTypeId()==TYPEID_UNIT && ((Creature*)(*vItr))->isAlive())
        {
            ((Creature*)(*vItr))->SendMonsterMoveWithSpeedToCurrentDestination(&player);

            //comment out this, we will see if we need it :]
            //((Creature*)(*vItr))->SendMeleeAttackStart(((Creature*)(*vItr))->getVictim());
        }
    }
}

void DynamicObjectUpdater::VisitHelper(Unit* target)
{
    if (!target->isAlive() || target->isInFlight())
        return;

    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->isTotem())
        return;

    if (!i_dynobject.IsWithinDistInMap(target, i_dynobject.GetRadius()))
        return;

    //Check targets for not_selectable unit flag and remove
    if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
        return;

    // Evade target
    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->IsInEvadeMode())
        return;

    //Check player targets and remove if in GM mode or GM invisibility (for not self casting case)
    if (target->GetTypeId() == TYPEID_PLAYER && target != i_check && (((Player*)target)->isGameMaster() || ((Player*)target)->GetVisibility() == VISIBILITY_OFF))
        return;

    if (i_dynobject.IsAffecting(target))
        return;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(i_dynobject.GetSpellId());
    uint32 eff_index  = i_dynobject.GetEffIndex();
    if (spellInfo->EffectImplicitTargetB[eff_index] == TARGET_DEST_DYNOBJ_ALLY
        || spellInfo->EffectImplicitTargetB[eff_index] == TARGET_UNIT_AREA_ALLY_DST)
    {
        if (!i_check->IsFriendlyTo(target))
            return;
    }
    else if (i_check->GetTypeId() == TYPEID_PLAYER)
    {
        if (i_check->IsFriendlyTo(target))
            return;

        i_check->CombatStart(target);
    }
    else
    {
        if (!i_check->IsHostileTo(target))
            return;

        i_check->CombatStart(target);
    }

    // Check target immune to spell or aura
    if (target->IsImmunedToSpell(spellInfo) || target->IsImmunedToSpellEffect(spellInfo->Effect[eff_index], spellInfo->EffectMechanic[eff_index]))
        return;

    if (target->preventApplyPersistentAA(spellInfo, eff_index))
        return;

    // Apply PersistentAreaAura on target
    PersistentAreaAura* Aur = new PersistentAreaAura(spellInfo, eff_index, NULL, target, i_dynobject.GetCaster(), NULL, i_dynobject.GetGUID());

    target->AddAura(Aur);
    i_dynobject.AddAffected(target);
}

void DynamicObjectUpdater::Visit(CreatureMapType &m)
{
    for (CreatureMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
        VisitHelper(itr->getSource());
}

void DynamicObjectUpdater::Visit(PlayerMapType &m)
{
    for (PlayerMapType::iterator itr=m.begin(); itr != m.end(); ++itr)
        VisitHelper(itr->getSource());
}

void MessageDeliverer::Visit(CameraMapType &m)
{
    for(CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* owner = iter->getSource()->GetOwner();

        if (i_toSelf || owner != &i_player)
        {
            if (WorldSession* session = owner->GetSession())
                session->SendPacket(i_message);
        }
    }
}

void MessageDelivererExcept::Visit(CameraMapType &m)
{
    for(CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* owner = iter->getSource()->GetOwner();

        if (owner == i_skipped_receiver)
            continue;

        if (WorldSession* session = owner->GetSession())
            session->SendPacket(i_message);
    }
}

void ObjectMessageDeliverer::Visit(CameraMapType &m)
{
    for(CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        if(WorldSession* session = iter->getSource()->GetOwner()->GetSession())
            session->SendPacket(i_message);
    }
}

void MessageDistDeliverer::Visit(CameraMapType &m)
{
    for(CameraMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        Player * owner = iter->getSource()->GetOwner();

        if ((i_toSelf || owner != &i_player) &&
            (!i_ownTeamOnly || owner->GetTeam() == i_player.GetTeam()) &&
            (!i_dist || iter->getSource()->GetBody()->IsWithinDist(&i_player,i_dist)))
        {
            if (WorldSession* session = owner->GetSession())
                session->SendPacket(i_message);
        }
    }
}

void ObjectMessageDistDeliverer::Visit(CameraMapType &m)
{
    for(CameraMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        if (!i_dist || iter->getSource()->GetBody()->IsWithinDist(&i_object,i_dist))
        {
            if (WorldSession* session = iter->getSource()->GetOwner()->GetSession())
                session->SendPacket(i_message);
        }
    }
}

template<class T> void
ObjectUpdater::Visit(GridRefManager<T> &m)
{
    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        if (iter->getSource()->IsInWorld())
        {
            WorldObject::UpdateHelper helper(iter->getSource());
            helper.Update(i_timeDiff); 
        }
    }
}

bool CannibalizeObjectCheck::operator()(Corpse* u)
{
    // ignore bones
    if (u->GetType()==CORPSE_BONES)
        return false;

    Player* owner = ObjectAccessor::FindPlayer(u->GetOwnerGUID());

    if (!owner || i_funit->IsFriendlyTo(owner))
        return false;

    if (i_funit->IsWithinDistInMap(u, i_range))
        return true;

    return false;
}

template void ObjectUpdater::Visit<GameObject>(GameObjectMapType &);
template void ObjectUpdater::Visit<DynamicObject>(DynamicObjectMapType &);
