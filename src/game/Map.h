/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008-2009 TrinityCore <http://www.trinitycore.org/>
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

#ifndef HELLGROUND_MAP_H
#define HELLGROUND_MAP_H

#include "Platform/Define.h"
#include "ace/RW_Thread_Mutex.h"
#include "ace/Thread_Mutex.h"

#include "DBCStructure.h"
#include "GridDefines.h"
#include "Cell.h"
#include "Timer.h"
#include "SharedDefines.h"
#include "GridMap.h"
#include "GameSystem/GridRefManager.h"
#include "MapRefManager.h"
#include "vmap/DynamicTree.h"
#include "G3D/Vector3.h"
#include "mersennetwister/MersenneTwister.h"

#include <bitset>
#include <list>

namespace VMAP
{
    class ModelInstance;
};

class Unit;
class Creature;
class WorldPacket;
class InstanceData;
class Group;
class InstanceSave;
class Object;
class Player;
class WorldObject;
class CreatureGroup;
class BattleGround;
class Transport;

class GridMap;
class TerrainInfo;

struct ScriptInfo;
struct ScriptAction;

struct CreatureMover
{
    CreatureMover() : x(0), y(0), z(0), ang(0) {}
    CreatureMover(float _x, float _y, float _z, float _ang) : x(_x), y(_y), z(_z), ang(_ang) {}

    float x, y, z, ang;
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct InstanceTemplate
{
    uint32 map;
    uint32 parent;
    uint32 maxPlayers;
    uint32 reset_delay;
    uint32 access_id;
    float startLocX;
    float startLocY;
    float startLocZ;
    float startLocO;
    uint32 script_id;
};

enum LevelRequirementVsMode
{
    LEVELREQUIREMENT_HEROIC = 70
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

#define MAX_HEIGHT            100000.0f                     // can be use for find ground height at surface
#define INVALID_HEIGHT       -100000.0f                     // for check, must be equal to VMAP_INVALID_HEIGHT, real value for unknown height is VMAP_INVALID_HEIGHT_VALUE
#define MIN_UNLOAD_DELAY      1                             // immediate unload

typedef UNORDERED_MAP<Creature*, CreatureMover>                 CreatureMoveList;
typedef std::map<uint32/*leaderDBGUID*/, CreatureGroup*>        CreatureGroupHolderType;
typedef std::unordered_map<uint64, GameObject*>           GObjectMapType;
typedef std::unordered_map<uint64, DynamicObject*>        DObjectMapType;
typedef std::unordered_map<uint64, Creature*>             CreaturesMapType;
typedef std::unordered_map<uint32, std::list<uint64> >    CreatureIdToGuidListMapType;

enum GetCreatureGuidType
{
    GET_FIRST_CREATURE_GUID     = 0,
    GET_LAST_CREATURE_GUID      = 1,
    GET_RANDOM_CREATURE_GUID    = 2,
    GET_ALIVE_CREATURE_GUID     = 3
};

typedef std::list<std::pair<Map*, uint32> > DelayedMapList;

class HELLGROUND_IMPORT_EXPORT Map : public GridRefManager<NGridType>
{
    friend class MapReference;
    public:
        class UpdateHelper
        {
            public:
                explicit UpdateHelper(Map* m) : m_map(m) {}
                ~UpdateHelper() {}

                void Update(DelayedMapList& delayedUpdate);

                time_t GetTimeElapsed() const;

            private:
                UpdateHelper& operator=(const UpdateHelper&);
                UpdateHelper(const UpdateHelper& o);

                Map* m_map;
        };

        Map(uint32 id, time_t, uint32 InstanceId, uint8 SpawnMode);
        virtual ~Map();

        // currently unused for normal maps
        bool CanUnload(uint32 diff);

        virtual bool Add(Player *);
        virtual void Remove(Player *, bool);

        template<class T> void Add(T *);
        template<class T> void Remove(T *, bool);

        void InsertIntoCreatureGUIDList(Creature* obj);
        void RemoveFromCreatureGUIDList(Creature* obj);

        void InsertIntoObjMap(Object * obj);
        void RemoveFromObjMap(uint64 guid);
        void RemoveFromObjMap(Object * obj);

        virtual void Update(const uint32&);
        virtual void DelayedUpdate(const uint32);

        void BroadcastPacket(WorldObject*, WorldPacket*, bool = false);
        void BroadcastPacketInRange(WorldObject*, WorldPacket*, float, bool = false, bool = false);
        void BroadcastPacketExcept(WorldObject*, WorldPacket*, Player*);

        virtual void InitVisibilityDistance();

        float GetVisibilityDistance(WorldObject* = NULL, Player* = NULL) const;
        float GetActiveObjectUpdateDistance() const { return m_ActiveObjectUpdateDistance; }

        void PlayerRelocation(Player*, float, float, float, float);
        void CreatureRelocation(Creature*, float, float, float, float);

        template<class T, class CONTAINER>
        void Visit(const Cell &cell, TypeContainerVisitor<T, CONTAINER> &visitor);

        bool IsRemovalGrid(float x, float y) const;
        bool IsLoaded(float x, float y) const;
        bool UnloadGrid(const uint32 &x, const uint32 &y, bool pForce);

        virtual void UnloadAll();

        void ResetGridExpiry(NGridType &grid, float factor = 1) const;

        time_t GetGridExpiry(void) const { return i_gridExpiry; }
        uint32 GetId(void) const { return i_id; }

        static void InitStateMachine();
        static void DeleteStateMachine();

        void MoveAllCreaturesInMoveList();
        void RemoveAllObjectsInRemoveList();

        bool CreatureRespawnRelocation(Creature *c);        // used only in MoveAllCreaturesInMoveList and ObjectGridUnloader

        // assert print helper
        bool CheckGridIntegrity(Creature* c, bool moved) const;

        uint32 GetInstanceId() const { return i_InstanceId; }
        uint8 GetSpawnMode() const { return (i_spawnMode); }

        virtual bool CanEnter(Player* /*player*/) { return true; }
        virtual bool EncounterInProgress(Player*) { return false; }

        const char* GetMapName() const;

        MapEntry const* GetMapEntry() const { return i_mapEntry; }
        bool Instanceable() const { return i_mapEntry && i_mapEntry->Instanceable(); }
        // NOTE: this duplicate of Instanceable(), but Instanceable() can be changed when BG also will be instanceable
        bool IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
        bool IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
        bool IsHeroic() const { return i_spawnMode == DIFFICULTY_HEROIC; }
        bool IsBattleGround() const { return i_mapEntry && i_mapEntry->IsBattleGround(); }
        bool IsBattleArena() const { return i_mapEntry && i_mapEntry->IsBattleArena(); }
        bool IsBattleGroundOrArena() const { return i_mapEntry && i_mapEntry->IsBattleGroundOrArena(); }

        bool GetEntrancePos(int32 &mapid, float &x, float &y);

        void AddObjectToRemoveList(WorldObject *obj);
        void AddObjectToSwitchList(WorldObject *obj, bool on);

        void resetMarkedCells() { marked_cells.reset(); }
        bool isCellMarked(uint32 pCellId) { return marked_cells.test(pCellId); }
        void markCell(uint32 pCellId) { marked_cells.set(pCellId); }

        bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }
        uint32 GetPlayersCountExceptGMs() const;
        uint32 GetAlivePlayersCountExceptGMs() const;

        bool ActiveObjectsNearGrid(uint32 x, uint32 y) const;

        typedef MapRefManager PlayerList;
        PlayerList const& GetPlayers() const { return m_mapRefManager; }

        // must called with AddToWorld
        void AddToActive(WorldObject* obj);
        // must called with RemoveFromWorld
        void RemoveFromActive(WorldObject* obj);

        template<class T>
        void SwitchGridContainers(T* obj, bool active);

        CreatureGroupHolderType CreatureGroupHolder;

        Creature* GetCreature(uint64 guid);
        Creature* GetCreature(uint64 guid, float x, float y);
        Creature* GetCreatureById(uint32 id, GetCreatureGuidType type = GET_FIRST_CREATURE_GUID);
        Creature* GetCreatureOrPet(uint64 guid);
        GameObject* GetGameObject(uint64 guid);
        DynamicObject* GetDynamicObject(uint64 guid);
        Unit* GetUnit(uint64 guid);

        Object* GetObjectByTypeMask(Player const &p, uint64 guid, uint32 typemask);
        void VisibilityOfCreatureEntry(uint32 entry, bool hide);

        std::list<uint64> GetCreaturesGUIDList(uint32 id, GetCreatureGuidType type = GET_FIRST_CREATURE_GUID, uint32 max = 0);
        uint64 GetCreatureGUID(uint32 id, GetCreatureGuidType type = GET_FIRST_CREATURE_GUID);

        void AddUpdateObject(Object *obj)
        {
            i_objectsToClientUpdate.insert(obj);
        }

        void RemoveUpdateObject(Object *obj)
        {
            i_objectsToClientUpdate.erase(obj);
        }

        // map restarting system
        bool const IsBroken() { return m_broken; };
        void SetBroken( bool _value = true ) { m_broken = _value; };
        void ForcedUnload();

        // Dynamic VMaps
        float GetHeight(float x, float y, float z, bool vmap = true, float maxSearchDist = 10.0f) const;
        bool GetHeightInRange(float x, float y, float& z, float maxSearchDist = 4.0f) const;
        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2, bool checkDynLos = true, bool ignoreM2Model = true) const;
        bool GetLosHitPosition(float srcX, float srcY, float srcZ, float& destX, float& destY, float& destZ, float modifyDist) const;
        // Use navemesh to walk
        bool GetWalkHitPosition(Transport* t, float srcX, float srcY, float srcZ, float& destX, float& destY, float& destZ,
            uint32 moveAllowedFlags = 0xF /*NAV_GROUND | NAV_WATER | NAV_MAGMA | NAV_SLIME*/, float zSearchDist = 20.0f, bool locatedOnSteepSlope = true) const;
        VMAP::ModelInstance* FindCollisionModel(float x1, float y1, float z1, float x2, float y2, float z2);

        void Balance() { _dynamicTree.balance(); }
        void RemoveGameObjectModel(GameObjectModel const& model)
        {
            _dynamicTree_lock.acquire_write();
            _dynamicTree.remove(model);
            _dynamicTree.balance();
            _dynamicTree_lock.release();
        }
        void InsertGameObjectModel(GameObjectModel const& model)
        {
            _dynamicTree_lock.acquire_write();
            _dynamicTree.insert(model);
            _dynamicTree.balance();
            _dynamicTree_lock.release();
        }
        bool ContainsGameObjectModel(GameObjectModel const& model) const
        {
            _dynamicTree_lock.acquire_read();
            bool r = _dynamicTree.contains(model);
            _dynamicTree_lock.release();
            return r;
        }
        bool GetDynamicObjectHitPos(G3D::Vector3 start, G3D::Vector3 end, G3D::Vector3& out, float finalDistMod) const
        {
            _dynamicTree_lock.acquire_read();
            bool r = _dynamicTree.getObjectHitPos(start, end, out, finalDistMod);
            _dynamicTree_lock.release();
            return r;
        }
        float GetDynamicTreeHeight(float x, float y, float z, float maxSearchDist) const
        {
            _dynamicTree_lock.acquire_read();
            float r = _dynamicTree.getHeight(x, y, z, maxSearchDist);
            _dynamicTree_lock.release();
            return r;
        }
        bool CheckDynamicTreeLoS(float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model) const
        {
            _dynamicTree_lock.acquire_read();
            bool r = _dynamicTree.isInLineOfSight(x1, y1, z1, x2, y2, z2, ignoreM2Model);
            _dynamicTree_lock.release();
            return r;
        }

        // Random on map generation
        bool GetReachableRandomPosition(Unit* unit, float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetReachableRandomPointOnGround(float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetRandomPointInTheAir(float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetRandomPointUnderWater(float& x, float& y, float& z, float radius, GridMapLiquidData& liquid_status, bool randomRange = true) const;

        //get corresponding TerrainData object for this particular map
        const TerrainInfo * GetTerrain() const { return m_TerrainData; }

        bool WaypointMovementAutoActive() const;
        bool WaypointMovementPathfinding() const;

        void setNGrid(NGridType* grid, uint32 x, uint32 y);
        NGridType* getNGrid(uint32 x, uint32 y) const
        {
            return i_grids[x][y];
        }

        //per-map script storage
        void ScriptsStart(std::map<uint32, std::multimap<uint32, ScriptInfo> > const& scripts, uint32 id, Object* source, Object* target);
        void ScriptCommandStart(ScriptInfo const& script, uint32 delay, Object* source, Object* target);


        std::string getDebugData();
    private:
        void SetTimer(uint32 t) { i_gridExpiry = t < MIN_GRID_DELAY ? MIN_GRID_DELAY : t; }
        //uint64 CalculateGridMask(const uint32 &y) const;

        void SendInitSelf(Player * player);

        void SendInitTransports(Player * player);
        void SendRemoveTransports(Player * player);

        bool CreatureCellRelocation(Creature *creature, Cell new_cell);

        void AddCreatureToMoveList(Creature *c, float x, float y, float z, float ang);
        CreatureMoveList i_creaturesToMove;

        bool loaded(const GridPair &) const;
        void EnsureGridCreated(const GridPair &);
        void EnsureGridLoaded(Cell const&);

        void buildNGridLinkage(NGridType* pNGridType) { pNGridType->link(this); }

        bool isGridObjectDataLoaded(uint32 x, uint32 y) const { return getNGrid(x,y)->isGridObjectDataLoaded(); }
        void setGridObjectDataLoaded(bool pLoaded, uint32 x, uint32 y) { getNGrid(x,y)->setGridObjectDataLoaded(pLoaded); }

        void ScriptsProcess();

        void CheckHostileRefFor(Player*);
        void SendObjectUpdates();

        typedef std::set<Object*> ObjectSet;
        ObjectSet i_objectsToClientUpdate;

        GObjectMapType                  gameObjectsMap;
        DObjectMapType                  dynamicObjectsMap;
        CreaturesMapType                creaturesMap;
        CreatureIdToGuidListMapType     creatureIdToGuidMap;

        bool m_broken;

    protected:
        ACE_Thread_Mutex Lock;

        MapEntry const* i_mapEntry;
        uint8 i_spawnMode;
        uint32 i_id;
        uint32 i_InstanceId;
        Timer m_unloadTimer;

        float m_ActiveObjectUpdateDistance;

        mutable ACE_RW_Mutex   _dynamicTree_lock;
        DynamicMapTree _dynamicTree;

        MapRefManager m_mapRefManager;
        MapRefManager::iterator m_mapRefIter;

        typedef std::set<WorldObject*> ActiveNonPlayers;
        ActiveNonPlayers m_activeNonPlayers;
        ActiveNonPlayers::iterator m_activeNonPlayersIter;

    private:
        NGridType* i_grids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        //Shared geodata object with map coord info...
        TerrainInfo* const m_TerrainData;

        std::bitset<TOTAL_NUMBER_OF_CELLS_PER_MAP*TOTAL_NUMBER_OF_CELLS_PER_MAP> marked_cells;

        time_t i_gridExpiry;
        WorldUpdateCounter m_updateTracker;

        bool i_scriptLock;
        uint32 m_wanted_delay;

        std::set<WorldObject *> i_objectsToRemove;
        std::map<WorldObject*, bool> i_objectsToSwitch;
        std::multimap<time_t, ScriptAction> m_scriptSchedule;

        // Type specific code for add/remove to/from grid
        template<class T>
        void AddToGrid(T*, NGridType *, Cell const&);

        template<class T>
        void RemoveFromGrid(T*, NGridType *, Cell const&);

        template<class T>
        void DeleteFromWorld(T*);
};

enum InstanceResetMethod
{
    INSTANCE_RESET_ALL,
    INSTANCE_RESET_CHANGE_DIFFICULTY,
    INSTANCE_RESET_GLOBAL,
    INSTANCE_RESET_GROUP_DISBAND,
    INSTANCE_RESET_GROUP_JOIN,
    INSTANCE_RESET_RESPAWN_DELAY
};

class HELLGROUND_IMPORT_EXPORT InstanceMap : public Map
{
    public:
        InstanceMap(uint32 id, time_t, uint32 InstanceId, uint8 SpawnMode);
        ~InstanceMap();
        bool Add(Player *);
        void Remove(Player *, bool);
        void Update(const uint32&);
        void CreateInstanceData(bool load);
        bool Reset(uint8 method);
        uint32 GetScriptId() { return i_script_id; }
        InstanceData* GetInstanceData() { return i_data; }
        void PermBindAllPlayers(Player *player);
        void UnloadAll();
        bool CanEnter(Player* player);
        bool EncounterInProgress(Player *player);
        void SendResetWarnings(uint32 timeLeft) const;
        void SetResetSchedule(bool on);
        virtual void InitVisibilityDistance();
        uint32 GetMaxPlayers() const;

        void SummonUnlootedCreatures();
    private:
        bool m_resetAfterUnload;
        bool m_unloadWhenEmpty;
        bool m_unlootedCreaturesSummoned;
        InstanceData* i_data;
        uint32 i_script_id;
};

class HELLGROUND_IMPORT_EXPORT BattleGroundMap : public Map
{
    public:
        BattleGroundMap(uint32 id, time_t, uint32 InstanceId, BattleGround *bg);
        ~BattleGroundMap();

        bool Add(Player *);
        void Remove(Player *, bool);
        bool CanEnter(Player* player);
        void Update(const uint32&);
        virtual void InitVisibilityDistance();
        void SetUnload();
        void UnloadAll();
        void SetBattleGround(BattleGround *pBg){ m_bg = pBg; }
    private:
        BattleGround *m_bg;
};

template<class T, class CONTAINER>
inline void Map::Visit(const Cell& cell, TypeContainerVisitor<T, CONTAINER> &visitor)
{
    const uint32 x = cell.GridX();
    const uint32 y = cell.GridY();
    const uint32 cell_x = cell.CellX();
    const uint32 cell_y = cell.CellY();

    if (!cell.NoCreate() || loaded(GridPair(x,y)))
    {
        EnsureGridLoaded(cell);
        getNGrid(x, y)->Visit(cell_x, cell_y, visitor);
    }
}

#endif
