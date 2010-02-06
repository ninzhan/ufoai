/**
 * @file cp_team.c
 * @brief Team management for the campaign gametype
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "../cl_team.h"
#include "../menu/m_main.h"
#include "cp_campaign.h"
#include "cp_team.h"

/**
 * @brief Updates status of weapon (sets pointers, reloads, etc).
 * @param[in] ed Pointer to equipment definition.
 * @param[in] item An item to update.
 * @return Updated item in any case, even if there was no update.
 * @sa CL_CleanupAircraftCrew
 */
static item_t CP_AddWeaponAmmo (equipDef_t * ed, item_t item)
{
	int i;
	objDef_t *type = item.t;

	assert(ed->numItems[type->idx] > 0);
	ed->numItems[type->idx]--;

	if (type->weapons[0]) {
		/* The given item is ammo or self-contained weapon (i.e. It has firedefinitions. */
		if (type->oneshot) {
			/* "Recharge" the oneshot weapon. */
			item.a = type->ammo;
			item.m = item.t; /* Just in case this hasn't been done yet. */
			Com_DPrintf(DEBUG_CLIENT, "CL_AddWeaponAmmo: oneshot weapon '%s'.\n", type->id);
			return item;
		} else {
			/* No change, nothing needs to be done to this item. */
			return item;
		}
	} else if (!type->reload) {
		/* The given item is a weapon but no ammo is needed,
		 * so fire definitions are in t (the weapon). Setting equal. */
		item.m = item.t;
		return item;
	} else if (item.a) {
		assert(item.m);
		/* The item is a weapon and it was reloaded one time. */
		if (item.a == type->ammo) {
			/* Fully loaded, no need to reload, but mark the ammo as used. */
			if (ed->numItems[item.m->idx] > 0) {
				ed->numItems[item.m->idx]--;
				return item;
			} else {
				/* Your clip has been sold; give it back. */
				item.a = NONE_AMMO;
				return item;
			}
		}
	}

	/* Check for complete clips of the same kind */
	if (item.m && ed->numItems[item.m->idx] > 0) {
		ed->numItems[item.m->idx]--;
		item.a = type->ammo;
		return item;
	}

	/* Search for any complete clips. */
	/** @todo We may want to change this to use the type->ammo[] info. */
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)) {
			if (ed->numItems[i] > 0) {
				ed->numItems[i]--;
				item.a = type->ammo;
				item.m = &csi.ods[i];
				return item;
			}
		}
	}

	/** @todo on return from a mission with no clips left
	 * and one weapon half-loaded wielded by soldier
	 * and one empty in equip, on the first opening of equip,
	 * the empty weapon will be in soldier hands, the half-full in equip;
	 * this should be the other way around. */

	/* Failed to find a complete clip - see if there's any loose ammo
	 * of the same kind; if so, gather it all in this weapon. */
	if (item.m && ed->numItemsLoose[item.m->idx] > 0) {
		item.a = ed->numItemsLoose[item.m->idx];
		ed->numItemsLoose[item.m->idx] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	/** @todo We may want to change this to use the type->ammo[] info. */
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type) && ed->numItemsLoose[i] > item.a) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other
				 * loose ammo of a different (but appropriate) type with
				 * more bullets.  Put the previously found ammo back, so
				 * we'll take the new type. */
				assert(item.m);
				ed->numItemsLoose[item.m->idx] = item.a;
				/* We don't have to accumulate loose ammo into clips
				 * because we did it previously and we create no new ammo */
			}
			/* Found some loose ammo to load the weapon with */
			item.a = ed->numItemsLoose[i];
			ed->numItemsLoose[i] = 0;
			item.m = &csi.ods[i];
		}
	}
	return item;
}

/**
 * @brief Reloads weapons, removes not assigned and resets defaults
 * @param[in] aircraft Pointer to an aircraft for given team.
 * @param[in] ed equipDef_t pointer to equipment
 * @sa CL_AddWeaponAmmo
 * @note Iterate through in container order (right hand, left hand, belt,
 * holster, backpack) at the top level, i.e. each squad member reloads
 * the right hand, then each reloads the left hand, etc. The effect
 * of this is that when things are tight, everyone has the opportunity
 * to get their preferred weapon(s) loaded before anyone is allowed
 * to keep her spares in the backpack or on the floor. We don't want
 * the first person in the squad filling their backpack with spare ammo
 * leaving others with unloaded guns in their hands...
 */
void CL_CleanupAircraftCrew (aircraft_t *aircraft, equipDef_t * ed)
{
	int p, container;

	assert(aircraft);

	Com_DPrintf(DEBUG_CLIENT, "CL_CleanupAircraftCrew:aircraft idx: %i, team size: %i\n",
		aircraft->idx, aircraft->teamSize);

	/* Auto-assign weapons to UGVs/Robots if they have no weapon yet. */
	for (p = 0; p < aircraft->maxTeamSize; p++) {
		if (aircraft->acTeam[p]) {
			character_t *chr = &aircraft->acTeam[p]->chr;
			assert(chr);

			/* This is an UGV */
			if (aircraft->acTeam[p]->ugv) {
				/* Check if there is a weapon and add it if there isn't. */
				if (!chr->inv.c[csi.idRight] || !chr->inv.c[csi.idRight]->item.t)
					cls.i.EquipActorRobot(&cls.i, &chr->inv, chr, INVSH_GetItemByID(aircraft->acTeam[p]->ugv->weapon));
			}
		}
	}

	for (container = 0; container < csi.numIDs; container++) {
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				invList_t *ic, *next;
				character_t *chr = &aircraft->acTeam[p]->chr;
				assert(chr);
#if 0
				/* ignore items linked from any temp container */
				if (csi.ids[container].temp)
					continue;
#endif
				for (ic = chr->inv.c[container]; ic; ic = next) {
					next = ic->next;
					if (ed->numItems[ic->item.t->idx] > 0) {
						ic->item = CP_AddWeaponAmmo(ed, ic->item);
					} else {
						/* Drop ammo used for reloading and sold carried weapons. */
						if (!cls.i.RemoveFromInventory(&cls.i, &chr->inv, &csi.ids[container], ic))
							Com_Error(ERR_DROP, "Could not remove item from inventory");
					}
				}
			}
		}
	}
}

/**
 * @brief Clears all containers that are temp containers (see script definition).
 * @sa CL_UpdateEquipmentMenuParameters_f
 * @sa MP_SaveTeamMultiplayerInfo
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
void CL_CleanTempInventory (base_t* base)
{
	int i, k;

	for (i = 0; i < MAX_EMPLOYEES; i++)
		for (k = 0; k < csi.numIDs; k++)
			if (csi.ids[k].temp) {
				/* idFloor and idEquip are temp */
				ccs.employees[EMPL_SOLDIER][i].chr.inv.c[k] = NULL;
				ccs.employees[EMPL_ROBOT][i].chr.inv.c[k] = NULL;
			}

	if (!base)
		return;

	cls.i.DestroyInventory(&cls.i, &base->bEquipment);
}

/**
 * @brief Updates data about teams in aircraft.
 * @param[in] aircraft Pointer to an aircraft for a desired team.
 * @param[in] employeeType Type of employee for which data is being updated.
 * @returns the number of employees that are in the aircraft and were added to
 * the character list
 */
int CL_UpdateActorAircraftVar (aircraft_t *aircraft, employeeType_t employeeType)
{
	int i;

	assert(aircraft);

	Cvar_Set("mn_hired", va(_("%i of %i"), aircraft->teamSize, aircraft->maxTeamSize));
	Cvar_Set("mn_hirable_count", va("%i", aircraft->maxTeamSize - aircraft->teamSize));
	Cvar_Set("mn_hired_count", va("%i", aircraft->teamSize));
	Cvar_Set("mn_pilotassigned", va("%i", aircraft->pilot != NULL));

	if (aircraft->pilot) {
		Cvar_ForceSet("mn_pilot_name", aircraft->pilot->chr.name);
		Cvar_ForceSet("mn_pilot_body", CHRSH_CharGetBody(&aircraft->pilot->chr));
		Cvar_ForceSet("mn_pilot_head", CHRSH_CharGetHead(&aircraft->pilot->chr));
		Cvar_ForceSet("mn_pilot_skin", va("%i", aircraft->pilot->chr.skin));
	} else {
		Cvar_ForceSet("mn_pilot_name", "");
		Cvar_ForceSet("mn_pilot_body", "");
		Cvar_ForceSet("mn_pilot_head", "");
		Cvar_ForceSet("mn_pilot_skin", "");
	}

	/* update chrDisplayList list (this is the one that is currently displayed) */
	chrDisplayList.num = 0;
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		employee_t *empl = aircraft->acTeam[i];
		assert(chrDisplayList.num < MAX_ACTIVETEAM);
		if (!empl)
			continue; /* Skip unused team-slot. */

		if (empl->type != employeeType)
			continue;

		chrDisplayList.chr[chrDisplayList.num] = &empl->chr;

		/* Sanity check(s) */
		if (!chrDisplayList.chr[chrDisplayList.num])
			Com_Error(ERR_DROP, "CL_UpdateActorAircraftVar: Could not get employee character with idx: %i", chrDisplayList.num);
		Com_DPrintf(DEBUG_CLIENT, "add %s to chrDisplayList (pos: %i)\n", chrDisplayList.chr[chrDisplayList.num]->name, chrDisplayList.num);
		Cvar_ForceSet(va("mn_name%i", chrDisplayList.num), chrDisplayList.chr[chrDisplayList.num]->name);

		/* Update number of displayed team-members. */
		chrDisplayList.num++;
	}

	for (i = chrDisplayList.num; i < MAX_ACTIVETEAM; i++)
		chrDisplayList.chr[i] = NULL;	/* Just in case */

	return chrDisplayList.num;
}
