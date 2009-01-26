/**
 * @file m_node_model.c
 * @todo use MN_GetNodeAbsPos instead of menu->pos
 * @todo cleanup old depandancy between menu->name and view (menuPtr...)
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_mesh.h"
#include "../../renderer/r_mesh_anim.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "m_node_model.h"
#include "m_node_abstractnode.h"

#define ROTATE_SPEED	0.5
#define MAX_OLDREFVALUE MAX_VAR

static nodeBehaviour_t *modelBehaviour;

/**
 * @brief Add a menu link to menumodel definition for faster access
 * @note Called after all menus are parsed - only once
 */
void MN_LinkMenuModels (void)
{
	int i, j;

	for (i = 0; i < mn.numMenuModels; i++) {
		menuModel_t *m = &mn.menuModels[i];
		for (j = 0; j < m->menuTransformCnt; j++) {
			m->menuTransform[j].menuPtr = MN_GetMenu(m->menuTransform[j].menuID);
#if 0	/* should be check, but all this code will be soon removed and view ID is now not every time a menu name */
			if (m->menuTransform[j].menuPtr == NULL)
				Com_Printf("Could not find menu '%s' as requested by menumodel '%s'\n", m->menuTransform[j].menuID, m->id);

			/* we don't need this anymore */
			Mem_Free(m->menuTransform[j].menuID);
			m->menuTransform[j].menuID = NULL;
#endif
		}
	}
}

/**
 * @brief Returns pointer to menu model
 * @param[in] menuModel menu model id from script files
 * @return menuModel_t pointer
 */
menuModel_t *MN_GetMenuModel (const char *menuModel)
{
	int i;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		if (!Q_strncmp(m->id, menuModel, MAX_VAR))
			return m;
	}
	return NULL;
}

void MN_ListMenuModels_f (void)
{
	int i;

	/* search for menumodels with same name */
	Com_Printf("menu models: %i\n", mn.numMenuModels);
	for (i = 0; i < mn.numMenuModels; i++)
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", mn.menuModels[i].id, mn.menuModels[i].model, mn.menuModels[i].need);
}


#ifdef DEBUG
/**
 * @brief This function allows you inline transforming of models.
 * @note Changes you made are lost on quit
 */
static void MN_SetModelTransform_f (void)
{
	menuNode_t *node;
	menuModel_t *model;
	const char *command, *nodeOrMenuID, *menuModel;
	float x, y ,z;
	vec3_t value;

	assert(!Q_strcmp(modelBehaviour->name, "model"));	/**< Make sure the code dont move the behaviours */

	/* not initialized yet - commandline? */
	if (mn.menuStackPos <= 0)
		return;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s [<model> <menu>] | [<node>] <x> <y> <z>\n", Cmd_Argv(0));
		Com_Printf("<model> <menu> is needed for menumodel definitions\n");
		Com_Printf("<node> is needed for 'normal' models\n");
		return;
	}

	if (Cmd_Argc() == 5) {
		command = Cmd_Argv(0);
		menuModel = NULL;
		nodeOrMenuID = Cmd_Argv(1);
		x = atof(Cmd_Argv(2));
		y = atof(Cmd_Argv(3));
		z = atof(Cmd_Argv(4));
	} else {
		command = Cmd_Argv(0);
		menuModel = Cmd_Argv(1);
		nodeOrMenuID = Cmd_Argv(2);
		x = atof(Cmd_Argv(3));
		y = atof(Cmd_Argv(4));
		z = atof(Cmd_Argv(5));
	}

	VectorSet(value, x, y, z);

	if (menuModel) {
		model = MN_GetMenuModel(menuModel);
		if (!model) {
			Com_Printf("MN_SetModelTransform_f: model \"%s\" wasn't found\n", menuModel);
			return;
		}

		if (model->menuTransformCnt) {
			const menu_t *menu = MN_GetActiveMenu();
			int i;

			for (i = 0; i < model->menuTransformCnt; i++) {
				if (menu == model->menuTransform[i].menuPtr) {
					if (!Q_strcmp(command, "debug_mnscale")) {
						VectorCopy(value, model->menuTransform[i].scale);
					} else if (!Q_strcmp(command, "debug_mnangles")) {
						VectorCopy(value, model->menuTransform[i].angles);
					} else if (!Q_strcmp(command, "debug_mnorigin")) {
						VectorCopy(value, model->menuTransform[i].origin);
					}
					break;
				}
			}
		} else {
			Com_Printf("MN_SetModelTransform_f: no entry in menumodel '%s' for menu '%s'\n", menuModel, nodeOrMenuID);
			return;
		}
	} else {
		/* search the node */
		node = MN_GetNode(MN_GetActiveMenu(), nodeOrMenuID);
		if (!node) {
			/* didn't find node -> "kill" action and print error */
			Com_Printf("MN_SetModelTransform_f: node \"%s\" doesn't exist\n", nodeOrMenuID);
			return;
		} else if (node->behaviour != modelBehaviour) {
			Com_Printf("MN_SetModelTransform_f: node \"%s\" isn't a model node\n", nodeOrMenuID);
			return;
		}

		if (!Q_strcmp(command, "debug_mnscale")) {
			VectorCopy(value, node->scale);
		} else if (!Q_strcmp(command, "debug_mnangles")) {
			VectorCopy(value, node->u.model.angles);
		} else if (!Q_strcmp(command, "debug_mnorigin")) {
			VectorCopy(value, node->u.model.origin);
		}
	}
}
#endif

static void MN_ModelNodeDraw (menuNode_t *node)
{
	const char* ref = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	char source[MAX_VAR];
	if (ref == NULL || ref[0] == '\0')
		source[0] = '\0';
	else
		Q_strncpyz(source, ref, sizeof(source));
	MN_DrawModelNode(node, source);
}

/**
 * @brief Set the Model info view (angle,origin,scale) according to the node definition
 */
static inline void MN_InitModelInfoView (menuNode_t *node, modelInfo_t *mi, menuModel_t *menuModel)
{
	qboolean isInitialised = qfalse;
	vec3_t nodeorigin;
	MN_GetNodeAbsPos(node, nodeorigin);
	nodeorigin[0] += node->size[0] / 2 + node->u.model.origin[0];
	nodeorigin[1] += node->size[1] / 2 + node->u.model.origin[1];
	nodeorigin[2] = node->u.model.origin[2];

	if (menuModel->menuTransformCnt) {
		int i;
		/* search a menuTransforme */
		for (i = 0; i < menuModel->menuTransformCnt; i++) {
			if (node->u.model.viewName != NULL) {
				/** @todo improve the test when its possible */
				if (!Q_strcmp(node->u.model.viewName, menuModel->menuTransform[i].menuID))
					break;
			} else {
				/** @todo remove it when its possible */
				if (node->menu == menuModel->menuTransform[i].menuPtr)
					break;
			}
		}

		/* menuTransforme found */
		if (i != menuModel->menuTransformCnt) {
			/* Use menu scale if defined. */
			if (menuModel->menuTransform[i].useScale) {
				VectorCopy(menuModel->menuTransform[i].scale, mi->scale);
			} else {
				VectorCopy(node->scale, mi->scale);
			}

			/* Use menu angles if defined. */
			if (menuModel->menuTransform[i].useAngles) {
				VectorCopy(menuModel->menuTransform[i].angles, mi->angles);
			} else {
				VectorCopy(node->u.model.angles, mi->angles);
			}

			/* Use menu origin if defined. */
			if (menuModel->menuTransform[i].useOrigin) {
				VectorAdd(nodeorigin, menuModel->menuTransform[i].origin, mi->origin);
			} else {
				VectorCopy(nodeorigin, mi->origin);
			}
			isInitialised = qtrue;
		}
	}

	if (!isInitialised) {
		VectorCopy(node->scale, mi->scale);
		VectorCopy(node->u.model.angles, mi->angles);
		VectorCopy(nodeorigin, mi->origin);
	}

	VectorCopy(node->u.model.center, mi->center);
}

/**
 * @brief Draw a model using "menu model" definition
 * @note "menu model" should be deprecated
 */
static void MN_DrawModelNodeWithMenuModel (menuNode_t *node, const char *source, modelInfo_t *mi, menuModel_t *menuModel)
{
	while (menuModel) {
		/* no animation */
		mi->frame = 0;
		mi->oldframe = 0;
		mi->backlerp = 0;

		assert(menuModel->model);
		mi->model = R_RegisterModelShort(menuModel->model);
		if (!mi->model) {
			menuModel = menuModel->next;
			continue;
		}

		mi->skin = menuModel->skin;
		mi->name = menuModel->model;

		/* set mi pointers to menuModel */
		mi->origin = menuModel->origin;
		mi->angles = menuModel->angles;
		mi->center = menuModel->center;
		mi->color = menuModel->color;
		mi->scale = menuModel->scale;

		/* no tag and no parent means - base model or single model */
		if (!menuModel->tag && !menuModel->parent) {
			const char *ref;
			MN_InitModelInfoView(node, mi, menuModel);
			Vector4Copy(node->color, mi->color);

			/* get the animation given by menu node properties */
			if (node->u.model.animation && *(char *) node->u.model.animation) {
				ref = MN_GetReferenceString(node->menu, node->u.model.animation);
			/* otherwise use the standard animation from modelmenu defintion */
			} else
				ref = menuModel->anim;

			/* only base models have animations */
			if (ref && *ref) {
				animState_t *as = &menuModel->animState;
				const char *anim = R_AnimGetName(as, mi->model);
				/* initial animation or animation change */
				if (!anim || (anim && Q_strncmp(anim, ref, MAX_VAR)))
					R_AnimChange(as, mi->model, ref);
				else
					R_AnimRun(as, mi->model, cls.frametime * 1000);

				mi->frame = as->frame;
				mi->oldframe = as->oldframe;
				mi->backlerp = as->backlerp;
			}
			R_DrawModelDirect(mi, NULL, NULL);
		/* tag and parent defined */
		} else {
			menuModel_t *menuModelParent;
			modelInfo_t pmi;
			vec3_t pmiorigin;
			animState_t *as;
			/* place this menumodel part on an already existing menumodel tag */
			assert(menuModel->parent);
			assert(menuModel->tag);
			menuModelParent = MN_GetMenuModel(menuModel->parent);
			if (!menuModelParent) {
				Com_Printf("Menumodel: Could not get the menuModel '%s'\n", menuModel->parent);
				break;
			}
			pmi.model = R_RegisterModelShort(menuModelParent->model);
			if (!pmi.model) {
				Com_Printf("Menumodel: Could not get the model '%s'\n", menuModelParent->model);
				break;
			}

			pmi.name = menuModelParent->model;

			pmi.origin = pmiorigin;
			pmi.angles = menuModelParent->angles;
			pmi.scale = menuModelParent->scale;
			pmi.center = menuModelParent->center;
			pmi.color = menuModelParent->color;

			pmi.origin[0] = menuModelParent->origin[0] + mi->origin[0];
			pmi.origin[1] = menuModelParent->origin[1] + mi->origin[1];
			pmi.origin[2] = menuModelParent->origin[2];
			/* don't count menuoffset twice for tagged models */
			mi->origin[0] -= node->menu->pos[0];
			mi->origin[1] -= node->menu->pos[1];

			/* autoscale? */
			if (!mi->scale[0]) {
				mi->scale = NULL;
				mi->center = node->size;
			}

			as = &menuModelParent->animState;
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", pmi.name, menuModelParent->anim);
			pmi.frame = as->frame;
			pmi.oldframe = as->oldframe;
			pmi.backlerp = as->backlerp;

			R_DrawModelDirect(mi, &pmi, menuModel->tag);
		}

		/* next */
		menuModel = menuModel->next;
	}

}

static void MN_ModelNodeGetParentFromTag (const char* tag, char *parent, int bufferSize)
{
	char *c;
	Q_strncpyz(parent, tag, bufferSize);
	c = parent;
	/* tag "menuNodeName modelTag" */
	while (*c && *c != ' ')
		c++;
	/* split node name and tag */
	*c++ = 0;
}

/**
 * @brief return the anchor name embeded in a tag
 */
static const char* MN_ModelNodeGetAnchorFromTag (const char* tag)
{
	const char *c = tag;
	assert(tag);
	while (*c != '\0' && *c != ' ')
		c++;
	/* split node name and tag */
	assert(*c != '\0');
	c++;
	return c;
}


/**
 * @todo Menu models should inherit the node values from their parent
 * @todo need to merge menuModel case, and the common case (look to be a copy-pasted code)
 */
void MN_DrawModelNode (menuNode_t *node, const char *source)
{
	modelInfo_t mi;
	menuModel_t *menuModel;
	vec3_t nodeorigin;

	assert(!Q_strcmp(modelBehaviour->name, "model"));	/**< Make sure the code dont move the behaviours */
	assert(MN_NodeInstanceOf(node, "model"));			/**< We use model extradata */

	if (source[0] == '\0')
		return;

	menuModel = node->u.model.menuModel = MN_GetMenuModel(source);
	/* direct model name - no menumodel definition */
	if (!menuModel) {
		/* prevent the searching for a menumodel def in the next frame */
		mi.model = R_RegisterModelShort(source);
		mi.name = source;
		if (!mi.model) {
			Com_Printf("Could not find model '%s'\n", source);
			return;
		}
	}

	/* compute the absolute origin ('origin' property is relative to the node center) */
	MN_GetNodeAbsPos(node, nodeorigin);
	nodeorigin[0] += node->size[0] / 2 + node->u.model.origin[0];
	nodeorigin[1] += node->size[1] / 2 + node->u.model.origin[1];
	nodeorigin[2] = node->u.model.origin[2];

	mi.origin = nodeorigin;
	mi.angles = node->u.model.angles;
	mi.scale = node->scale;
	mi.center = node->u.model.center;
	mi.color = node->color;
	mi.mesh = 0;

	/* autoscale? */
	if (!node->scale[0]) {
		mi.scale = NULL;
		mi.center = node->size;
	}

	/* special case to draw models with "menu model" */
	if (menuModel) {
		MN_DrawModelNodeWithMenuModel(node, source, &mi, menuModel);
		return;
	}

	/* if the node is linked to a parent, the parent will display it */
	if (node->u.model.tag) {
		return;
	}

	/* no animation */
	mi.frame = 0;
	mi.oldframe = 0;
	mi.backlerp = 0;

	/* get skin */
	if (node->dataModelSkinOrCVar && *(char *) node->dataModelSkinOrCVar)
		mi.skin = atoi(MN_GetReferenceString(node->menu, node->dataModelSkinOrCVar));
	else
		mi.skin = 0;

	/* do animations */
	if (node->u.model.animation && *(char *) node->u.model.animation) {
		animState_t *as;
		const char *ref;
		ref = MN_GetReferenceString(node->menu, node->u.model.animation);

		/* check whether the cvar value changed */
		if (Q_strncmp(node->u.model.oldRefValue, source, MAX_OLDREFVALUE)) {
			Q_strncpyz(node->u.model.oldRefValue, source, MAX_OLDREFVALUE);
			/* model has changed but mem is already reserved in pool */
			if (node->u.model.animationState) {
				Mem_Free(node->u.model.animationState);
				node->u.model.animationState = NULL;
			}
		}
		if (!node->u.model.animationState) {
			as = (animState_t *) Mem_PoolAlloc(sizeof(*as), cl_genericPool, CL_TAG_NONE);
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
			R_AnimChange(as, mi.model, ref);
			node->u.model.animationState = as;
		} else {
			const char *anim;
			/* change anim if needed */
			as = node->u.model.animationState;
			if (!as)
				Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
			anim = R_AnimGetName(as, mi.model);
			if (anim && Q_strncmp(anim, ref, MAX_VAR))
				R_AnimChange(as, mi.model, ref);
			R_AnimRun(as, mi.model, cls.frametime * 1000);
		}

		mi.frame = as->frame;
		mi.oldframe = as->oldframe;
		mi.backlerp = as->backlerp;
	}

	/* draw the main model on the node */
	R_DrawModelDirect(&mi, NULL, NULL);

	/* draw all childs */
	if (node->u.model.next) {
		menuNode_t *child;
		modelInfo_t pmi = mi;
		for (child = node->u.model.next; child; child = child->u.model.next) {
			const char *tag;
			char childSource[MAX_VAR];
			const char* childRef;

			/* skip invisible child */
			/** @todo add the 'visiblewhen' test */
			if (child->invis)
				continue;

			memset(&mi, 0, sizeof(mi));
			mi.angles = child->u.model.angles;
			mi.scale = child->scale;
			mi.center = child->u.model.center;
			mi.origin = child->u.model.origin;
			mi.color = pmi.color;

			/* get the anchor name to link the model into the parent */
			tag = MN_GetReferenceString(child->menu, child->u.model.tag);
			tag = MN_ModelNodeGetAnchorFromTag(tag);

			/* init model name */
			childRef = MN_GetReferenceString(child->menu, child->dataImageOrModel);
			if (childRef == NULL || childRef[0] == '\0')
				childSource[0] = '\0';
			else
				Q_strncpyz(childSource, childRef, sizeof(childSource));
			mi.model = R_RegisterModelShort(childSource);
			mi.name = childSource;

			/* init skin */
			if (child->dataModelSkinOrCVar && *(char *) child->dataModelSkinOrCVar)
				mi.skin = atoi(MN_GetReferenceString(child->menu, child->dataModelSkinOrCVar));
			else
				mi.skin = 0;

			R_DrawModelDirect(&mi, &pmi, tag);
		}
	}
}

static int oldMousePosX = 0;
static int oldMousePosY = 0;

static void MN_ModelNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	float *rotateAngles = node->u.model.angles;

	/* rotate a model */
	rotateAngles[YAW] -= ROTATE_SPEED * (x - oldMousePosX);
	rotateAngles[ROLL] += ROTATE_SPEED * (y - oldMousePosY);

	/* clamp the angles */
	while (rotateAngles[YAW] > 360.0)
		rotateAngles[YAW] -= 360.0;
	while (rotateAngles[YAW] < 0.0)
		rotateAngles[YAW] += 360.0;

	if (rotateAngles[ROLL] < 0.0)
		rotateAngles[ROLL] = 0.0;
	else if (rotateAngles[ROLL] > 180.0)
		rotateAngles[ROLL] = 180.0;

	oldMousePosX = x;
	oldMousePosY = y;
}

static void MN_ModelNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	MN_SetMouseCapture(node);
	oldMousePosX = x;
	oldMousePosY = y;
}

static void MN_ModelNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button != K_MOUSE1)
		return;
	MN_MouseRelease();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_ModelNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	node->u.model.oldRefValue = MN_AllocString("", MAX_OLDREFVALUE);
}

static void MN_ModelNodeLoaded (menuNode_t *node)
{
	/* link node with the parent */
	if (node->u.model.tag) {
		char parentName[MAX_VAR];
		menuNode_t *parent;
		menuNode_t *last;
		const char *tag = MN_GetReferenceString(node->menu, node->u.model.tag);

		/* find the parent node */
		MN_ModelNodeGetParentFromTag(tag, parentName, MAX_VAR);
		parent = MN_GetNode(node->menu, parentName);
		if (parent == NULL) {
			Sys_Error("MN_ModelNodeLoaded: Node '%s.%s' not found. Please define parent node before childs\n", node->menu->name, parentName);
		}

		/* find the last child, and like the new one */
		last = parent;
		while (last->u.model.next != NULL)
			last = last->u.model.next;
		last->u.model.next = node;
	}

	if (node->u.model.tag == NULL && (node->size[0] == 0 || node->size[1] == 0)) {
		Com_Printf("MN_ModelNodeLoaded: Please set a pos and size to the node '%s.%s'. Note: 'origin' is a relative value to the center of the node\n", node->menu->name, node->name);
	}
}

/** @brief valid properties for model */
static const value_t properties[] = {
	{"anim", V_CVAR_OR_STRING, offsetof(menuNode_t, u.model.animation), 0},
	{"angles", V_VECTOR, offsetof(menuNode_t, u.model.angles), MEMBER_SIZEOF(menuNode_t, u.model.angles)},
	{"center", V_VECTOR, offsetof(menuNode_t, u.model.center), MEMBER_SIZEOF(menuNode_t, u.model.center)},
	{"origin", V_VECTOR, offsetof(menuNode_t, u.model.origin), MEMBER_SIZEOF(menuNode_t, u.model.origin)},
	{"tag", V_CVAR_OR_STRING, offsetof(menuNode_t, u.model.tag), 0},
	/** @todo use V_REF_OF_STRING when its possible ('viewName' is never a cvar) */
	{"view", V_CVAR_OR_STRING, offsetof(menuNode_t, u.model.viewName), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterModelNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "model";
	behaviour->draw = MN_ModelNodeDraw;
	behaviour->mouseDown = MN_ModelNodeMouseDown;
	behaviour->mouseUp = MN_ModelNodeMouseUp;
	behaviour->loading = MN_ModelNodeLoading;
	behaviour->loaded = MN_ModelNodeLoaded;
	behaviour->capturedMouseMove = MN_ModelNodeCapturedMouseMove;
	behaviour->properties = properties;

#ifdef DEBUG
	Cmd_AddCommand("debug_mnscale", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnangles", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnorigin", MN_SetModelTransform_f, "Transform model from command line.");
#endif
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);

	/* save the behaviour */
	modelBehaviour = behaviour;
}
