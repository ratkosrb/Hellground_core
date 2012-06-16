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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "LootMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "World.h"
#include "Util.h"
#include "SharedDefines.h"
#include "Group.h"

static Rates const qualityToRate[MAX_ITEM_QUALITY] = {
    RATE_DROP_ITEM_POOR,                                    // ITEM_QUALITY_POOR
    RATE_DROP_ITEM_NORMAL,                                  // ITEM_QUALITY_NORMAL
    RATE_DROP_ITEM_UNCOMMON,                                // ITEM_QUALITY_UNCOMMON
    RATE_DROP_ITEM_RARE,                                    // ITEM_QUALITY_RARE
    RATE_DROP_ITEM_EPIC,                                    // ITEM_QUALITY_EPIC
    RATE_DROP_ITEM_LEGENDARY,                               // ITEM_QUALITY_LEGENDARY
    RATE_DROP_ITEM_ARTIFACT,                                // ITEM_QUALITY_ARTIFACT
};

// FIXME: remove from world.h RATE_DROP_ITEM_REFERENCED

//
// ----- Loot Store
//


//
// ----- loading loot definition from database
//

void LootStore::Clear()
{
    for (LootableObjectMap::const_iterator itr=m_LootableObjects.begin(); itr != m_LootableObjects.end(); ++itr)
        delete itr->second;
    m_LootableObjects.clear();

    for (LootTemplateMap::const_iterator itr=m_LootTemplates.begin(); itr != m_LootTemplates.end(); ++itr)
        delete itr->second;
    m_LootTemplates.clear();

    for (LootGroupMap::const_iterator itr=m_Groups.begin(); itr != m_Groups.end(); ++itr)
        delete itr->second;
    m_Groups.clear();
}


void LootStore::LoadLootGroupTable()
{
    LootGroupMap::iterator tab;
    uint32 count = 0;

    for (LootGroupMap::const_iterator itr=m_Groups.begin(); itr != m_Groups.end(); ++itr)
        delete itr->second;
    m_Groups.clear();

    //                                                       0      1           2         3       4          5          6          7                 8         
    QueryResultAutoPtr result = WorldDatabase.PQuery("SELECT entry, item_entry, is_quest, chance, min_count, max_count, condition, condition_value1, condition_value2 FROM loot_group_template");

    if(result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            Field *fields = result->Fetch();
            bar.step();

            uint32 entry               = fields[0].GetUInt32();
            uint32 item                = fields[1].GetUInt32();
            bool isQuest               = fields[2].GetBool();
            float chance               = fields[3].GetFloat();
            uint8  minCount            = fields[4].GetUInt8();
            uint8  maxCount            = fields[5].GetUInt8();
            ConditionType condition    = (ConditionType)fields[6].GetUInt8();
            uint32 cond_value1         = fields[7].GetUInt32();
            uint32 cond_value2         = fields[8].GetUInt32();

            if (!PlayerCondition::IsValid(condition,cond_value1, cond_value2))
            {
                sLog.outErrorDb("... in table loot_group_template entry %u item %u", entry, item);
                continue;                                   // error already printed to log/console.
            }

            // (condition + cond_value1/2) are converted into single conditionId
            uint16 conditionId = sObjectMgr.GetConditionId(condition, cond_value1, cond_value2);

            LootStoreItem storeitem = LootStoreItem(item, isQuest, chance, minCount, maxCount, conditionId);

            LootGroupTemplate* group;
            if (m_Groups.find(entry) == m_Groups.end())
            {
                group = new LootGroupTemplate();
                m_Groups[entry] = group;
            }
            else
                group = m_Groups[entry];

            group->AddEntry(storeitem);
            ++count;

        } while (result->NextRow());

        for (LootGroupMap::const_iterator i = m_Groups.begin(); i != m_Groups.end(); i++)
            i->second->Verify(i->first);

        sLog.outString();
        sLog.outString(">> Loaded %u group loot definitions (%d templates)", count, m_Groups.size());
    }
    else
    {
        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 group loot definitions. DB table loot_group_template is empty.");
    }
}

void LootStore::LoadLootTable()
{
    uint32 count = 0;

    for (LootTemplateMap::const_iterator itr=m_LootTemplates.begin(); itr != m_LootTemplates.end(); ++itr)
        delete itr->second;
    m_LootTemplates.clear();

    //                                                       0      1            2       3          4             
    QueryResultAutoPtr result = WorldDatabase.PQuery("SELECT entry, group_entry, chance, min_count, max_count FROM loot_template");

    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            Field *fields = result->Fetch();
            bar.step();

            uint32 entry               = fields[0].GetUInt32();
            uint32 groupEntry          = fields[1].GetUInt32();
            float chance               = fields[2].GetFloat();
            uint8  minCount            = fields[3].GetUInt8();
            uint8  maxCount            = fields[4].GetUInt8();

            LootGroupTemplate *groupTemplate = NULL;
            if (m_Groups.find(groupEntry) != m_Groups.end())
                groupTemplate = m_Groups[groupEntry];
            else
            {
                sLog.outErrorDb("Entry %u in table loot_template references group %u that doesn't exist. Skipped.", entry, groupEntry);
                continue;
            }

            LootStoreGroup group = LootStoreGroup(groupTemplate, chance, minCount, maxCount);

            LootTemplate *lootTemplate;
            if (m_LootTemplates.find(entry) == m_LootTemplates.end())
            {
                lootTemplate = new LootTemplate();
                m_LootTemplates[entry] = lootTemplate;
            }
            else
                lootTemplate = m_LootTemplates[entry];

            lootTemplate->AddEntry(group);

        } while (result->NextRow());

        sLog.outString();
        sLog.outString(">> Loaded %u loot definitions (%d templates)", count, m_LootTemplates.size());
    }
    else
    {
        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 loot definitions. DB table loot_template is empty.");
    }
}

void LootStore::LoadLootTypeTable()
{
    uint32 count = 0;

    for (LootableObjectMap::const_iterator itr=m_LootableObjects.begin(); itr != m_LootableObjects.end(); ++itr)
        delete itr->second;
    m_LootableObjects.clear();

    //                                                       0           1     2
    QueryResultAutoPtr result = WorldDatabase.PQuery("SELECT loot_entry, type, type_entry FROM loot_type_template");

    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            Field *fields = result->Fetch();
            bar.step();

            uint32 lootEntry           = fields[0].GetUInt32();
            LootableType type          = (LootableType)fields[1].GetUInt32();
            uint32 typeEntry           = fields[2].GetUInt32();


            LootTemplate *lootTemplate = NULL;
            if (m_LootTemplates.find(lootEntry) != m_LootTemplates.end())
                lootTemplate = m_LootTemplates[lootEntry];
            else
            {
                sLog.outErrorDb("type_entry %u and type %u in table loot_type_template references loot %u that doesn't exist. Skipped.", typeEntry, type, lootEntry);
                continue;
            }
           
            m_LootableObjects[LootableObject(type, typeEntry)] = lootTemplate;

        } while (result->NextRow());

        sLog.outString();
        sLog.outString(">> Loaded %u loot type definitions", count);
    }
    else
    {
        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 loot definitions. DB table loot_type_template is empty.");
    }
}

// --------------------

bool LootStore::HaveQuestLootfor (LootableType type, uint32 entry) const
{
    LootableObjectMap::const_iterator itr = m_LootableObjects.find(LootableObject(type, entry));
    if (itr == m_LootableObjects.end())
        return false;

    // scan loot for quest items
    return itr->second->HasQuestDrop();
}

bool LootStore::HaveQuestLootForPlayer(LootableType type, uint32 entry, Player* player) const
{
    LootableObjectMap::const_iterator itr = m_LootableObjects.find(LootableObject(type, entry));
    if (itr != m_LootableObjects.end())
        if (itr->second->HasQuestDropForPlayer(player))
            return true;

    return false;
}
 
LootTemplate const* LootStore::GetLootfor (LootableObject lootableObject) const
{
    LootableObjectMap::const_iterator tab = m_LootableObjects.find(lootableObject);

    if (tab == m_LootableObjects.end())
        return NULL;

    return tab->second;
}

//
// --------- LootStoreItem ---------
//

// Checks if the entry (quest, non-quest, reference) takes it's chance (at loot generation)
// RATE_DROP_ITEMS is no longer used for all types of entries
bool LootStoreItem::Roll() const
{
    if (chance>=100.f)
        return true;

    ItemPrototype const *pProto = ObjectMgr::GetItemPrototype(itemid);

    float qualityModifier = pProto ? sWorld.getRate(qualityToRate[pProto->Quality]) : 1.0f;

    return roll_chance_f(chance*qualityModifier);
}

//
// --------- LootItem ---------
//

LootItem::LootItem(uint32 id)
{
    itemid      = id;
    count       = 1;
    conditionId = 0;
    freeforall  = false;
    needs_quest = false;
    randomSuffix = GenerateEnchSuffixFactor(itemid);
    randomPropertyId = Item::GenerateItemRandomPropertyId(itemid);
    is_looted = 0;
    is_blocked = 0;
    is_counted = 0;
}

// Constructor, copies most fields from LootStoreItem and generates random count
LootItem::LootItem(LootStoreItem const& li)
{
    itemid      = li.itemid;
    conditionId = li.conditionId;

    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
    freeforall  = proto && (proto->Flags & ITEM_FLAGS_PARTY_LOOT);

    needs_quest = li.isQuest;

    count       = urand(li.minCount, li.maxCount);     
    randomSuffix = GenerateEnchSuffixFactor(itemid);
    randomPropertyId = Item::GenerateItemRandomPropertyId(itemid);
    is_looted = 0;
    is_blocked = 0;
    is_counted = 0;
}

// Basic checks for player/item compatibility - if false no chance to see the item in the loot
bool LootItem::AllowedForPlayer(Player const * player) const
{
    // DB conditions check
    if (!sObjectMgr.IsPlayerMeetToCondition(player,conditionId))
        return false;

    if (needs_quest)
    {
        // Checking quests for quest-only drop (check only quests requirements in this case)
        if (!player->HasQuestForItem(itemid))
            return false;
    }
    else
    {
        // Not quest only drop (check quest starting items for already accepted non-repeatable quests)
        ItemPrototype const *pProto = ObjectMgr::GetItemPrototype(itemid);
        if (pProto && pProto->StartQuest && player->GetQuestStatus(pProto->StartQuest) != QUEST_STATUS_NONE && !player->HasQuestForItem(itemid))
            return false;
    }

    return true;
}

//
// --------- Loot ---------
//

// Inserts the item into the loot (called by LootTemplate processors)
void Loot::AddItem(LootStoreItem const & item)
{
    if (item.isQuest)                                   // Quest drop
    {
        if (quest_items.size() < MAX_NR_QUEST_ITEMS)
            quest_items.push_back(LootItem(item));
        everyone_can_open = true;
    }
    else if (items.size() < MAX_NR_LOOT_ITEMS)              // Non-quest drop
    {
        items.push_back(LootItem(item));

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item.itemid);

        if (max_quality < proto->Quality)
            max_quality = (ItemQualities)proto->Quality;

        if(proto->Flags & ITEM_FLAGS_PARTY_LOOT || item.conditionId)
            everyone_can_open = true;

        // items with quality >= RARE are unique in loot, except for epic junk (-> tier tokens)
        if (proto->Quality >= ITEM_QUALITY_RARE &&
            (proto->Quality != ITEM_QUALITY_EPIC || proto->Class != ITEM_CLASS_JUNK))
            unique_items.insert(item.itemid);

        // non-conditional one-player only items are counted here,
        // free for all items are counted in FillFFALoot(),
        // non-ffa conditionals are counted in FillNonQuestNonFFAConditionalLoot()
        if (!item.conditionId)
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item.itemid);
            if (!proto || (proto->Flags & ITEM_FLAGS_PARTY_LOOT)==0)
                ++unlootedCount;
        }            
    }
}

bool Loot::IsPlayerAllowedToLoot(Player *player, WorldObject *object)
{
    return players_allowed_to_loot.empty() ?
        player->IsWithinDist(object, sWorld.getConfig(CONFIG_GROUP_XP_DISTANCE), false) :
        players_allowed_to_loot.find(player->GetGUID()) != players_allowed_to_loot.end();
}

void Loot::setCreatureGUID(Creature *pCreature)
{
    if (!pCreature)
        return;

    pCreature->FillPlayersAllowedToLoot(&players_allowed_to_loot);

    if (!pCreature->isWorldBoss())
        return;

    if (!pCreature->GetMap()->IsDungeon())
        return;

    m_creatureGUID = pCreature->GetGUID();
    m_mapID = MapID(pCreature->GetMapId(), pCreature->GetInstanceId());
}

// Calls processor of corresponding LootTemplate (which handles everything including references)
void Loot::FillLoot(LootableType type, uint32 entry, Player* loot_owner, bool personal)
{
    LootTemplate const* tab = sLootStore.GetLootfor(LootableObject(type, entry));

    if (!tab)
    {
        sLog.outErrorDb("No loot definition for loot type %d with entry %d.", type, entry);
        return;
    }

    items.reserve(MAX_NR_LOOT_ITEMS);
    quest_items.reserve(MAX_NR_QUEST_ITEMS);
    tab->Process(*this);                            // Processing is done there, callback via Loot::AddItem()

    if (loot_owner)                                 // loot_owner not provided for creatures with no loot recipient on death
    {
        // Setting access rights for group loot case
        Group * pGroup = loot_owner->GetGroup();
        if (!personal && pGroup)
        {
            for (GroupReference *itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                if (Player* pl = itr->getSource())
                    FillNotNormalLootFor(pl);
        }
        // ... for personal loot
        else
            FillNotNormalLootFor(loot_owner);

        if (m_creatureGUID)
            saveLootToDB(loot_owner);
    }
}

void Loot::FillNotNormalLootFor(Player* pl)
{
    uint32 plguid = pl->GetGUIDLow();

    QuestItemMap::const_iterator qmapitr = PlayerQuestItems.find(plguid);
    if (qmapitr == PlayerQuestItems.end())
        FillQuestLoot(pl);

    qmapitr = PlayerFFAItems.find(plguid);
    if (qmapitr == PlayerFFAItems.end())
        FillFFALoot(pl);

    qmapitr = PlayerNonQuestNonFFAConditionalItems.find(plguid);
    if (qmapitr == PlayerNonQuestNonFFAConditionalItems.end())
        FillNonQuestNonFFAConditionalLoot(pl);
}

QuestItemList* Loot::FillFFALoot(Player* player)
{
    QuestItemList *ql = new QuestItemList();

    for (uint8 i = 0; i < items.size(); i++)
    {
        LootItem &item = items[i];
        if (!item.is_looted && item.freeforall && item.AllowedForPlayer(player))
        {
            ql->push_back(QuestItem(i));
            ++unlootedCount;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    PlayerFFAItems[player->GetGUIDLow()] = ql;
    return ql;
}

QuestItemList* Loot::FillQuestLoot(Player* player)
{
    if (items.size() == MAX_NR_LOOT_ITEMS)
        return NULL;

    QuestItemList *ql = new QuestItemList();

    for (uint8 i = 0; i < quest_items.size(); i++)
    {
        LootItem &item = quest_items[i];
        if (!item.is_looted && item.AllowedForPlayer(player))
        {
            ql->push_back(QuestItem(i));

            // questitems get blocked when they first apper in a
            // player's quest vector
            //
            // increase once if one looter only, looter-times if free for all
            if (item.freeforall || !item.is_blocked)
                ++unlootedCount;

            item.is_blocked = true;

            if (items.size() + ql->size() == MAX_NR_LOOT_ITEMS)
                break;
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    PlayerQuestItems[player->GetGUIDLow()] = ql;
    return ql;
}

QuestItemList* Loot::FillNonQuestNonFFAConditionalLoot(Player* player)
{
    QuestItemList *ql = new QuestItemList();

    for (uint8 i = 0; i < items.size(); ++i)
    {
        LootItem &item = items[i];
        if (!item.is_looted && !item.freeforall && item.conditionId && item.AllowedForPlayer(player))
        {
            ql->push_back(QuestItem(i));
            if (!item.is_counted)
            {
                ++unlootedCount;
                item.is_counted=true;
            }
        }
    }
    if (ql->empty())
    {
        delete ql;
        return NULL;
    }

    PlayerNonQuestNonFFAConditionalItems[player->GetGUIDLow()] = ql;
    return ql;
}

//
// System for saving loot to DB
//

void Loot::FillLootFromDB(Creature *pCreature, Player* pLootOwner)
{
    clear();

    QueryResultAutoPtr result = CharacterDatabase.PQuery("SELECT itemId, itemCount, playerGuids FROM group_saved_loot WHERE creatureId='%u' AND instanceId='%u'", pCreature->GetEntry(), pCreature->GetInstanceId());
    if (result)
    {
        m_creatureGUID = pCreature->GetGUID();
        m_mapID = MapID(pCreature->GetMapId(), pCreature->GetInstanceId());

        std::stringstream ss;
        ss << "Loaded LootedItems: ";

        do
        {
            Field *fields = result->Fetch();

            uint32 itemid    = fields[0].GetUInt32();
            uint32 itemcount = fields[1].GetUInt32();
            for (uint8 i = 0; i < itemcount; ++i)
            {
                ss << "[" << itemid << "] ";

                LootItem item(itemid);
                items.push_back(item);
                unlootedCount++;
            }

            std::stringstream guids;
            uint8 playerscount;
            uint64 playerguid;

            guids << fields[2].GetString();
            guids >> playerscount;
            for(uint8 i = 0; i < playerscount; ++i)
            {
                guids >> playerguid;
                players_allowed_to_loot.insert(playerguid);
            }
        }
        while (result->NextRow());

        if (unlootedCount == 0)
            return;

        ss << " players in instance: ";
        Map *pMap = pCreature->GetMap();
        if (pMap && pMap->IsDungeon())
        {
            typedef std::list<GroupMemberSlot> MemberSlotList;
            typedef MemberSlotList::const_iterator member_citerator;

            Map::PlayerList const &PlayerList = pMap->GetPlayers();
            for(Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            {
                if (Player* i_pl = i->getSource())
                {
                    if (Group *pGroup = i_pl->GetGroup())
                    {
                        MemberSlotList gList = pGroup->GetMemberSlots();
                        for (member_citerator citr = gList.begin(); citr != gList.end(); ++citr)
                            ss << citr->name << " (" << citr->guid << ")  ";

                        break;
                    }
                    else
                        ss << i_pl->GetName() << " (" << i_pl->GetGUIDLow() << ")  ";
                }
            }
        }
        else
            return;

        //set variable to true even if we don't load anything so new loot won't be generated
        m_lootLoadedFromDB = true;

        sLog.outBoss(ss.str().c_str());

        // make body visible to loot
        pCreature->setDeathState(JUST_DIED);
        pCreature->SetCorpseDelay(3600);

        pCreature->LowerPlayerDamageReq(pCreature->GetMaxHealth());

        pCreature->SetVisibility(VISIBILITY_ON);

        pCreature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        pCreature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
    }
}

void Loot::removeItemFromSavedLoot(LootItem *item)
{
    if (!m_creatureGUID)
        return;

    Map *pMap = sMapMgr.FindMap(m_mapID.nMapId, m_mapID.nInstanceId);
    if (!pMap || !pMap->Instanceable())
        return;

    Creature *pCreature = pMap->GetCreatureOrPet(m_creatureGUID);
    if (!pCreature)
    {
        // log only for raids
        if (pMap->IsRaid())
            sLog.outBoss("Loot::removeItemFromSavedLoot: pCreature not found !! guid: %u, instanceid: %u) ", m_creatureGUID, pMap->GetInstanceId());
        return;
    }

    QueryResultAutoPtr result = CharacterDatabase.PQuery("SELECT itemCount FROM group_saved_loot WHERE itemId='%u' AND instanceId='%u' AND creatureId='%u'", item->itemid, pMap->GetInstanceId(), pCreature->GetEntry());
    if (!result)
    {
        if (pMap->IsRaid())
            sLog.outBoss("Loot::removeItemFromSavedLoot: result empty !! SQL: SELECT itemCount FROM group_saved_loot WHERE itemId='%u' AND instanceId='%u' AND creatureId='%u'", item->itemid, pMap->GetInstanceId(), pCreature->GetEntry());
        return;
    }

    // it should be single line
    uint32 count = 0;
    Field *fields = result->Fetch();
    count = fields[0].GetUInt32();

    static SqlStatementID updateItemCount;
    static SqlStatementID deleteItem;

    //CharacterDatabase.BeginTransaction();
    if (count > 1)
    {
        count--;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updateItemCount, "UPDATE group_saved_loot SET itemCount=? WHERE instanceId=? AND itemId=? AND creatureId=?");
        stmt.PExecute(count, pCreature->GetInstanceId(), item->itemid, pCreature->GetEntry());
    }
    else
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteItem, "DELETE FROM group_saved_loot WHERE instanceId=? AND itemId=? AND creatureId=?");
        stmt.PExecute(pCreature->GetInstanceId(), item->itemid, pCreature->GetEntry());
    }
    //CharacterDatabase.CommitTransaction();
}

void Loot::saveLootToDB(Player *owner)
{
    if (!m_creatureGUID)
        return;

    Map *pMap = sMapMgr.FindMap(m_mapID.nMapId, m_mapID.nInstanceId);
    if (!pMap || !pMap->Instanceable())
        return;

    Creature *pCreature = pMap->GetCreatureOrPet(m_creatureGUID);
    if (!pCreature)
    {
        sLog.outBoss("Loot::saveLootToDB: pCreature not found !!: player %s(%u)", owner->GetName(),owner->GetGUIDLow());
        return;
    }

    std::map<uint32, uint32> item_count;
    CharacterDatabase.BeginTransaction();

    static SqlStatementID deleteCreatureLoot;
    static SqlStatementID updateItemCount;
    static SqlStatementID insertItem;

    // delete old saved loot
    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteCreatureLoot, "DELETE FROM group_saved_loot WHERE creatureId=? AND instanceId=?");
    stmt.PExecute(pCreature->GetEntry(), pCreature->GetInstanceId());

    std::stringstream ss;
    ss << "Player's group: " << owner->GetName() << ":(" << owner->GetGUIDLow() << ") " << "LootedItems: ";

    std::stringstream guids;

    guids << players_allowed_to_loot.size();
    for (std::set<uint64>::iterator it = players_allowed_to_loot.begin(); it != players_allowed_to_loot.end(); it++)
        guids << " " << *it ;

    for (std::vector<LootItem>::iterator iter = items.begin(); iter != items.end(); ++iter)
    {
        LootItem const *item = &(*iter);

        if (item->is_looted || item->conditionId)
            continue;

        ItemPrototype const *item_proto = ObjectMgr::GetItemPrototype(item->itemid);
        if (!item_proto || item_proto->Flags & ITEM_FLAGS_PARTY_LOOT)
            continue;

        if (item_proto->Quality >= ITEM_QUALITY_RARE)
        {
            item_count[item->itemid] += 1;
            uint32 count = item_count[item->itemid];
            if (count > 1)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateItemCount, "UPDATE group_saved_loot SET itemCount=? WHERE itemId=? AND instanceId=?");
                stmt.PExecute(count, item->itemid, pCreature->GetInstanceId());
            }
            else
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertItem, "INSERT INTO group_saved_loot VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
                stmt.addUInt32(pCreature->GetEntry());
                stmt.addUInt32(pCreature->GetInstanceId());
                stmt.addUInt32(item->itemid);
                stmt.addUInt32(count);
                stmt.addBool(pCreature->IsTempSummon());
                stmt.addFloat(pCreature->GetPositionX());
                stmt.addFloat(pCreature->GetPositionY());
                stmt.addFloat(pCreature->GetPositionZ());
                stmt.addString(guids.str());
                stmt.Execute();
            }
            ss << "[" << item->itemid << "] ";
        }
    }

    sLog.outBoss(ss.str().c_str());
    CharacterDatabase.CommitTransaction();
}

//===================================================

void Loot::NotifyItemRemoved(uint8 lootIndex)
{
    // notify all players that are looting this that the item was removed
    // convert the index to the slot the player sees
    std::set<uint64>::iterator i_next;
    for (std::set<uint64>::iterator i = PlayersLooting.begin(); i != PlayersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = ObjectAccessor::FindPlayer(*i))
            pl->SendNotifyLootItemRemoved(lootIndex);
        else
            PlayersLooting.erase(i);
    }
}

void Loot::NotifyMoneyRemoved()
{
    // notify all players that are looting this that the money was removed
    std::set<uint64>::iterator i_next;
    for (std::set<uint64>::iterator i = PlayersLooting.begin(); i != PlayersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = ObjectAccessor::FindPlayer(*i))
            pl->SendNotifyLootMoneyRemoved();
        else
            PlayersLooting.erase(i);
    }
}

void Loot::NotifyQuestItemRemoved(uint8 questIndex)
{
    // when a free for all questitem is looted
    // all players will get notified of it being removed
    // (other questitems can be looted by each group member)
    // bit inefficient but isn't called often

    std::set<uint64>::iterator i_next;
    for (std::set<uint64>::iterator i = PlayersLooting.begin(); i != PlayersLooting.end(); i = i_next)
    {
        i_next = i;
        ++i_next;
        if (Player* pl = ObjectAccessor::FindPlayer(*i))
        {
            QuestItemMap::iterator pq = PlayerQuestItems.find(pl->GetGUIDLow());
            if (pq != PlayerQuestItems.end() && pq->second)
            {
                // find where/if the player has the given item in it's vector
                QuestItemList& pql = *pq->second;

                uint8 j;
                for (j = 0; j < pql.size(); ++j)
                    if (pql[j].index == questIndex)
                        break;

                if (j < pql.size())
                    pl->SendNotifyLootItemRemoved(items.size()+j);
            }
        }
        else
            PlayersLooting.erase(i);
    }
}

void Loot::generateMoneyLoot(uint32 minAmount, uint32 maxAmount)
{
    if (maxAmount > 0)
    {
        if (maxAmount <= minAmount)
            gold = uint32(maxAmount * sWorld.getRate(RATE_DROP_MONEY));
        else if ((maxAmount - minAmount) < 32700)
            gold = uint32(urand(minAmount, maxAmount) * sWorld.getRate(RATE_DROP_MONEY));
        else
            gold = uint32(urand(minAmount >> 8, maxAmount >> 8) * sWorld.getRate(RATE_DROP_MONEY)) << 8;
    }
}

void Loot::setItemLooted(LootItem *pLootItem)
{
    pLootItem->is_looted = true;
    removeItemFromSavedLoot(pLootItem);
}

LootItem* Loot::LootItemInSlot(uint32 lootSlot, Player* player, QuestItem **qitem, QuestItem **ffaitem, QuestItem **conditem)
{
    LootItem* item = NULL;
    bool is_looted = true;
    if (lootSlot >= items.size())
    {
        uint32 questSlot = lootSlot - items.size();
        QuestItemMap::const_iterator itr = PlayerQuestItems.find(player->GetGUIDLow());
        if (itr != PlayerQuestItems.end() && questSlot < itr->second->size())
        {
            QuestItem *qitem2 = &itr->second->at(questSlot);
            if (qitem)
                *qitem = qitem2;
            item = &quest_items[qitem2->index];
            is_looted = qitem2->is_looted;
        }
    }
    else
    {
        item = &items[lootSlot];
        is_looted = item->is_looted;
        if (item->freeforall)
        {
            QuestItemMap::const_iterator itr = PlayerFFAItems.find(player->GetGUIDLow());
            if (itr != PlayerFFAItems.end())
            {
                for (QuestItemList::iterator iter=itr->second->begin(); iter!= itr->second->end(); ++iter)
                    if (iter->index==lootSlot)
                    {
                        QuestItem *ffaitem2 = (QuestItem*)&(*iter);
                        if (ffaitem)
                            *ffaitem = ffaitem2;
                        is_looted = ffaitem2->is_looted;
                        break;
                    }
            }
        }
        else if (item->conditionId)
        {
            QuestItemMap::const_iterator itr = PlayerNonQuestNonFFAConditionalItems.find(player->GetGUIDLow());
            if (itr != PlayerNonQuestNonFFAConditionalItems.end())
            {
                for (QuestItemList::iterator iter=itr->second->begin(); iter!= itr->second->end(); ++iter)
                {
                    if (iter->index==lootSlot)
                    {
                        QuestItem *conditem2 = (QuestItem*)&(*iter);
                        if (conditem)
                            *conditem = conditem2;
                        is_looted = conditem2->is_looted;
                        break;
                    }
                }
            }
        }
    }

    if (is_looted)
        return NULL;

    return item;
}

LootItem* Loot::LootItemInSlot(uint32 lootSlot)
{
    if (lootSlot >= items.size())
        return NULL;
    return &items[lootSlot];
}

uint32 Loot::GetMaxSlotInLootFor(Player* player) const
{
    QuestItemMap::const_iterator itr = PlayerQuestItems.find(player->GetGUIDLow());
    return items.size() + (itr != PlayerQuestItems.end() ?  itr->second->size() : 0);
}

// should be fixed after some research in loot implementation etc
void Loot::RemoveQuestLoot(Player* player)
{
    return;
    QuestItemMap::iterator itr = PlayerQuestItems.find(player->GetGUIDLow());
    if (itr == PlayerQuestItems.end())
        return;

    QuestItemList * tmpList = itr->second;

    uint32 count = 0;

    for (QuestItemList::iterator qItr = tmpList->begin(); qItr != tmpList->end(); ++qItr)
    {
        LootItem &item = quest_items[(*qItr).index];
        if (item.freeforall && !item.is_looted)
            ++count;
    }

    if (unlootedCount >= count)
        unlootedCount -= count;
    else
        unlootedCount = 0;

    delete tmpList;
    tmpList = NULL;
    itr->second = NULL;

    PlayerQuestItems.erase(itr);
}

ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li)
{
    b << uint32(li.itemid);
    b << uint32(li.count);                                  // nr of items of this type
    b << uint32(ObjectMgr::GetItemPrototype(li.itemid)->DisplayInfoID);
    b << uint32(li.randomSuffix);
    b << uint32(li.randomPropertyId);
    //b << uint8(0);                                        // slot type - will send after this function call
    return b;
}

ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv)
{
    if (lv.permission == NONE_PERMISSION)
    {
        b << uint32(0);                                     //gold
        b << uint8(0);                                      // item count
        return b;                                           // nothing output more
    }

    Loot &l = lv.loot;

    uint8 itemsShown = 0;

    //gold
    b << uint32(l.gold);

    size_t count_pos = b.wpos();                            // pos of item count byte
    b << uint8(0);                                          // item count placeholder

    uint8 slot_type = 0; // 0 - get, 1 - look only, 2 - master selection
    for (uint8 i = 0; i < l.items.size(); ++i)
    {
        if (!l.items[i].is_looted && !l.items[i].freeforall && !l.items[i].conditionId && l.items[i].AllowedForPlayer(lv.viewer))
        {
            if (lv.permission == ALL_PERMISSION) 
                slot_type = 0;
            else if (lv.permission == MASTER_PERMISSION)
                slot_type = 2;
            else if (lv.permission == GROUP_LOOTER_PERMISSION)
                slot_type = l.items[i].is_blocked ? 1 : 0;
            else if (lv.permission == GROUP_NONE_PERMISSION)
                slot_type = 1;
            b << uint8(i) << l.items[i];            //only send one-player loot items now, free for all will be sent later
            b << uint8(slot_type);                  
            ++itemsShown;
        }
    }

    QuestItemMap const& lootPlayerQuestItems = l.GetPlayerQuestItems();
    QuestItemMap::const_iterator q_itr = lootPlayerQuestItems.find(lv.viewer->GetGUIDLow());
    if (q_itr != lootPlayerQuestItems.end())
    {
        QuestItemList *q_list = q_itr->second;
        for (QuestItemList::iterator qi = q_list->begin() ; qi != q_list->end(); ++qi)
        {
            LootItem &item = l.quest_items[qi->index];
            if (!qi->is_looted && !item.is_looted)
            {
                b << uint8(l.items.size() + (qi - q_list->begin()));
                b << item;
                b << uint8(0);                              // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerFFAItems = l.GetPlayerFFAItems();
    QuestItemMap::const_iterator ffa_itr = lootPlayerFFAItems.find(lv.viewer->GetGUIDLow());
    if (ffa_itr != lootPlayerFFAItems.end())
    {
        QuestItemList *ffa_list = ffa_itr->second;
        for (QuestItemList::iterator fi = ffa_list->begin() ; fi != ffa_list->end(); ++fi)
        {
            LootItem &item = l.items[fi->index];
            if (!fi->is_looted && !item.is_looted)
            {
                b << uint8(fi->index) << item;
                b << uint8(0);                              // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerNonQuestNonFFAConditionalItems = l.GetPlayerNonQuestNonFFAConditionalItems();
    QuestItemMap::const_iterator nn_itr = lootPlayerNonQuestNonFFAConditionalItems.find(lv.viewer->GetGUIDLow());
    if (nn_itr != lootPlayerNonQuestNonFFAConditionalItems.end())
    {
        QuestItemList *conditional_list =  nn_itr->second;
        for (QuestItemList::iterator ci = conditional_list->begin() ; ci != conditional_list->end(); ++ci)
        {
            LootItem &item = l.items[ci->index];
            if (!ci->is_looted && !item.is_looted)
            {
                b << uint8(ci->index) << item;
                b << uint8(0);                              // allow loot
                ++itemsShown;
            }
        }
    }

    //update number of items shown
    b.put<uint8>(count_pos,itemsShown);

    return b;
}

//
// --------- LootGroupTemplate ---------
//

// Adds an entry to the group (at loading stage)
void LootGroupTemplate::AddEntry(LootStoreItem& item)
{
    if (item.chance != 0)
        ExplicitlyChanced.push_back(item);
    else
        EqualChanced.push_back(item);
}

// Rolls an item from the group, returns NULL if all miss their chances
LootStoreItem const * LootGroupTemplate::Roll(std::set<uint32> &except) const
{
    if (!ExplicitlyChanced.empty())                         // First explicitly chanced entries are checked
    {
        float Roll = rand_chance();

        std::vector<uint32> allowed;
        float notAllowedChance = 0;
        int counter = 0;
        for (LootStoreItemList::const_iterator i = ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        {
            if (except.find(i->itemid) != except.end())
                notAllowedChance += i->chance;
            else
                allowed.push_back(counter);
            counter++;
        }
        if (notAllowedChance >= 100.0f)
            return NULL;

        Roll = (100.0f - notAllowedChance)*Roll/100.0f;

        for (std::vector<uint32>::const_iterator i = allowed.begin(); i != allowed.end(); ++i)    //check each explicitly chanced entry in the template and modify its chance based on quality.
        {
            const LootStoreItem *item = &ExplicitlyChanced[*i];
            //if(item->chance >= 100.f)
            //    return item;

            //ItemPrototype const *pProto = ObjectMgr::GetItemPrototype(ExplicitlyChanced[*i].itemid);
            Roll -= item->chance;
            if (Roll < 0)
                return item;
        }
    }
    if (!EqualChanced.empty())                              // If nothing selected yet - an item is taken from equal-chanced part
    {
        std::vector<uint32> allowed;

        int counter = 0;
        for (LootStoreItemList::const_iterator i = EqualChanced.begin(); i != EqualChanced.end(); ++i)
        {
            if (except.find(i->itemid) == except.end())
                allowed.push_back(counter);
            counter++;
        }

        if (!allowed.empty())
            return &EqualChanced[allowed[irand(0, allowed.size()-1)]];
    }

    return NULL;                                            // Empty drop from the group
}

// True if group includes at least 1 quest drop entry
bool LootGroupTemplate::HasQuestDrop() const
{
    for (LootStoreItemList::const_iterator i=ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (i->isQuest)
            return true;
    for (LootStoreItemList::const_iterator i=EqualChanced.begin(); i != EqualChanced.end(); ++i)
        if (i->isQuest)
            return true;
    return false;
}

// True if group includes at least 1 quest drop entry for active quests of the player
bool LootGroupTemplate::HasQuestDropForPlayer(Player const * player) const
{
    for (LootStoreItemList::const_iterator i=ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (player->HasQuestForItem(i->itemid))
            return true;
    for (LootStoreItemList::const_iterator i=EqualChanced.begin(); i != EqualChanced.end(); ++i)
        if (player->HasQuestForItem(i->itemid))
            return true;
    return false;
}

// Rolls an item from the group (if any takes its chance) and adds the item to the loot
void LootGroupTemplate::Process(Loot& loot) const
{
    LootStoreItem const * item = Roll(loot.unique_items);
    if (item != NULL)
        loot.AddItem(*item);
}

// Overall chance for the group without equal chanced items
float LootGroupTemplate::RawTotalChance() const
{
    float result = 0;

    for (LootStoreItemList::const_iterator i=ExplicitlyChanced.begin(); i != ExplicitlyChanced.end(); ++i)
        if (!i->isQuest)
            result += i->chance;

    return result;
}

// Overall chance for the group
float LootGroupTemplate::TotalChance() const
{
    float result = RawTotalChance();

    if (!EqualChanced.empty() && result < 100.0f)
        return 100.0f;

    return result;
}

void LootGroupTemplate::Verify(uint32 group_id) const
{
    float chance = RawTotalChance();
    if (chance > 101.0f)                                    // TODO: replace with 100% when DBs will be ready
    {
        sLog.outErrorDb("Group %d has total chance > 100%% (%f)", group_id, chance);
    }

    if (chance >= 100.0f && !EqualChanced.empty())
    {
        sLog.outErrorDb("Group %d has items with chance=0%% but group total chance >= 100%% (%f)", group_id, chance);
    }
}

//
// --------- LootTemplate ---------
//

// Adds an entry to the group (at loading stage)
void LootTemplate::AddEntry(LootStoreGroup& item)
{
    Groups.push_back(item);
}

// Rolls for every item in the template and adds the rolled items the the loot
void LootTemplate::Process(Loot& loot) const
{
    for (LootGroups::const_iterator it = Groups.begin(); it != Groups.end(); ++it)
    {
        if (!it->Roll())
            continue;                                       // Bad luck for the group

        uint32 count = urand(it->minCount, it->maxCount);
        for (uint32 i = 0; i < count; ++i)
            it->group->Process(loot);
    }
}

// True if template includes at least 1 quest drop entry
bool LootTemplate::HasQuestDrop() const
{
    for (LootGroups::const_iterator i = Groups.begin() ; i != Groups.end() ; ++i)
        if (i->group->HasQuestDrop())
            return true;

    return false;
}

// True if template includes at least 1 quest drop for an active quest of the player
bool LootTemplate::HasQuestDropForPlayer(Player const* player) const
{
    for (LootGroups::const_iterator i = Groups.begin(); i != Groups.end(); ++i)
        if (i->group->HasQuestDropForPlayer(player))
            return true;

    return false;
}

void LoadLootTemplates()
{
    sLootStore.LoadLootTable();
}

void LoadLootTypeTemplates()
{
    sLootStore.LoadLootTypeTable();
}

void LoadLootGroupTemplates()
{
    sLootStore.LoadLootGroupTable();
}
