/**
 * @file sv_send.c
 * @brief Event message handling?
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_send.c
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "server.h"

/*
=============================================================================
EVENT MESSAGES
=============================================================================
*/

/**
 * @sa SV_BroadcastCommand
 */
void SV_ClientCommand (client_t *client, const char *fmt, ...)
{
	va_list ap;
	struct dbuffer *msg = new_dbuffer();

	NET_WriteByte(msg, svc_stufftext);

	va_start(ap, fmt);
	NET_VPrintf(msg, fmt, ap);
	va_end(ap);

	NET_WriteMsg(client->stream, msg);
}

/**
 * @brief Sends text across to be displayed if the level passes
 */
void SV_ClientPrintf (client_t * cl, int level, const char *fmt, ...)
{
	va_list argptr;
	struct dbuffer *msg;

	if (level > cl->messagelevel)
		return;

	msg = new_dbuffer();
	NET_WriteByte(msg, svc_print);
	NET_WriteByte(msg, level);

	va_start(argptr, fmt);
	NET_VPrintf(msg, fmt, argptr);
	va_end(argptr);

	NET_WriteMsg(cl->stream, msg);
}

/**
 * @brief Sends text to all active clients
 */
void SV_BroadcastPrintf (int level, const char *fmt, ...)
{
	va_list argptr;
	struct dbuffer *msg;
	client_t *cl;
	int i;

	msg = new_dbuffer();
	NET_WriteByte(msg, svc_print);
	NET_WriteByte(msg, level);

	va_start(argptr, fmt);
	NET_VPrintf(msg, fmt, argptr);
	va_end(argptr);

	/* echo to console */
	if (sv_dedicated->integer) {
		char copy[1024];
		const int length = sizeof(copy) - 1;

		va_start(argptr, fmt);
		Q_vsnprintf(copy, sizeof(copy), fmt, argptr);
		va_end(argptr);

		/* mask off high bits */
		for (i = 0; i < length && copy[i]; i++)
			copy[i] = copy[i] & 127;
		copy[i] = '\0';
		Com_Printf("%s", copy);
	}

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (level > cl->messagelevel)
			continue;
		if (cl->state < cs_connected)
			continue;
		NET_WriteConstMsg(cl->stream, msg);
	}

	free_dbuffer(msg);
}

/**
 * @brief Sends the contents of msg to a subset of the clients, then frees msg
 * @param[in] mask Bitmask of the players to send the multicast to
 * @param[in,out] msg The message to send to the clients
 */
void SV_Multicast (int mask, struct dbuffer *msg)
{
	client_t *c;
	int j;

	/* send the data to all relevant clients */
	for (j = 0, c = svs.clients; j < sv_maxclients->integer; j++, c++) {
		if (c->state < cs_connected)
			continue;
		if (!(mask & (1 << j)))
			continue;

		/* write the message */
		NET_WriteConstMsg(c->stream, msg);
	}

	free_dbuffer(msg);
}

/**
 * @brief If origin is NULL, the origin is determined from the entity origin or the midpoint of the entity box for bmodels.
 */
void SV_StartSound (int mask, vec3_t origin, edict_t *entity, const char *sound)
{
	vec3_t origin_v;
	struct dbuffer *msg;

	/* use the entity origin unless it is a bmodel or explicitly specified */
	if (!origin) {
		origin = origin_v;
		if (entity->solid == SOLID_BSP) {
			VectorCenterFromMinsMaxs(entity->mins, entity->maxs, origin_v);
			VectorAdd(entity->origin, origin_v, origin_v);
		} else {
			VectorCopy(entity->origin, origin_v);
		}
	}

	msg = new_dbuffer();

	NET_WriteByte(msg, svc_sound);
	NET_WriteString(msg, sound);
	NET_WritePos(msg, origin);

	SV_Multicast(mask, msg);
}
