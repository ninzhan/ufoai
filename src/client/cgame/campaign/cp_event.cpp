/**
 * @file
 * @brief Geoscape event implementation
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_time.h"

static linkedList_t *eventMails = NULL;

/**
 * @brief Searches all event mails for a given id
 * @note Might also return NULL - always check the return value
 * @note If you want to create mails that base on a script definition but have different
 * body messages, set createCopy to true
 * @param[in] id The id from the script files
 */
eventMail_t* CL_GetEventMail (const char *id)
{
	int i;

	for (i = 0; i < ccs.numEventMails; i++) {
		eventMail_t* mail = &ccs.eventMails[i];
		if (Q_streq(mail->id, id))
			return mail;
	}

	LIST_Foreach(eventMails, eventMail_t, listMail) {
		if (Q_streq(listMail->id, id))
			return listMail;
	}

	return NULL;
}

/**
 * @brief Make sure, that the linked list is freed with every new game
 * @sa CP_ResetCampaignData
 */
void CP_FreeDynamicEventMail (void)
{
	/* the pointers are not freed, this is done with the
	 * pool clear in CP_ResetCampaignData */
	LIST_Delete(&eventMails);
}

/** @brief Valid event mail parameters */
static const value_t eventMail_vals[] = {
	{"subject", V_TRANSLATION_STRING, offsetof(eventMail_t, subject), 0},
	{"from", V_TRANSLATION_STRING, offsetof(eventMail_t, from), 0},
	{"to", V_TRANSLATION_STRING, offsetof(eventMail_t, to), 0},
	{"cc", V_TRANSLATION_STRING, offsetof(eventMail_t, cc), 0},
	{"date", V_TRANSLATION_STRING, offsetof(eventMail_t, date), 0},
	{"body", V_TRANSLATION_STRING, offsetof(eventMail_t, body), 0},
	{"icon", V_HUNK_STRING, offsetof(eventMail_t, icon), 0},
	{"model", V_HUNK_STRING, offsetof(eventMail_t, model), 0},
	{"skipmessage", V_BOOL, offsetof(eventMail_t, skipMessage), MEMBER_SIZEOF(eventMail_t, skipMessage)},

	{NULL, V_NULL, 0, 0}
};

/**
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseEventMails (const char *name, const char **text)
{
	eventMail_t *eventMail;

	if (ccs.numEventMails >= MAX_EVENTMAILS) {
		Com_Printf("CL_ParseEventMails: mail def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the eventMail */
	eventMail = &ccs.eventMails[ccs.numEventMails++];
	OBJZERO(*eventMail);

	Com_DPrintf(DEBUG_CLIENT, "...found eventMail %s\n", name);

	eventMail->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	Com_ParseBlock(name, text, eventMail, eventMail_vals, cp_campaignPool);
}

void CP_CheckCampaignEvents (campaign_t *campaign)
{
	const campaignEvents_t *events = campaign->events;
	int i;

	/* no events for the current campaign */
	if (!events)
		return;

	/* no events in that definition */
	if (!events->numCampaignEvents)
		return;

	for (i = 0; i < events->numCampaignEvents; i++) {
		const campaignEvent_t *event = &events->campaignEvents[i];
		if (event->interest <= ccs.overallInterest) {
			RS_MarkStoryLineEventResearched(event->tech);
		}
	}
}

/**
 * Will return the campaign related events
 * @note Also performs some sanity check
 * @param name The events id
 */
const campaignEvents_t *CP_GetEventsByID (const char *name)
{
	int i;

	for (i = 0; i < ccs.numCampaignEventDefinitions; i++) {
		const campaignEvents_t *events = &ccs.campaignEvents[i];
		if (Q_streq(events->id, name)) {
			int j;
			for (j = 0; j < events->numCampaignEvents; j++) {
				const campaignEvent_t *event = &events->campaignEvents[j];
				if (!RS_GetTechByID(event->tech))
					Sys_Error("Illegal tech '%s' given in events '%s'", event->tech, events->id);
			}
			return events;
		}
	}

	return NULL;
}

/**
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseCampaignEvents (const char *name, const char **text)
{
	const char *errhead = "CL_ParseCampaignEvents: unexpected end of file (mail ";
	const char *token;
	campaignEvents_t* events;

	if (ccs.numCampaignEventDefinitions >= MAX_CAMPAIGNS) {
		Com_Printf("CL_ParseCampaignEvents: max event def limit hit\n");
		return;
	}

	token = Com_EParse(text, errhead, name);
	if (!*text)
		return;

	if (!*text || token[0] != '{') {
		Com_Printf("CL_ParseCampaignEvents: event def '%s' without body ignored\n", name);
		return;
	}

	events = &ccs.campaignEvents[ccs.numCampaignEventDefinitions];
	OBJZERO(*events);
	Com_DPrintf(DEBUG_CLIENT, "...found events %s\n", name);
	events->id = Mem_PoolStrDup(name, cp_campaignPool, 0);
	ccs.numCampaignEventDefinitions++;

	do {
		campaignEvent_t *event;
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (events->numCampaignEvents >= MAX_CAMPAIGNEVENTS) {
			Com_Printf("CL_ParseCampaignEvents: max events per event definition limit hit\n");
			return;
		}

		/* initialize the eventMail */
		event = &events->campaignEvents[events->numCampaignEvents++];
		OBJZERO(*event);

		Mem_PoolStrDupTo(token, (char**) ((char*)event + (int)offsetof(campaignEvent_t, tech)), cp_campaignPool, 0);

		token = Com_EParse(text, errhead, name);
		if (!*text)
			return;

		Com_EParseValue(event, token, V_INT, offsetof(campaignEvent_t, interest), sizeof(int));

		if (event->interest < 0)
			Sys_Error("Illegal interest value in events definition '%s' for tech '%s'\n", events->id, event->tech);
	} while (*text);
}

/**
 * @brief Adds the event mail to the message stack. This message is going to be added to the savegame.
 */
void CL_EventAddMail (const char *eventMailId)
{
	char dateBuf[MAX_VAR] = "";

	eventMail_t* eventMail = CL_GetEventMail(eventMailId);
	if (!eventMail) {
		Com_Printf("CL_EventAddMail: Could not find eventmail with id '%s'\n", eventMailId);
		return;
	}

	if (eventMail->sent) {
		return;
	}

	if (!eventMail->from || !eventMail->to || !eventMail->subject || !eventMail->body) {
		Com_Printf("CL_EventAddMail: mail with id '%s' has incomplete data\n", eventMailId);
		return;
	}

	if (!eventMail->date) {
		dateLong_t date;
		CP_DateConvertLong(&ccs.date, &date);
		Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
			date.year, Date_GetMonthName(date.month - 1), date.day);
		eventMail->date = Mem_PoolStrDup(dateBuf, cp_campaignPool, 0);
	}

	eventMail->sent = true;

	if (!eventMail->skipMessage) {
		message_t *m = MS_AddNewMessage("", va(_("You've got a new mail: %s"), _(eventMail->subject)), MSG_EVENT);
		if (m)
			m->eventMail = eventMail;
		else
			Com_Printf("CL_EventAddMail: Could not add message with id: %s\n", eventMailId);
	}

	UP_OpenEventMail(eventMailId);
}

/**
 * @sa UP_OpenMail_f
 * @sa MS_AddNewMessage
 * @sa UP_SetMailHeader
 * @sa UP_OpenEventMail
 */
void CL_EventAddMail_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <event_mail_id>\n", Cmd_Argv(0));
		return;
	}

	CL_EventAddMail(Cmd_Argv(1));
}
