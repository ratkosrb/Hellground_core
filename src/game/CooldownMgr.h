/*
* Copyright (C) 2008-2015 Hellground <http://hellground.net/>
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
#ifndef HELLGROUND_COOLMGR_H
#define HELLGROUND_COOLMGR_H

#include "common.h"
#include "DatabaseEnv.h"

// category cooldown on items
#define ITEM_COOLDOWN_ALL_ITEMS 4
// spellid for comands
#define COMMAND_COOLDOWN 2

class Player;
class ByteBuffer;

class HELLGROUND_IMPORT_EXPORT CooldownMgr
{
    friend class Player; // for RemoveAllSpellCooldowns, RemoveArenaSpellCooldowns, SendInitialSpells
public:
    CooldownMgr() {}
    struct Cooldown
    {
        Cooldown(uint32 s = 0, uint32 d = 0) : start(s), duration(d) {};
        uint32 start;
        uint32 duration;
    };
    typedef std::map<uint32, Cooldown> CooldownList;

    bool HasSpellCooldown(uint32 id, uint32 category) const;
    void AddSpellCooldown(uint32 id, uint32 ms, uint32 category, uint32 categoryms);
    void CancelSpellCooldown(uint32 id, uint32 category);
    uint32 GetCooldownTimeLeft(uint32 id) const;

    void AddItemCooldown(uint32 item, uint32 ms, uint32 category, uint32 categoryms);
    bool HasItemCooldown(uint32 item, uint32 category) const;

    std::string SendCooldownsDebug();
    void WriteCooldowns(ByteBuffer& bb);
    void Clear();
    void LoadFromDB(QueryResultAutoPtr result);
    void SaveToDB(uint32 playerguid);
private:
    CooldownList m_CategoryCooldowns;
    CooldownList m_SpellCooldowns;
    CooldownList m_ItemCooldowns;
    CooldownList m_ItemCatCooldowns;
};

#endif