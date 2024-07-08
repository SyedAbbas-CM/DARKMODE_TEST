/*****************************************************************************
The Dark Mod GPL Source Code

This file is part of the The Dark Mod Source Code, originally based
on the Doom 3 GPL Source Code as published in 2011.

The Dark Mod Source Code is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. For details, see LICENSE.TXT.

Project: The Dark Mod (http://www.thedarkmod.com/)

******************************************************************************/

#ifndef __RENDERWORLD_H__
#define __RENDERWORLD_H__

/*
===============================================================================

	Render World

===============================================================================
*/

#define PROC_FILE_EXT				"proc"
#define	PROC_FILE_ID				"mapProcFile003"

// portals
#define NUM_PORTAL_ATTRIBUTES		4 // grayman #3042 - was 3, but I added PS_BLOCK_SOUND

// guis
#define MAX_RENDERENTITY_GUI		3

// shader parms
#define	MAX_GLOBAL_SHADER_PARMS		12

#define SHADERPARM_RED				0
#define SHADERPARM_GREEN			1
#define SHADERPARM_BLUE				2
#define SHADERPARM_ALPHA			3

#define SHADERPARM_TIMESCALE		3
#define SHADERPARM_TIMEOFFSET		4
#define SHADERPARM_DIVERSITY		5	// random between 0.0 and 1.0 for some effects (muzzle flashes, etc)
#define SHADERPARM_MODE				7	// for selecting which shader passes to enable
#define SHADERPARM_TIME_OF_DEATH	7	// for the monster skin-burn-away effect enable and time offset

// model parms
#define SHADERPARM_MD5_SKINSCALE	8	// for scaling vertex offsets on md5 models (jack skellington effect)
#define SHADERPARM_MD3_FRAME		8
#define SHADERPARM_MD3_LASTFRAME	9
#define SHADERPARM_MD3_BACKLERP		10

#define SHADERPARM_BEAM_END_X		8	// for _beam models
#define SHADERPARM_BEAM_END_Y		9
#define SHADERPARM_BEAM_END_Z		10
#define SHADERPARM_BEAM_WIDTH		11

#define SHADERPARM_SPRITE_WIDTH		8
#define SHADERPARM_SPRITE_HEIGHT	9

#define SHADERPARM_PARTICLE_STOPTIME 8	// don't spawn any more particles after this time


typedef bool(*deferredEntityCallback_t)( renderEntity_s *, const renderView_s * );


typedef struct renderEntity_s {
	idRenderModel *			hModel;				// this can only be null if callback is set

	int						entityNum;
	int						bodyId;

	// Entities that are expensive to generate, like skeletal models, can be
	// deferred until their bounds are found to be in view, in the frustum
	// of a shadowing light that is in view, or contacted by a trace / overlay test.
	// This is also used to do visual cueing on items in the view
	// The renderView may be NULL if the callback is being issued for a non-view related
	// source.
	// The callback function should clear renderEntity->callback if it doesn't
	// want to be called again next time the entity is referenced (ie, if the
	// callback has now made the entity valid until the next updateEntity)
	idBounds				bounds;					// only needs to be set for deferred models and md5s
	deferredEntityCallback_t	callback;

	void *					callbackData;			// used for whatever the callback wants

	// player bodies and possibly player shadows should be suppressed in views from
	// that player's eyes, but will show up in mirrors and other subviews
	// security cameras could suppress their model in their subviews if we add a way
	// of specifying a view number for a remoteRenderMap view
	int						suppressSurfaceInViewID;
	int						suppressShadowInViewID;

	// world models for the player and weapons will not cast shadows from view weapon
	// muzzle flashes
	int						suppressShadowInLightID;

	// if non-zero, the surface and shadow (if it casts one)
	// will only show up in the specific view, ie: player weapons
	int						allowSurfaceInViewID;

	// stgatilov #5172: entities with this flag cast shadows
	// regardless of whether light flow can reach them or not
	// used as hacky workaround for issue happening due to caulk overuse on existing missions
	bool					forceShadowBehindOpaque;

	// positioning
	// axis rotation vectors must be unit length for many
	// R_LocalToGlobal functions to work, so don't scale models!
	// axis vectors are [0] = forward, [1] = left, [2] = up
	idVec3					origin;
	idMat3					axis;

	// texturing
	const idMaterial *		customShader;			// if non-0, all surfaces will use this
	const idMaterial *		referenceShader;		// used so flares can reference the proper light shader
	const idDeclSkin *		customSkin;				// 0 for no remappings
	class idSoundEmitter *	referenceSound;			// for shader sound tables, allowing effects to vary with sounds
	float					shaderParms[ MAX_ENTITY_SHADER_PARMS ];	// can be used in any way by shader or model generation

	// networking: see WriteGUIToSnapshot / ReadGUIFromSnapshot
	class idUserInterface * gui[ MAX_RENDERENTITY_GUI ];

	struct renderView_s	*	remoteRenderView;		// any remote camera surfaces will use this

	int						numJoints;
	idJointMat *			joints;					// array of joints that will modify vertices.
													// NULL if non-deformable model.  NOT freed by renderer

	float					modelDepthHack;			// squash depth range so particle effects don't clip into walls

	// options to override surface shader flags (replace with material parameters?)
	bool					noSelfShadow;			// cast shadows onto other objects,but not self
	bool					noShadow;				// no shadow at all
	enum areaLock_t {
		RAL_NONE,
		RAL_ORIGIN,
		RAL_CENTER,
	}						areaLock;				// 2.08 Dragofer's draw call optimization

	bool					noDynamicInteractions;	// don't create any light / shadow interactions after
													// the level load is completed.  This is a performance hack
													// for the gigantic outdoor meshes in the monorail map, so
													// all the lights in the moving monorail don't touch the meshes
    bool                    isLightgem;             //nbohr1more: #4379 lightgem culling
	
	bool					noFog;                  //nbohr1more: #3662 noFog for entities
	
	int						spectrum;				//nbohr1more: #4956 spectrum entity arg
	
	int						lightspectrum;
	
	int						nospectrum;

	bool					weaponDepthHack;		// squash depth range so view weapons don't poke into walls
													// this automatically implies noShadow
	int						forceUpdate;			// force an update (NOTE: not a bool to keep this struct a multiple of 4 bytes)
	int						timeGroup;
	int						xrayIndex;				// 1 - regular entity, no substitute, 2 - xray view substitute, 4 - has substitute
	int						sortOffset;				// 2.08: mappers finetune translucent draw order

#ifndef NDEBUG
	int						_check;
#endif
} renderEntity_t;


typedef struct renderLight_s {
	int						entityNum;			//index of owning idLight in game (foe debugging)

	idMat3					axis;				// rotation vectors, must be unit length
	idVec3					origin;

	// if non-zero, the light will not show up in the specific view,
	// which may be used if we want to have slightly different muzzle
	// flash lights for the player and other views
	int						suppressLightInViewID;

	// if non-zero, the light will only show up in the specific view
	// which can allow player gun gui lights and such to not effect everyone
	int						allowLightInViewID;

	// this is different to *InViewID because a subview can still be VID_PLAYER_VIEW
	int						suppressInSubview; // bitmask: 0 - suppress in subviews, 1 - supress in player views

	bool					noShadows;			// (should we replace this with material parameters on the shader?)
	bool					noSpecular;			// (should we replace this with material parameters on the shader?)

	bool					pointLight;			// otherwise a projection light (should probably invert the sense of this, because points are way more common)
	bool					parallel;			// lightCenter gives the direction to the light at infinity
	bool					parallelSky;		// stgatilov #5121: parallel light which starts in all areas having portalSky material
	idVec3					lightRadius;		// xyz radius for point lights
	idVec3					lightCenter;		// offset the lighting direction for shading and
												// shadows, relative to origin

	// frustum definition for projected lights, all relative to origin
	// FIXME: we should probably have real plane equations here, and offer
	// a helper function for conversion from this format
	idVec3					target;
	idVec3					right;
	idVec3					up;
	idVec3					start;
	idVec3					end;

	// Dmap will generate an optimized shadow volume named _prelight_<lightName>
	// for the light against all the _area* models in the map.  The renderer will
	// ignore this value if the light has been moved after initial creation
	idRenderModel *			prelightModel;

	// muzzle flash lights will not cast shadows from player and weapon world models
	int						lightId;

	const idMaterial *		shader;				// NULL = either lights/defaultPointLight or lights/defaultProjectedLight
	float					shaderParms[MAX_ENTITY_SHADER_PARMS];		// can be used in any way by shader
	idSoundEmitter *		referenceSound;		// for shader sound tables, allowing effects to vary with sounds

	bool					noFogBoundary;		// Stops fogs drawing and fogging their bounding boxes -- SteveL #3664
	bool                    noPortalFog;		// Prevents fog from prematurely closing portals and snapping off lights, matches the material flag from Doom 3	-- nbohr1more #6282
	
	int						spectrum;			//nbohr1more: #4956 spectrum entity arg
	renderEntity_s::areaLock_t areaLock;

	float					volumetricDust;		//stgatilov #5816: strength of volumetric light (in color per meter)
	int						volumetricNoshadows;//stgatilov #5816: use shadows or disable volumetric light?

#ifndef NDEBUG
	int						_check;
#endif
} renderLight_t;

typedef enum {							// #define RENDERTOOLS_SKIP_ID			-1 // DARKMOD_LG_VIEWID
	VID_LIGHTGEM =			-1,			// #define TR_SCREEN_VIEW_ID			 0 // viewIDs of 0 and above are those drawn on screen. Negative numbers are for special
	VID_SUBVIEW =			0,			// non-visible renders: light gem (TDM), Sikk's depth render (Doom3) etc. The player's view
	VID_PLAYER_VIEW =		1			// is 1 for single player mode, multiplayer uses 2+. 0 is for subviews: cameras, reflections etc.*/
} viewID_t;								// The lightgem viewid defines the viewid that is to be used for the lightgem surfacetestmodel
										// static const int	DARKMOD_LG_VIEWID =	-1;

typedef struct renderView_s {
	// player views will set this to a non-zero integer for model suppress / allow
	// subviews (mirrors, cameras, etc) will always clear it to zero
	viewID_t				viewID;

	// sized from 0 to SCREEN_WIDTH / SCREEN_HEIGHT (640/480), not actual resolution
	int						x, y, width, height;

	float					fov_x, fov_y;
	idVec3					vieworg;
	idMat3					viewaxis;			// transformation matrix, view looks down the positive X axis

	bool					cramZNear;			// for cinematics, we want to set ZNear much lower
	bool					forceUpdate;		// for an update 
	bool					isOverlay;			// stgatilov: rendered on top of previous color contents, don't clear

	// time in milliseconds for shader effects and other time dependent rendering issues
	int						time;
	float					shaderParms[MAX_GLOBAL_SHADER_PARMS];		// can be used in any way by shader
	const idMaterial		*globalMaterial;							// used to override everything draw
} renderView_t;


// exitPortal_t is returned by idRenderWorld::GetPortal()
struct exitPortal_t {
	int					areas[2];		// areas connected by this portal
	const idWinding&	w;				// winding points have counter clockwise ordering seen from areas[0]
	int					blockingBits;	// PS_BLOCK_VIEW, PS_BLOCK_AIR, etc
	float				lossPlayer;		// grayman #3042 - sound loss (in dB) for Player-heard sounds
	qhandle_t			portalHandle;
	exitPortal_t(idWinding& _w) : w(_w) {}
};


// guiPoint_t is returned by idRenderWorld::GuiTrace()
typedef struct {
	float				x, y;			// 0.0 to 1.0 range if trace hit a gui, otherwise -1
	int					guiId;			// id of gui ( 0, 1, or 2 ) that the trace happened against
} guiPoint_t;


// modelTrace_t is for tracing vs. visual geometry
typedef struct modelTrace_s {
	float					fraction;			// fraction of trace completed
	idVec3					point;				// end point of trace in global space
	idVec3					normal;				// hit triangle normal vector in global space
	const idMaterial *		material;			// material of hit surface
	const renderEntity_t *	entity;				// render entity that was hit
	const idRenderModel *	model;				// render model that was hit
	int						surfIdx;			// index of model's surface which was hit
	int						jointNumber;		// md5 joint nearest to the hit triangle
} modelTrace_t;


typedef enum {
	PS_BLOCK_NONE =			0,
	PS_BLOCK_VIEW =			1,
	PS_BLOCK_LOCATION =		2,		// game map location strings often stop in hallways
	PS_BLOCK_AIR =			4,		// windows between pressurized and unpresurized areas
	PS_BLOCK_SOUND =		8,		// grayman #3042 - PS_BLOCK_VIEW should not be used to determine sound occlusion

	PS_BLOCK_ALL = (1<<NUM_PORTAL_ATTRIBUTES)-1,
} portalConnection_t;


class idRenderWorld {
public:
	virtual					~idRenderWorld() {};

	// The same render world can be reinitialized as often as desired
	// a NULL or empty mapName will create an empty, single area world
	virtual bool			InitFromMap( const char *mapName ) = 0;

	//-------------- Entity and Light Defs -----------------

	// entityDefs and lightDefs are added to a given world to determine
	// what will be drawn for a rendered scene.  Most update work is defered
	// until it is determined that it is actually needed for a given view.
	virtual	qhandle_t		AddEntityDef( const renderEntity_t *re ) = 0;
	virtual	void			UpdateEntityDef( qhandle_t entityHandle, const renderEntity_t *re ) = 0;
	virtual	void			FreeEntityDef( qhandle_t entityHandle ) = 0;
	virtual const renderEntity_t *GetRenderEntity( qhandle_t entityHandle ) const = 0;

	virtual	qhandle_t		AddLightDef( const renderLight_t *rlight ) = 0;
	virtual	void			UpdateLightDef( qhandle_t lightHandle, const renderLight_t *rlight ) = 0;
	virtual	void			FreeLightDef( qhandle_t lightHandle ) = 0;
	virtual const renderLight_t *GetRenderLight( qhandle_t lightHandle ) const = 0;

	// Force the generation of all light / surface interactions at the start of a level
	// If this isn't called, they will all be dynamically generated
	virtual	void			GenerateAllInteractions() = 0;

	// returns true if this area model needs portal sky to draw
	virtual bool			CheckAreaForPortalSky( int areaNum ) = 0;

	//-------------- Decals and Overlays  -----------------

	// Creates decals on all world surfaces that the winding projects onto.
	// The projection origin should be infront of the winding plane.
	// The decals are projected onto world geometry between the winding plane and the projection origin.
	// The decals are depth faded from the winding plane to a certain distance infront of the
	// winding plane and the same distance from the projection origin towards the winding.
	virtual void			ProjectDecalOntoWorld( const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) = 0;

	// Creates decals on static models.
	virtual void			ProjectDecal( qhandle_t entityHandle, const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) = 0;

	// Creates overlays on dynamic models.
	virtual void			ProjectOverlay( qhandle_t entityHandle, const idPlane localTextureAxis[2], const idMaterial *material ) = 0;

	// Removes all decals and overlays from the given entity def.
	virtual void			RemoveDecals( qhandle_t entityHandle ) = 0;

	//-------------- Scene Rendering -----------------

	// some calls to material functions use the current renderview time when servicing cinematics.  this function
	// ensures that any parms accessed (such as time) are properly set.
	virtual void			SetRenderView( const renderView_t *renderView ) = 0;

	// rendering a scene may actually render multiple subviews for mirrors and portals, and
	// may render composite textures for gui console screens and light projections
	// It would also be acceptable to render a scene multiple times, for "rear view mirrors", etc
	virtual void			RenderScene( const renderView_t &renderView ) = 0;

	//-------------- Portal Area Information -----------------

	// returns the number of portals
	virtual int				NumPortals( void ) const = 0;

	// returns 0 if no portal contacts the bounds
	// This is used by the game to identify portals that are contained
	// inside doors, so the connection between areas can be topologically
	// terminated when the door shuts.
	virtual	qhandle_t		FindPortal( const idBounds &b ) const = 0;

	// doors explicitly close off portals when shut
	// multiple bits can be set to block multiple things, ie: ( PS_VIEW | PS_LOCATION | PS_AIR )
	virtual	void			SetPortalState( qhandle_t portal, int blockingBits ) = 0;
	virtual int				GetPortalState( qhandle_t portal ) = 0;

	// stgatilov #5462: returns the plane of the portal, oriented arbitrarily
	// used to ensure that the origin of door sound is on the right side 
	virtual idPlane			GetPortalPlane( qhandle_t portal ) = 0;

	// grayman #3042 - set portal sound loss (in dB)
	virtual void			SetPortalPlayerLoss( qhandle_t portal, float loss ) = 0;

	// returns true only if a chain of portals without the given connection bits set
	// exists between the two areas (a door doesn't separate them, etc)
	virtual	bool			AreasAreConnected( int areaNum1, int areaNum2, portalConnection_t connection ) = 0;

	// returns the number of portal areas in a map, so game code can build information
	// tables for the different areas
	virtual	int				NumAreas( void ) const = 0;

	// Will return -1 if the point is not in an area, otherwise
	// it will return 0 <= value < NumAreas()
	virtual int				GetAreaAtPoint( const idVec3 &point ) const = 0;

	// stgatilov #6083: returns ANY point inside area with specified number
	// it tries to find a point distant from area boundaries
	//   return 0: success, good point found
	//   return 1: failed to find good point, returned center of area's bounding box
	//   return -1: can't even approximately locate the area! point zeroed
	// warning: this function is very slow!
	// used only for developer tool or "teleportArea"
	virtual int				GetPointInArea( int areaNum, idVec3 &point ) const = 0;

	// fills the *areas array with the numbers of the areas the bounds cover
	// returns the total number of areas the bounds cover
	virtual int				FindAreasInBounds( const idBounds &bounds, int *areas, int maxAreas ) const = 0;

	// Used by the sound system to do area flowing
	virtual	int				NumPortalsInArea( int areaNum ) = 0;

	// returns one portal from an area
	virtual exitPortal_t	GetPortal( int areaNum, int portalNum ) = 0;

	//-------------- Tracing  -----------------

#if 0
	// Checks a ray trace against any gui surfaces in an entity, returning the
	// fraction location of the trace on the gui surface, or -1,-1 if no hit.
	// This doesn't do any occlusion testing, simply ignoring non-gui surfaces.
	// start / end are in global world coordinates.
	virtual guiPoint_t		GuiTrace( qhandle_t entityHandle, const idVec3 start, const idVec3 end ) const = 0;
#endif

	// Traces vs the render model, possibly instantiating a dynamic version, and returns true if something was hit
	virtual bool			ModelTrace( modelTrace_t &trace, qhandle_t entityHandle, const idVec3 &start, const idVec3 &end, const float radius ) const = 0;

	// Traces vs the whole rendered world.
	// stgatilov: calls new TraceAll method internally
	virtual bool			Trace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end, const float radius, bool skipDynamic = true, bool skipPlayer = false ) const = 0;

	// Traces vs the world model bsp tree.
	virtual bool			FastWorldTrace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end ) const = 0;

	virtual bool			MaterialTrace( const idVec3 &p, const idMaterial *mat, idStr &matName ) const = 0;

	typedef bool (*TraceFilterFunc)(void *context, const renderEntity_t *, const idRenderModel *, const idMaterial *);
	// stgatilov: traces the whole rendered world with flexible filtering
	// if fastWorld is true, then:
	//    1) filter is not called for world-area models (defaults to true)
	//    2) does not return entity/material if trace hits world
	//    3) works faster due to BSP traversal
	virtual bool			TraceAll(
		modelTrace_t &trace,
		const idVec3 &start, const idVec3 &end,
		bool fastWorld = false, float radius = 0.0f,
		TraceFilterFunc filterCallback = nullptr, void *context = nullptr
	) const = 0;

	// stgatilov #6546: querying light value at various points in space
	typedef int lightQuery_t;
	virtual lightQuery_t	LightAtPointQuery_AddQuery( qhandle_t onEntity, const samplePointOnModel_t &point, const idList<qhandle_t> &ignoredEntities ) = 0;
	virtual bool			LightAtPointQuery_CheckResult( lightQuery_t query, idVec3 &outputValue, idVec3& outputPosition ) const = 0;
	virtual void			LightAtPointQuery_Forget( lightQuery_t query ) = 0;

	//-------------- Demo Control  -----------------

	// Writes a loadmap command to the demo, and clears archive counters.
	virtual void			StartWritingDemo( idDemoFile *demo ) = 0;
	virtual void			StopWritingDemo() = 0;

	// Returns true when demoRenderView has been filled in.
	// adds/updates/frees entityDefs and lightDefs based on the current demo file
	// and returns the renderView to be used to render this frame.
	// a demo file may need to be advanced multiple times if the framerate
	// is less than 30hz
	// demoTimeOffset will be set if a new map load command was processed before
	// the next renderScene
	virtual bool			ProcessDemoCommand( idDemoFile *readDemo, renderView_t *demoRenderView, int *demoTimeOffset ) = 0;

	// this is used to regenerate all interactions ( which is currently only done during influences ), there may be a less 
	// expensive way to do it
	virtual void			RegenerateWorld() = 0;

	//-------------- Debug Visualization  -----------------

	// Line drawing for debug visualization
	virtual void			DebugClearLines( int time ) = 0;		// a time of 0 will clear all lines and text
	virtual void			DebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugArrow( const idVec4 &color, const idVec3 &start, const idVec3 &end, int size, const int lifetime = 0 ) = 0;
	virtual void			DebugWinding( const idVec4 &color, const idWinding &w, const idVec3 &origin, const idMat3 &axis, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugCircle( const idVec4 &color, const idVec3 &origin, const idVec3 &dir, const float radius, const int numSteps, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugSphere( const idVec4 &color, const idSphere &sphere, const int lifetime = 0, bool depthTest = false ) = 0;
	virtual void			DebugBounds( const idVec4 &color, const idBounds &bounds, const idVec3 &org = vec3_origin, const int lifetime = 0 ) = 0;
	virtual void			DebugBox( const idVec4 &color, const idBox &box, const int lifetime = 0 ) = 0;
	virtual void			DebugFilledBox( const idVec4 &color, const idBox &box, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugFrustum( const idVec4 &color, const idFrustum &frustum, const bool showFromOrigin = false, const int lifetime = 0 ) = 0;
	virtual void			DebugCone( const idVec4 &color, const idVec3 &apex, const idVec3 &dir, float radius1, float radius2, const int lifetime = 0 ) = 0;
	virtual void			DebugAxis( const idVec3 &origin, const idMat3 &axis ) = 0;

	// Polygon drawing for debug visualization.
	virtual void			DebugClearPolygons( int time ) = 0;		// a time of 0 will clear all polygons
	virtual void			DebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime = 0, const bool depthTest = false ) = 0;

	// Text drawing for debug visualization.
	virtual void			DebugText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align = 1, const int lifetime = 0, bool depthTest = false ) = 0;
};

#endif /* !__RENDERWORLD_H__ */
