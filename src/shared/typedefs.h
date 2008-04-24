#ifndef SHARED_TYPEDEFS_H
#define SHARED_TYPEDEFS_H

#include "defines.h"

/*
==============================================================
FILESYSTEM
==============================================================
*/
typedef struct qFILE_s {
	void *z; /**< in case of the file being a zip archive */
	FILE *f; /**< in case the file being part of a pak or the actual file */
	char name[MAX_OSPATH];
	unsigned long filepos;
	unsigned long size;
} qFILE;

typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

/** @brief Links one file onto another - like a symlink */
typedef struct filelink_s {
	struct filelink_s *next;
	char *from;
	int fromlength;
	char *to;
} filelink_t;

typedef struct {
	char name[MAX_QPATH];
	unsigned long filepos;
	unsigned long filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	qFILE handle;
	int numfiles;
	packfile_t *files;
} pack_t;

typedef struct searchpath_s {
	char filename[MAX_OSPATH];
	pack_t *pack;				/* only one of filename / pack will be used */
	struct searchpath_s *next;
} searchpath_t;

/*
==============================================================
MAP CONFIGURATION
==============================================================
*/

typedef int ipos_t;
typedef ipos_t ipos3_t[3];

/**
 * @brief plane_t structure
 */
typedef struct cBspPlane_s {
	vec3_t normal;
	float dist;
	byte type;					/**< for fast side tests */
	byte signbits;				/**< signx + (signy<<1) + (signz<<1) */
	byte pad[2];
} cBspPlane_t;

typedef struct cBspModel_s {
	vec3_t mins, maxs;
	vec3_t origin, angles;		/**< used to orient doors and rotating entities */
	int headnode;
	/** @note Not used by ufo2map */
	int tile;					/**< which tile in assembly */
	/** @note Used only by ufo2map */
	int firstface, numfaces;	/**< submodels just draw faces without walking the bsp tree */
} cBspModel_t;

typedef struct cBspSurface_s {
	char name[MAX_QPATH];	/**< not used except in loading CMod_LoadSurfaces */
	int flags;	/**< not used except in loading CMod_LoadSurfaces */
	int value;	/**< not used except in loading CMod_LoadSurfaces */
} cBspSurface_t;

typedef struct {
	cBspPlane_t *plane;
	vec3_t mins, maxs;
	int children[2];			/**< negative numbers are leafs */
} cBspNode_t;

typedef struct {
	cBspPlane_t *plane;
	cBspSurface_t *surface;
} cBspBrushSide_t;

typedef struct {
	int contentFlags;
	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} cBspLeaf_t;

typedef struct {
	int contentFlags;			/**< the CONTENTS_* mask */
	int numsides;				/**< number of sides for this models - start to count from firstbrushside */
	int firstbrushside;			/**< first brush in the list of this model */
	int checkcount;				/**< to avoid repeated testings */
} cBspBrush_t;

/**
 * @brief Data for line tracing (?)
 */
typedef struct tnode_s {
	int type;
	vec3_t normal;
	float dist;
	int children[2];
	int pad;
} tnode_t;

typedef struct chead_s {
	int cnode;
	int level;
} cBspHead_t;

/**
 * @brief Stores the data of a map tile
 */
typedef struct {
	char name[MAX_QPATH];

	int numbrushsides;
	cBspBrushSide_t *brushsides;

	int numtexinfo;
	cBspSurface_t *surfaces;

	int numplanes;
	cBspPlane_t *planes; /* numplanes + 12 for box hull */

	int numnodes;
	cBspNode_t *nodes; /* numnodes + 6 for box hull */

	int numleafs;
	cBspLeaf_t *leafs;
	int emptyleaf;

	int numleafbrushes;
	unsigned short *leafbrushes;

	int nummodels;
	cBspModel_t *models;

	int numbrushes;
	cBspBrush_t *brushes;

	/* tracing box */
	cBspPlane_t *box_planes;
	int box_headnode;
	cBspBrush_t *box_brush;
	cBspLeaf_t *box_leaf;

	/* line tracing */
	tnode_t *tnodes;
	int numtheads;
	int thead[LEVEL_MAX];
	int theadlevel[LEVEL_MAX];

	int numcheads;
	cBspHead_t cheads[MAX_MAP_NODES];
} mapTile_t;

/**
 * @brief Pathfinding routing structure and tile layout
 * @note Comments strongly WIP!
 *
 * ROUTE
 * 	Information stored in "route"
 *
 * 	connections (see Grid_MoveCheck)
 * 	mask				description
 * 	0x10	0001 0000	connection to +x	(height ignored?)
 * 	0x20	0010 0000	connection to -x	(height ignored?)
 * 	0x40	0100 0000	connection to +y	(height ignored?)
 * 	0x80	1000 0000	connection to -y	(height ignored?)
 *
 * 	See "h = map->route[z][y][x] & 0x0F;" and "if (map->route[az][ay][ax] & 0x0F) > h)" in CM_UpdateConnection
 * 	0x0F	0000 1111	some height info?
 *
 * FALL
 * 	Information about how much you'll fall down from x,y position?
 *	I THINK as long as a bit is set you will fall down ...
 *	See "while (map->fall[ny][nx] & (1 << z)) z--;" in Grid_MoveMark
 *
 * STEP
 *
 *	0000 0000
 *	Access with "step[y][x] & (1 << z)"
 *	Guess:
 *		Each bit if set to 0 if a unit can step on it (e.g. ground or chair) or it's 1 if there is a wall or similar (i.e. it's blocked).
 * 		GERD THINKS it shows stairs and step-on stuff
 *		Search for "sh = (map->step[y][x] & (1 << z)) ? sh_big : sh_low;" and similar.
 *		"sh" seems to mean "step height"
 *
 * AREA
 *	The needed TUs to walk to a given position. (See Grid_MoveLength)
 *
 * AREASTORED
 * 	The stored mask (the cached move) of the routing data. (See Grid_MoveLength)
 *
 * TILE LAYOUT AND PATHING
 *  Maps are comprised of tiles.  Each tile has a number of levels corresponding to entities in game.
 *  All static entities in the tile are located in levels 0-255, with the main world located in 0.
 *  Levels 256-258 are reserved, see LEVEL_* constants in src/shared/shared.h.  Non-static entities
 *  (ET_BREAKABLE and ET_ROTATING, ET_DOOR, etc.) are contained in levels 259 and above. These entities'
 *  models are named *##, beginning from 1, and each corresponds to level LEVEL_STEPON + ##.
 *
 *  The code that handles the pathing has separate checks for the static and non-static levels in a tile.
 *  The static levels have their bounds precalculated by CM_MakeTracingNodes and stored in tile->theads.
 *  The other levels are checked in the fly when Grid_CheckUnit is called.
 *
 */
typedef struct routing_s {
	byte route[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte fall[PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte step[PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte filled[PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	byte area[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte areaStored[PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* forbidden list */
	pos_t **fblist;	/**< pointer to forbidden list (entities are standing here) */
	int fblength;	/**< length of forbidden list (amount of entries) */
} routing_t;

typedef struct plane_s {
	vec3_t	normal;
	vec_t	dist;
	int		type;
	ipos3_t	planeVector[3];
	struct plane_s	*hash_chain;
} plane_t;

typedef struct {
	vec2_t	shift;
	vec_t	rotate;
	vec2_t	scale;
	char	name[32];
	int		surfaceFlags;
	int		value;
} brush_texture_t;

typedef struct {
	int		numpoints;
	vec3_t	p[4];		/* variable sized - @todo - but why 4? */
} winding_t;

typedef struct side_s {
	int			planenum;
	int			texinfo;
	winding_t	*winding;
	struct side_s	*original;	/**< bspbrush_t sides will reference the mapbrush_t sides */
	int			contentFlags;	/**< from miptex */
	int			surfaceFlags;	/**< from miptex */
	qboolean	visible;		/**< choose visible planes first */
	qboolean	tested;			/**< this plane already checked as a split */
	qboolean	bevel;			/**< don't ever use for bsp splitting */
} side_t;

typedef struct brush_s {
	int		entitynum;
	int		brushnum;

	int		contentFlags;

	vec3_t	mins, maxs;

	int		numsides;
	side_t	*original_sides;

	qboolean finished;
	qboolean isTerrain;
	qboolean isGenSurf;
} mapbrush_t;

typedef struct face_s {
	struct face_s	*next;		/**< on node */

	/** the chain of faces off of a node can be merged or split,
	 * but each face_t along the way will remain in the chain
	 * until the entire tree is freed */
	struct face_s	*merged;	/**< if set, this face isn't valid anymore */
	struct face_s	*split[2];	/**< if set, this face isn't valid anymore */

	struct portal_s	*portal;
	int				texinfo;
	int				planenum;
	int				contentFlags;	/**< faces in different contents can't merge */
	winding_t		*w;
	int				numpoints;
	int				vertexnums[MAXEDGES];
} face_t;

typedef struct bspbrush_s {
	struct bspbrush_s	*next;
	vec3_t	mins, maxs;
	int		side, testside;		/**< side of node during construction */
	mapbrush_t	*original;
	int		numsides;
	side_t	sides[6];			/**< variably sized */
} bspbrush_t;

typedef struct node_s {
	/** both leafs and nodes */
	int				planenum;	/**< -1 = leaf node */
	struct node_s	*parent;
	vec3_t			mins, maxs;	/**< valid after portalization */
	bspbrush_t		*volume;	/**< one for each leaf/node */

	/** nodes only */
	side_t			*side;		/**< the side that created the node */
	struct node_s	*children[2];
	face_t			*faces;

	/** leafs only */
	bspbrush_t		*brushlist;	/**< fragments of all brushes in this leaf */
	int				contentFlags;	/**< OR of all brush contents */
	int				area;		/**< for areaportals */
	struct portal_s	*portals;	/**< also on nodes during construction */
} node_t;

typedef struct portal_s {
	plane_t		plane;
	node_t		*onnode;		/**< NULL = outside box */
	node_t		*nodes[2];		/**< [0] = front side of plane */
	struct portal_s	*next[2];
	winding_t	*winding;

	qboolean	sidefound;		/**< false if ->side hasn't been checked */
	side_t		*side;			/**< NULL = non-visible */
	face_t		*face[2];		/**< output face in bsp file */
} portal_t;

typedef struct {
	node_t		*headnode;
	node_t		outside_node;
	vec3_t		mins, maxs;
} tree_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;			/**< for sounds or lights */
	int headnode;
	int firstface, numfaces;	/**< submodels just draw faces without walking the bsp tree */
} dBspModel_t;

typedef struct {
	float point[3];
} dBspVertex_t;

typedef struct {
	vec3_t normal;
	float dist;
	int type;		/**< PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate */
} dBspPlane_t;

typedef struct {
	int planenum;
	int children[2];			/**< negative numbers are -(leafs+1), not nodes */
	short mins[3];				/**< for frustum culling */
	short maxs[3];
	unsigned short firstface;
	unsigned short numfaces;	/**< counting both sides */
} dBspNode_t;

typedef struct texinfo_s {
	float vecs[2][4];				/**< [s/t][xyz offset] */
	int surfaceFlags;			/**< miptex flags + overrides */
	int value;					/**< light emission, etc */
	char texture[32];			/**< texture name */
} dBspTexinfo_t;

/**
 * @note note that edge 0 is never used, because negative edge nums are used for
 * counterclockwise use of the edge in a face
 */
typedef struct {
	unsigned short v[2];		/**< vertex numbers */
} dBspEdge_t;

typedef struct {
	unsigned short planenum;
	short side;

	int firstedge;				/**< we must support > 64k edges */
	short numedges;
	short texinfo;

	/** lighting info */
	int lightofs[LIGHTMAP_MAX];				/**< start of [surfsize] samples */
} dBspFace_t;

typedef struct {
	int contentFlags;				/**< OR of all brushes */

	short area;

	short mins[3];				/**< for frustum culling */
	short maxs[3];

	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} dBspLeaf_t;

typedef struct {
	unsigned short planenum;	/**< facing out of the leaf */
	short texinfo;
} dBspBrushSide_t;

typedef struct {
	int firstbrushside;
	int numsides;
	int contentFlags;				/**< OR of all brushes */
} dBspBrush_t;

typedef struct {
	/* tracing box */
	dBspPlane_t *box_planes;
	int box_headnode;
	dBspBrush_t *box_brush;
	dBspLeaf_t *box_leaf;

	/* line tracing */
	tnode_t *tnodes;
	int numtheads;
	int thead[LEVEL_MAX];
	int theadlevel[LEVEL_MAX];

    /* Used by CM_CompleteBoxTrace */
	int numcheads;
	cBspHead_t cheads[MAX_MAP_NODES];

	/* ---- */
	int 			entdatasize;
	char 			entdata[MAX_MAP_ENTSTRING];

	int				routedatasize;
	byte			routedata[MAX_MAP_ROUTING];

	int				lightdatasize[LIGHTMAP_MAX];
	byte			lightdata[LIGHTMAP_MAX][MAX_MAP_LIGHTING];

	int				nummodels;
	dBspModel_t		models[MAX_MAP_MODELS];

	int				numleafs;
	dBspLeaf_t		leafs[MAX_MAP_LEAFS];
	int 			emptyleaf;

	int				numplanes;
	dBspPlane_t		planes[MAX_MAP_PLANES];

	int				numvertexes;
	dBspVertex_t	vertexes[MAX_MAP_VERTS];

	int				numnodes;
	dBspNode_t		nodes[MAX_MAP_NODES];

	int				numtexinfo;
	dBspTexinfo_t	texinfo[MAX_MAP_TEXINFO];

	int				numfaces;
	dBspFace_t		faces[MAX_MAP_FACES];

	int				numedges;
	dBspEdge_t		edges[MAX_MAP_EDGES];

	int				numleafbrushes;
	unsigned short	leafbrushes[MAX_MAP_LEAFBRUSHES];

	int				numsurfedges;
	int				surfedges[MAX_MAP_SURFEDGES];

	int				numbrushes;
	dBspBrush_t		dbrushes[MAX_MAP_BRUSHES];
	cBspBrush_t		brushes[MAX_MAP_BRUSHES];

	int				numbrushsides;
	dBspBrushSide_t	brushsides[MAX_MAP_BRUSHSIDES];
} dMapTile_t;

#endif /* SHARED_TYPEDEFS_H */
