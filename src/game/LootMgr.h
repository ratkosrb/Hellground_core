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

#ifndef HELLGROUND_LOOTMGR_H
#define HELLGROUND_LOOTMGR_H

#include "ItemEnchantmentMgr.h"
#include "ByteBuffer.h"
#include "Utilities/LinkedReference/RefManager.h"

#include "MapManager.h"

#include <ace/Singleton.h>
#include <ace/Null_Mutex.h>

#include <map>
#include <vector>

class Creature;

enum RollType
{
    ROLL_PASS         = 0,
    ROLL_NEED         = 1,
    ROLL_GREED        = 2
};

#define MAX_NR_LOOT_ITEMS 16
// note: the client cannot show more than 16 items total
#define MAX_NR_QUEST_ITEMS 32
// unrelated to the number of quest items shown, just for reserve

enum LootMethod
{
    FREE_FOR_ALL      = 0,
    ROUND_ROBIN       = 1,
    MASTER_LOOT       = 2,
    GROUP_LOOT        = 3,
    NEED_BEFORE_GREED = 4
};

enum PermissionTypes
{
    ALL_PERMISSION           = 0,
    GROUP_LOOTER_PERMISSION  = 1,
    GROUP_NONE_PERMISSION    = 2,
    MASTER_PERMISSION        = 3,
    NONE_PERMISSION          = 4
};

enum LootableType
{
    CREATURE = 0,
    CREATURE_SKINNING = 1
};

class WorldObject;
class Player;
class LootStore;

struct Loot;
class LootTemplate;
class LootGroupTemplate;
class LootConditionTemplate;

struct LootStoreItem
{
    uint32  itemid;                                         // id of the item
    float   chance;
    uint8   minCount    :8;
    uint8   maxCount    :8;                                 // max drop count for the item (mincountOrRef positive) or Ref multiplicator (mincountOrRef negative)
    uint16  conditionId :16;                                // additional loot condition Id
    bool    isQuest     :1;

    // displayid is filled in IsValid() which must be called after
    LootStoreItem(uint32 _itemid, bool _isQuest, float _chance, uint8 _minCount, uint8 _maxCount, uint16 _conditionId)
        : itemid(_itemid), chance(_chance), minCount(_minCount), maxCount(_maxCount), conditionId(_conditionId) {}

    bool Roll() const;                                      // Checks if the entry takes it's chance (at loot generation)
};

struct LootStoreGroup
{
    LootGroupTemplate* group;                                        
    float              chance;
    uint8              minCount    :8;
    uint8              maxCount    :8;        

    LootStoreGroup(LootGroupTemplate *_group, float _chance, uint8 _minCount, uint8 _maxCount)
        : group(_group), chance(_chance), minCount(_minCount), maxCount(_maxCount) {}

    bool Roll() const;
};

struct LootItem
{
    uint32  itemid;
    uint32  randomSuffix;
    int32   randomPropertyId;
    uint16  conditionId       :16;                          // allow compiler pack structure
    uint8   count             : 8;
    bool    is_looted         : 1;
    bool    is_blocked        : 1;
    bool    freeforall        : 1;                          // free for all
    bool    is_counted        : 1;
    bool    needs_quest       : 1;                          // quest drop

    // Constructor, copies most fields from LootStoreItem, generates random count and random suffixes/properties
    // Should be called for non-reference LootStoreItem entries only (mincountOrRef > 0)
    explicit LootItem(LootStoreItem const& li);

    explicit LootItem(uint32 id);

    // Basic checks for player/item compatibility - if false no chance to see the item in the loot
    bool AllowedForPlayer(Player const * player) const;
};

struct QuestItem
{
    uint8   index;                                          // position in quest_items;
    bool    is_looted;

    QuestItem()
        : index(0), is_looted(false) {}

    QuestItem(uint8 _index, bool _islooted = false)
        : index(_index), is_looted(_islooted) {}
};

typedef std::vector<QuestItem> QuestItemList;
typedef std::map<uint32, QuestItemList *> QuestItemMap;
typedef std::vector<LootStoreItem> LootStoreItemList;

typedef UNORDERED_MAP<uint32, LootTemplate*> LootTemplateMap;
typedef UNORDERED_MAP<uint32, LootGroupTemplate*> LootGroupMap;
typedef std::pair<LootableType, uint32> LootableObject;
typedef UNORDERED_MAP<LootableObject, LootTemplate*> LootableObjectMap;

typedef std::set<LootableObject> LootIdSet;

class LootStore
{
    public:
        explicit LootStore() {}
        virtual ~LootStore() { Clear(); }

        void Verify() const;

        void LoadAndCollectLootIds(LootIdSet& ids_set);

        bool HaveLootfor (uint32 loot_id) const { return m_LootTemplates.find(loot_id) != m_LootTemplates.end(); }
        bool HaveQuestLootfor (uint32 loot_id) const;
        bool HaveQuestLootForPlayer(uint32 loot_id,Player* player) const;

        LootTemplate const* GetLootfor (uint32 loot_id) const;
    protected:
        void LoadLootTable();
        void LoadLootTypeTable();
        void LoadLootGroupTable();

        void Clear();
    private:
        LootableObjectMap m_LootableObjects;
        LootTemplateMap m_LootTemplates;
        LootGroupMap m_Groups;
};

class LootGroupTemplate                               // A set of loot definitions for items (refs are not allowed)
{
    public:
        void AddEntry(LootStoreItem& item);                 // Adds an entry to the group (at loading stage)
        bool HasQuestDrop() const;                          // True if group includes at least 1 quest drop entry
        bool HasQuestDropForPlayer(Player const * player) const;
                                                            // The same for active quests of the player
        void Process(Loot& loot) const;                     // Rolls an item from the group (if any) and adds the item to the loot
        float RawTotalChance() const;                       // Overall chance for the group (without equal chanced items)
        float TotalChance() const;                          // Overall chance for the group

        void Verify(LootStore const& lootstore, uint32 id, uint32 group_id) const;
        void CollectLootIds(LootIdSet& set) const;
        void CheckLootRefs(LootTemplateMap const& store, LootIdSet* ref_set) const;
    private:
        LootStoreItemList ExplicitlyChanced;                // Entries with chances defined in DB
        LootStoreItemList EqualChanced;                     // Zero chances - every entry takes the same chance

        LootStoreItem const * Roll(std::set<uint32> &except) const;                 // Rolls an item from the group, returns NULL if all miss their chances
};

typedef std::vector<LootStoreGroup> LootGroups;

class LootTemplate
{
    public:
        // Adds an entry to the group (at loading stage)
        void AddEntry(LootStoreGroup &group);

        // Rolls for every item in the template and adds the rolled items the the loot
        void Process(Loot& loot) const;

        // True if template includes at least 1 quest drop entry
        bool HasQuestDrop() const;
        // True if template includes at least 1 quest drop for an active quest of the player
        bool HasQuestDropForPlayer(Player const * player) const;

        // Checks integrity of the template
        void Verify(LootStore const& store, uint32 Id) const;
        void CheckLootRefs(LootTemplateMap const& store, LootIdSet* ref_set) const;
    private:
        LootGroups        Groups;                           // groups have own (optimised) processing, grouped entries go there
        bool m_hasQuestDrop;

};



//=====================================================

class LootValidatorRef :  public Reference<Loot, LootValidatorRef>
{
    public:
        LootValidatorRef() {}
        void targetObjectDestroyLink() {}
        void sourceObjectDestroyLink() {}
};

//=====================================================

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
    public:
        typedef LinkedListHead::Iterator< LootValidatorRef > iterator;

        LootValidatorRef* getFirst() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst(); }
        LootValidatorRef* getLast() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast(); }

        iterator begin() { return iterator(getFirst()); }
        iterator end() { return iterator(NULL); }
        iterator rbegin() { return iterator(getLast()); }
        iterator rend() { return iterator(NULL); }
};

struct LootView;

ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li);
ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

//=====================================================
struct Loot
{
    friend ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

    QuestItemMap const& GetPlayerQuestItems() const { return PlayerQuestItems; }
    QuestItemMap const& GetPlayerFFAItems() const { return PlayerFFAItems; }
    QuestItemMap const& GetPlayerNonQuestNonFFAConditionalItems() const { return PlayerNonQuestNonFFAConditionalItems; }

    std::vector<LootItem> items;
    std::vector<LootItem> quest_items;
    std::set<uint32> unique_items;
    std::set<uint64> players_allowed_to_loot;

    // required by round robin to see who can open loot
    ItemQualities max_quality;
    bool everyone_can_open;

    uint32 gold;
    uint8 unlootedCount;
    uint64 looterGUID;
    uint64 looterTimer;
    uint64 looterCheckTimer;

    Loot(uint32 _gold = 0) : gold(_gold), unlootedCount(0), m_lootLoadedFromDB(false), m_creatureGUID(0), m_mapID(0,0), looterGUID(0), max_quality(ITEM_QUALITY_POOR), everyone_can_open(false),
                            looterTimer(0), looterCheckTimer(0) {}
    ~Loot() { clear(); }

    // if loot becomes invalid this reference is used to inform the listener
    void addLootValidatorRef(LootValidatorRef* pLootValidatorRef)
    {
        i_LootValidatorRefManager.insertFirst(pLootValidatorRef);
    }

    void clear()
    {
        for (QuestItemMap::iterator itr = PlayerQuestItems.begin(); itr != PlayerQuestItems.end(); ++itr)
            delete itr->second;
        PlayerQuestItems.clear();

        for (QuestItemMap::iterator itr = PlayerFFAItems.begin(); itr != PlayerFFAItems.end(); ++itr)
            delete itr->second;
        PlayerFFAItems.clear();

        for (QuestItemMap::iterator itr = PlayerNonQuestNonFFAConditionalItems.begin(); itr != PlayerNonQuestNonFFAConditionalItems.end(); ++itr)
            delete itr->second;
        PlayerNonQuestNonFFAConditionalItems.clear();

        PlayersLooting.clear();
        items.clear();
        quest_items.clear();
        unique_items.clear();
        gold = 0;
        unlootedCount = 0;
        looterGUID = 0;
        i_LootValidatorRefManager.clearReferences();
    }

    bool empty() const { return items.empty() && gold == 0; }
    bool isLooted() const { return gold == 0 && unlootedCount == 0; }

    void NotifyItemRemoved(uint8 lootIndex);
    void NotifyQuestItemRemoved(uint8 questIndex);
    void NotifyMoneyRemoved();
    void AddLooter(uint64 GUID) { PlayersLooting.insert(GUID); }
    void RemoveLooter(uint64 GUID) { PlayersLooting.erase(GUID); }

    void generateMoneyLoot(uint32 minAmount, uint32 maxAmount);
    void FillLoot(LootableObject lootableObject, Player* loot_owner, bool personal);

    void saveLootToDB(Player *owner);

    bool IsPlayerAllowedToLoot(Player *player, WorldObject *object);

    // Inserts the item into the loot (called by LootTemplate processors)
    void AddItem(LootStoreItem const & item);

    void setItemLooted(LootItem *pLootItem);
    void removeItemFromSavedLoot(LootItem *pLootItem);

    void setCreatureGUID(Creature *pCreature);

    void FillLootFromDB(Creature *pCreature, Player* pLootOwner);
    bool LootLoadedFromDB() { return m_lootLoadedFromDB; }

    LootItem* LootItemInSlot(uint32 lootslot, Player* player, QuestItem** qitem = NULL, QuestItem** ffaitem = NULL, QuestItem** conditem = NULL);

    LootItem* LootItemInSlot(uint32 lootslot);

    uint32 GetMaxSlotInLootFor(Player* player) const;

    void RemoveQuestLoot(Player* player);

    private:
        void FillNotNormalLootFor(Player* player);
        QuestItemList* FillFFALoot(Player* player);
        QuestItemList* FillQuestLoot(Player* player);
        QuestItemList* FillNonQuestNonFFAConditionalLoot(Player* player);

        std::set<uint64> PlayersLooting;
        QuestItemMap PlayerQuestItems;
        QuestItemMap PlayerFFAItems;
        QuestItemMap PlayerNonQuestNonFFAConditionalItems;

        uint64 m_creatureGUID;
        bool m_lootLoadedFromDB;

        MapID m_mapID;

        // All rolls are registered here. They need to know, when the loot is not valid anymore
        LootValidatorRefManager i_LootValidatorRefManager;
};

struct LootView
{
    Loot &loot;
    Player *viewer;
    PermissionTypes permission;
    LootView(Loot &_loot, Player *_viewer,PermissionTypes _permission = ALL_PERMISSION)
        : loot(_loot), viewer(_viewer), permission(_permission) {}
};

#define sLootStore (*ACE_Singleton<LootStore, ACE_Null_Mutex>::instance())

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;
extern LootStore LootTemplates_Prospecting;
extern LootStore LootTemplates_QuestMail;

void LoadLootTemplates_Creature();
void LoadLootTemplates_Fishing();
void LoadLootTemplates_Gameobject();
void LoadLootTemplates_Item();
void LoadLootTemplates_Pickpocketing();
void LoadLootTemplates_Skinning();
void LoadLootTemplates_Disenchant();
void LoadLootTemplates_Prospecting();
void LoadLootTemplates_QuestMail();
void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();
    LoadLootTemplates_Prospecting();
    LoadLootTemplates_QuestMail();
    LoadLootTemplates_Reference();
}

#endif

