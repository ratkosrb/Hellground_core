/*
 * Copyright (C) 2012 HellGround <http://www.hellground.pl/>
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

#ifndef SCRIPTMGR_H
#define SCRIPTMGR_H

#include <ace/Singleton.h>
#include <ace/Null_Mutex.h>

#include <map>
#include <string>

class SpellScript;

typedef SpellScript* (*ScriptPtr)();
typedef std::map<uint32, ScriptPtr> SpellScriptMap;

class NScriptMgr
{
    friend class ACE_Singleton<NScriptMgr, ACE_Null_Mutex>;

    public:
        void LoadSpellScripts();

        SpellScript* GetSpellScript(uint32);
        void RegisterScript(uint32, ScriptPtr);

    private:
        SpellScriptMap _spellScripts;
};

#define sScriptMgr (*ACE_Singleton<NScriptMgr, ACE_Null_Mutex>::instance())
#endif
