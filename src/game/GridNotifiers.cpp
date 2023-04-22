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

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Item.h"
#include "Map.h"
#include "MapManager.h"
#include "Transports.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "SpellAuras.h"

using namespace Hellground;

void VisibleNotifier::SendToSelf()
{
    Player& player = *_camera.GetOwner();
    // at this moment i_clientGUIDs have guids that not iterate at grid level checks
    // but exist one case when this possible and object not out of range: transports
    if (Transport* transport = player.GetTransport())
    {
        for (Transport::PlayerSet::const_iterator itr = transport->GetPassengers().begin(); itr != transport->GetPassengers().end(); ++itr)
        {
            if (vis_guids.find((*itr)->GetGUID()) != vis_guids.end())
            {
                vis_guids.erase((*itr)->GetGUID());

                (*itr)->UpdateVisibilityOf(*itr, &player);
                player.UpdateVisibilityOf(&player, *itr, i_data, i_visibleNow);
            }
        }
    }

    for (Player::ClientGUIDs::const_iterator it = vis_guids.begin(); it != vis_guids.end(); ++it)
    {
        player.m_clientGUIDs.erase(*it);
        i_data.AddOutOfRangeGUID(*it);
        if (IS_PLAYER_GUID(*it))
        {
            Player* plr = ObjectAccessor::FindPlayer(*it);
            if (plr && plr->IsInWorld())
                plr->UpdateVisibilityOf(plr->GetCamera().GetBody(), &player);
        }
    }

    if (!i_data.HasData())
        return;

    WorldPacket packet;
    i_data.BuildPacket(&packet);
    player.SendPacketToSelf(&packet);

    for (std::set<WorldObject*>::const_iterator it = i_visibleNow.begin(); it != i_visibleNow.end(); ++it)
    {
        if ((*it)->GetObjectGuid().IsUnit())
            player.SendInitialVisiblePackets((*it)->ToUnit());
    }
}

void VisibleChangesNotifier::Visit(CameraMapType& m)
{
    for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        iter->getSource()->UpdateVisibilityOf(&_object);
}

void DynamicObjectUpdater::VisitHelper(Unit* target)
{
    if (!target->IsAlive() || target->IsTaxiFlying())
        return;

    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->isTotem())
        return;

    if (!i_dynobject.IsWithinExactDistInMap(target, i_dynobject.GetRadius()))
        return;

    //Check targets for not_selectable unit flag and remove
    if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING | UNIT_FLAG_NOT_SELECTABLE))
        return;

    // Evade target
    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->IsInEvadeMode())
        return;

    //Check player targets and remove if in GM mode or GM invisibility (for not self casting case)
    if (target->GetTypeId() == TYPEID_PLAYER && target != i_check && (((Player*)target)->IsGameMaster() || ((Player*)target)->GetVisibility() == VISIBILITY_OFF))
        return;

    if (i_dynobject.IsAffecting(target))
        return;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(i_dynobject.GetSpellId());

    if (!i_dynobject.m_ignore_los && !i_dynobject.IsWithinLOSInMap(target))
        return;

    uint32 eff_index  = i_dynobject.GetEffIndex();
    if (spellInfo->EffectApplyAuraName[eff_index] == SPELL_AURA_NONE)
        return; // do nothing aura does nothing

    if (spellInfo->EffectImplicitTargetB[eff_index] == TARGET_ENUM_UNITS_FRIEND_AOE_AT_DYNOBJ_LOC
        || spellInfo->EffectImplicitTargetB[eff_index] == TARGET_ENUM_UNITS_FRIEND_AOE_AT_DEST_LOC)
    {
        if (!i_check->IsFriendlyTo(target))
            return;
    }
    else if (i_check->GetTypeId() == TYPEID_PLAYER)
    {
        if (i_check->IsFriendlyTo(target))
            return;
        
        i_check->CombatStart(target, !(spellInfo->AttributesEx3 & SPELL_ATTR_EX3_NO_INITIAL_AGGRO));
    }
    else
    {
        if (!i_check->IsHostileTo(target))
            return;

        i_check->CombatStart(target, !(spellInfo->AttributesEx3 & SPELL_ATTR_EX3_NO_INITIAL_AGGRO));
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

PacketBroadcaster::PacketBroadcaster(WorldObject& src, WorldPacket* msg, Player* except /*= NULL*/, float dist /*= 0.0f*/, bool ownTeam /*= false*/ ) : _source(src), _message(msg), _dist(dist)
{
    if (except)
        playerGUIDS.insert(except->GetGUID());

   _ownTeam = ownTeam && _source.GetObjectGuid().IsPlayer();
}

void PacketBroadcaster::Visit(CameraMapType& m)
{
    for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        if (_dist && !_source.IsWithinDist(iter->getSource()->GetBody(), _dist))
            continue;

        BroadcastPacketTo(iter->getSource()->GetOwner());
    }
}

void PacketBroadcaster::BroadcastPacketTo(Player* player)
{
    if (_ownTeam && _source.ToPlayer()->GetTeam() != player->GetTeam())
        return;

    if (!player->HaveAtClient(&_source))
        return;

    if (playerGUIDS.find(player->GetGUID()) == playerGUIDS.end())
    {
        if (WorldSession* session = player->GetSession())
            session->SendPacket(_message);

        playerGUIDS.insert(player->GetGUID());
    }
}

template<class T>
void ObjectUpdater::Visit(GridRefManager<T> &m)
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

bool AnyUnfriendlyUnitInObjectRangeCheck::operator()(Unit* u)
{
    if (Player* owner = sObjectAccessor.GetPlayer(i_unit->GetCharmerOrOwnerGUID()))
    {
        if (!owner->HaveAtClient(u))
            return false; // pets should be able to attack stealthed unit if only player detected them
    }
    else if (u->m_invisibilityMask && u->m_invisibilityMask & (1 << 10) &&
        !u->canDetectInvisibilityOf(i_unit))
        return false;

    if (u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range) && !i_unit->IsFriendlyTo(u))
        return true;
    else
        return false;
}

bool AnyUnfriendlyUnitInPetAttackRangeCheck::operator()(Unit* u)
{
    if (Player* owner = sObjectAccessor.GetPlayer(i_unit->GetCharmerOrOwnerGUID()))
    {
        if (!owner->HaveAtClient(u))
            return false; // pets should be able to attack stealthed unit if only player detected them
    }
    else if (u->m_invisibilityMask && u->m_invisibilityMask & (1 << 10) &&
        !u->canDetectInvisibilityOf(i_unit))
        return false;

    float dist = 20.0f + i_unit->GetLevel() - u->GetLevel();
    if (dist < 5.0f) dist = 5.0f;
    if (dist > 30.0f) dist = 30.0f;

    if (u->IsAlive() && i_unit->IsWithinDistInMap(u, dist) && !i_unit->IsFriendlyTo(u))
        return true;
    else
        return false;
}

template void ObjectUpdater::Visit<GameObject>(GameObjectMapType &);
template void ObjectUpdater::Visit<DynamicObject>(DynamicObjectMapType &);
