/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "Creature.h"
#include "World.h"
#include "SpellMgr.h"
#include "Vehicle.h"

//Disable CreatureAI when charmed
void CreatureAI::OnCharmed(bool /*apply*/)
{
    //me->IsAIEnabled = !apply;*/
    me->NeedChangeAI = true;
    me->IsAIEnabled = false;
}

AISpellInfoType * UnitAI::AISpellInfo;
 AISpellInfoType * GetAISpellInfo(uint32 i) { return &CreatureAI::AISpellInfo[i]; }

void CreatureAI::Talk(uint8 id, uint64 WhisperGuid)
{
    sCreatureTextMgr->SendChat(me, id, WhisperGuid);
}

void CreatureAI::DoZoneInCombat(Creature* creature /*= NULL*/)
{
    if (!creature)
        creature = me;

    if (!creature->CanHaveThreatList())
        return;

    Map *map = creature->GetMap();
    if (!map->IsDungeon())                                  //use IsDungeon instead of Instanceable, in case battlegrounds will be instantiated
    {
        sLog->outError("DoZoneInCombat call for map that isn't an instance (creature entry = %d)", creature->GetTypeId() == TYPEID_UNIT ? creature->ToCreature()->GetEntry() : 0);
        return;
    }

    if (!creature->HasReactState(REACT_PASSIVE) && !creature->getVictim())
    {
        if (Unit *target = creature->SelectNearestTarget(50))
            creature->AI()->AttackStart(target);
        else if (creature->isSummon())
        {
            if (Unit *summoner = creature->ToTempSummon()->GetSummoner())
            {
                Unit *target = summoner->getAttackerForHelper();
                if (!target && summoner->CanHaveThreatList() && !summoner->getThreatManager().isThreatListEmpty())
                    target = summoner->getThreatManager().getHostilTarget();
                if (target && (creature->IsFriendlyTo(summoner) || creature->IsHostileTo(target)))
                    creature->AI()->AttackStart(target);
            }
        }
    }

    if (!creature->HasReactState(REACT_PASSIVE) && !creature->getVictim())
    {
        sLog->outError("DoZoneInCombat called for creature that has empty threat list (creature entry = %u)", creature->GetEntry());
        return;
    }

    Map::PlayerList const &PlList = map->GetPlayers();

    if (PlList.isEmpty())
        return;

    for (Map::PlayerList::const_iterator i = PlList.begin(); i != PlList.end(); ++i)
    {
        if (Player* pPlayer = i->getSource())
        {
            if (pPlayer->isGameMaster())
                continue;

            if (pPlayer->isAlive())
            {
                creature->SetInCombatWith(pPlayer);
                pPlayer->SetInCombatWith(creature);
                creature->AddThreat(pPlayer, 0.0f);
            }
        }
    }
}

void CreatureAI::AggroAllPlayersInRange(float fMaxSearchRange, Creature* creature)
{
    if (!creature)
        creature = me;

    std::list<Player*> PlList;

    creature->GetPlayerListInDistance(PlList, fMaxSearchRange);

    if(PlList.empty())
        return;

    for (std::list<Player*>::const_iterator itr = PlList.begin(); itr != PlList.end(); ++itr)
    {
        if (Player* pPlayer = (*itr))
        {        
            if(pPlayer->isGameMaster() || !pPlayer->isAlive())
                continue;

            creature->SetInCombatWith(pPlayer);
            pPlayer->SetInCombatWith(creature);
            creature->AddThreat(pPlayer, 0.0f);
        }
    }

}

// scripts does not take care about MoveInLineOfSight loops
// MoveInLineOfSight can be called inside another MoveInLineOfSight and cause stack overflow
void CreatureAI::MoveInLineOfSight_Safe(Unit *who)
{
    if (m_MoveInLineOfSight_locked == true)
        return;
    m_MoveInLineOfSight_locked = true;
    MoveInLineOfSight(who);
    m_MoveInLineOfSight_locked = false;
}

void CreatureAI::MoveInLineOfSight(Unit *who)
{
    if (me->getVictim())
        return;

    if (me->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET) // non-combat pets should just stand there and look good;)
        return;

    if (me->canStartAttack(who, false))
        AttackStart(who);
}

void CreatureAI::EnterEvadeMode()
{
    if (!_EnterEvadeMode())
        return;

    sLog->outDebug(LOG_FILTER_UNITS, "Creature %u enters evade mode.", me->GetEntry());

    if (!me->GetVehicle()) // otherwise me will be in evade mode forever
    {
        if (Unit *owner = me->GetCharmerOrOwner())
        {
            me->GetMotionMaster()->Clear(false);
            me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle(), MOTION_SLOT_ACTIVE);
        }
        else
            me->GetMotionMaster()->MoveTargetedHome();
    }

    Reset();

    if (me->IsVehicle()) // use the same sequence of addtoworld, aireset may remove all summons!
        me->GetVehicleKit()->Reset();
}

/*void CreatureAI::AttackedBy(Unit* attacker)
{
    if (!me->getVictim())
        AttackStart(attacker);
}*/
