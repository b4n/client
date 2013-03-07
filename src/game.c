/*------------------------------------------------------------------------------
    Tune Land - Sandbox RPG
    Copyright (C) 2012-2012
        Antony Martin <antony(dot)martin(at)scengine(dot)org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 -----------------------------------------------------------------------------*/

#include <SDL.h>
#include <SCE/interface/SCEInterface.h>
#include <tunel/common/netprotocol.h>
#include <tunel/common/terrainbrush.h>
#include "game.h"

#define FPS 60

#define verif(cd)\
if (cd) {\
    SCEE_LogSrc ();\
    SCEE_Out ();\
    exit (EXIT_FAILURE);\
}

#define GRID_SIZE 128

#define GW (GRID_SIZE)
#define GH (GRID_SIZE)
#define GD (GRID_SIZE)

#define OCTREE_SIZE 32


typedef enum {
    TERRAIN_AVAILABLE,
    TERRAIN_UNAVAILABLE,
    TERRAIN_QUEUED
} TerrainStatus;

typedef struct terrainchunk TerrainChunk;
struct terrainchunk {
    TerrainStatus status;
    SCE_SVoxelOctreeNode *node;
    SCE_SListIterator it;
};

typedef struct terraintree TerrainTree;
struct terraintree {
    TerrainStatus status;
    SCE_SVoxelWorldTree *tree;
    SCE_SListIterator it;
};


static void TTree_Init (TerrainTree *tree)
{
    tree->status = TERRAIN_UNAVAILABLE;
    tree->tree = NULL;
    SCE_List_InitIt (&tree->it);
    SCE_List_SetData (&tree->it, tree);
}
static void TTree_Clear (TerrainTree *tree)
{
    SCE_List_Remove (&tree->it);
}
static TerrainTree* TTree_New (void)
{
    TerrainTree *tree = NULL;
    if (!(tree = SCE_malloc (sizeof *tree)))
        SCEE_LogSrc ();
    else
        TTree_Init (tree);
    return tree;
}
static void TTree_Free (TerrainTree *tree)
{
    if (tree) {
        TTree_Clear (tree);
        SCE_free (tree);
    }
}

static void TChunk_Init (TerrainChunk *chunk)
{
    chunk->status = TERRAIN_UNAVAILABLE;
    chunk->node = NULL;
    SCE_List_InitIt (&chunk->it);
    SCE_List_SetData (&chunk->it, chunk);
}
static void TChunk_Clear (TerrainChunk *chunk)
{
    SCE_List_Remove (&chunk->it);
}
static TerrainChunk* TChunk_New (void)
{
    TerrainChunk *chunk = NULL;
    if (!(chunk = SCE_malloc (sizeof *chunk)))
        SCEE_LogSrc ();
    else
        TChunk_Init (chunk);
    return chunk;
}
static void TChunk_Free (TerrainChunk *chunk)
{
    if (chunk) {
        TChunk_Clear (chunk);
        SCE_free (chunk);
    }
}



void Game_InitConfig (GameConfig *config)
{
    /* default screen resolution */
    config->screen_w = 1024;
    config->screen_h = 768;
}
void Game_ClearConfig (GameConfig *config)
{
    (void)config;
}

void Game_InitClient (GameClient *gc)
{
    gc->id = 0;
    NetClient_Init (&gc->client);
    memset (gc->nick, 0, GAME_MAX_NICK_LENGTH);
    SCE_Vector3_Set (gc->pos, 0.0, 0.0, 0.0);
}
void Game_ClearClient (GameClient *gc)
{
    NetClient_Clear (&gc->client);
}


#if 0
static GameClient* Game_LocateClientByID (Game *game, int id)
{
    if (game->self.id == id)
        return &game->self;
    else {
        SCE_SListIterator *it = NULL;
        SCE_List_ForEach (it, &game->clients) {
            GameClient *client = SCE_List_GetData (it);
            if (client->id == id)
                return client;
        }
    }
    return NULL;
}

#define GAME_GETCLIENT()                                            \
    Game *game = NULL;                                              \
    GameClient *gc = NULL;                                          \
    game = NetClient_GetData (client);                              \
    gc = Game_LocateClientByID (game, Socket_GetID (packet));       \
    if (!gc) {                                                      \
        SCEE_SendMsg ("received command for an unknown player\n");  \
        return;                                                     \
    }
#endif

/**************** client callbacks ****************/

static void
Game_tlp_get_client_num (NetClient *client, void *cmddata, const char *packet,
                         size_t size)
{
    SockID num = Socket_GetID (packet);
    SCEE_SendMsg ("there are %d clients on the server\n", num);
    (void)cmddata;
}

static void
Game_tlp_connect_accepted (NetClient *client, void *cmddata,
                           const char *packet, size_t size)
{
    Game *game = NULL;
    game = NetClient_GetData (client);
    game->self.id = Socket_GetID (packet);
    game->connected = SCE_TRUE;
    SCEE_SendMsg ("connection accepted! our ID: %d\n", game->self.id);
    (void)cmddata; (void)size;
}
static void
Game_tlp_connect_refused (NetClient *client, void *cmddata,
                          const char *packet, size_t size)
{
    char *msg = NULL;
    Game *game = NetClient_GetData (client);
    msg = SCE_String_NDup (packet, size);
    game->connected = SCE_FALSE;
    SCEE_SendMsg ("connection refused: %s\n", msg);
    SCE_free (msg);
    (void)cmddata; (void)size;
}


static void
Game_tlp_connect (NetClient *client, void *cmddata, const char *packet,
                  size_t size)
{
    SCEE_SendMsg ("client %d connected (packet size is %d)\n",
                  Socket_GetID (packet), size);
    (void)cmddata; (void)client;
}

static void
Game_tlp_disconnect (NetClient *client, void *cmddata, const char *packet,
                     size_t size)
{
    int pid;
#if 0
    GAME_GETCLIENT()
    pid = gc->id;
    if (gc != &game->self)
        Game_RemoveNetClient (game, gc);
#endif
    SCEE_SendMsg ("client %d disconnected\n", pid);
}

static void
Game_tlp_chunk_size (NetClient *client, void *cmddata, const char *packet,
                     size_t size)
{
    (void)cmddata;
    Game *game = NULL;

    game = NetClient_GetData (client);

    /* check if we were expecting such information */
    if (game->chunk_size != 0) {
        SCEE_SendMsg ("unexpected TLP_CHUNK_SIZE packet received.\n");
    } else {
        /* NOTE: dont check packet length? */
        game->chunk_size = SCE_Decode_Long (packet);
    }
}

static void
Game_tlp_num_lod (NetClient *client, void *cmddata, const char *packet,
                  size_t size)
{
    (void)cmddata;
    Game *game = NULL;

    game = NetClient_GetData (client);

    /* check if we were expecting such information */
    if (game->n_lod != 0) {
        SCEE_SendMsg ("unexpected TLP_NUM_LOD packet received.\n");
    } else {
        /* NOTE: dont check packet length? */
        game->n_lod = SCE_Decode_Long (packet);
    }
}

static void
Game_tlp_query_octree (NetClient *client, void *cmddata, const char *p,
                       size_t size)
{
    (void)cmddata;
    Game *game = NULL;
    long x, y, z;
    SCE_SVoxelWorldTree *wt = NULL;
    SCE_SVoxelOctree *vo = NULL;
    TerrainTree *tt = NULL;
    int expected = SCE_FALSE;
    const unsigned char *packet = p;

    game = NetClient_GetData (client);

    /* check if we were expecting such information */
    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    wt = SCE_VWorld_GetTree (game->vw, x, y, z);
    if (wt) {
        vo = SCE_VWorld_GetOctree (wt);
        if (!(tt = SCE_VOctree_GetData (vo)))
            SCEE_SendMsg ("octree: duh, that's kinda unfortunate.\n");
        else if (tt->status == TERRAIN_QUEUED)
            expected = SCE_TRUE;
    }

    if (!expected) {
        SCEE_SendMsg ("unexpected TLP_QUERY_OCTREE packet received.\n");
        return;
    }

    /* NOTE: dont check packet length?.. naah. */

    {
        SCE_SFileSystem fs;
        SCE_SFile fp;

        /* full in-memory file */
        fs = sce_cachefs;
        fs.subfs = &sce_nullfs;

        SCE_File_Init (&fp);
        if (SCE_File_Open (&fp, &fs, "foo", SCE_FILE_READ | SCE_FILE_WRITE) < 0)
            goto fail;

        if (SCE_File_Write (&packet[12], 1, size - 12, &fp) != size - 12)
            goto fail;
        /* TODO: this goto leaves fp open */

        vo = SCE_VWorld_GetOctree (wt);
        SCE_File_Rewind (&fp);
        if (SCE_VOctree_LoadFile (vo, &fp) < 0) {
            SCE_File_Close (&fp);
            goto fail;
        }

        SCE_File_Close (&fp);

        /* save the octree on disk */
        if (SCE_VWorld_SaveTree (game->vw, x, y, z) < 0)
            goto fail;
    }

    tt->status = TERRAIN_AVAILABLE;
    SCE_List_Remove (&tt->it);

    return;
fail:
    SCEE_LogSrc ();
    SCEE_Out ();
    SCEE_Clear ();
}

static void
Game_tlp_query_chunk (NetClient *client, void *cmddata, const char *p,
                      size_t size)
{
    (void)cmddata;
    Game *game = NULL;
    SCEuint level;
    long x, y, z;
    SCE_SVoxelOctreeNode *node = NULL;
    TerrainChunk *tc = NULL;
    SCE_SFile fp;
    int expected = SCE_FALSE;
    const unsigned char *packet = p;

#define PACKET_SIZE 16
    game = NetClient_GetData (client);

    /* check if we were expecting such information */
    level = SCE_Decode_Long (packet);
    x = SCE_Decode_Long (&packet[4]);
    y = SCE_Decode_Long (&packet[8]);
    z = SCE_Decode_Long (&packet[12]);
    node = SCE_VWorld_FetchNode (game->vw, level, x, y, z);
    if (node) {
        if (!(tc = SCE_VOctree_GetNodeData (node)))
            SCEE_SendMsg ("chunk: duh, that's kinda unfortunate.\n");
        else if (tc->status == TERRAIN_QUEUED)
            expected = SCE_TRUE;
    }

    /* TODO: NULL node doesnt mean we are not expecting the chunk:
       a modification to the terrain could have removed the queued node */

    if (!expected) {
        SCEE_SendMsg ("unexpected TLP_QUERY_CHUNK packet received.\n");
        return;
    }

    if (size > PACKET_SIZE) {
        /* write down the file */
        SCE_File_Init (&fp);
        if (SCE_File_Open (&fp, NULL, SCE_VOctree_GetNodeFilename (node),
                           SCE_FILE_CREATE | SCE_FILE_WRITE) < 0)
            goto fail;

        if (SCE_File_Write (&packet[PACKET_SIZE], size-PACKET_SIZE, 1, &fp) < 0)
            goto fail;
        SCE_File_Close (&fp);
    }

    tc->status = TERRAIN_AVAILABLE;
    SCE_List_Remove (&tc->it);
    return;
fail:
    SCEE_LogSrc ();
    SCEE_Out ();
    SCEE_Clear ();
}


static void
Game_tlp_no_octree (NetClient *client, void *cmddata, const char *p,
                    size_t size)
{
    (void)cmddata;
    Game *game = NULL;
    long x, y, z;
    SCE_SVoxelWorldTree *wt = NULL;
    SCE_SVoxelOctree *vo = NULL;
    TerrainTree *tt = NULL;
    int expected = SCE_FALSE;
    const unsigned char *packet = p;

    game = NetClient_GetData (client);

    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    wt = SCE_VWorld_GetTree (game->vw, x, y, z);
    if (wt) {
        vo = SCE_VWorld_GetOctree (wt);
        if (!(tt = SCE_VOctree_GetData (vo)))
            SCEE_SendMsg ("nooctree: duh, that's kinda unfortunate.\n");
        else if (tt->status == TERRAIN_QUEUED)
            expected = SCE_TRUE;
    }

    if (expected) {
        /* TODO: not truely available, but surely the server will notify us
                 when the tree gets added */
        tt->status = TERRAIN_AVAILABLE;
        SCE_List_Remove (&tt->it);
    }
}

static void
Game_tlp_no_chunk (NetClient *client, void *cmddata, const char *p,
                   size_t size)
{
    (void)cmddata;
    Game *game = NULL;
    SCEuint level;
    long x, y, z;
    SCE_SVoxelOctreeNode *node = NULL;
    TerrainChunk *tc = NULL;
    SCE_SFile fp;
    int expected = SCE_FALSE;
    const unsigned char *packet = p;

    game = NetClient_GetData (client);

    level = SCE_Decode_Long (packet);
    x = SCE_Decode_Long (&packet[4]);
    y = SCE_Decode_Long (&packet[8]);
    z = SCE_Decode_Long (&packet[12]);
    node = SCE_VWorld_FetchNode (game->vw, level, x, y, z);
    if (node) {
        if (!(tc = SCE_VOctree_GetNodeData (node)))
            SCEE_SendMsg ("nochunk: duh, that's kinda unfortunate.\n");
        else if (tc->status == TERRAIN_QUEUED)
            expected = SCE_TRUE;
    }

    if (expected) {
        /* TODO: not truely available, but surely the server will notify us
                 when the node gets added */
        tc->status = TERRAIN_AVAILABLE;
        SCE_List_Remove (&tc->it);
    }
}

static void
Game_tlp_edit_terrain (NetClient *client, void *cmddata, const char *p,
                       size_t size)
{
    (void)cmddata;
    Game *game = NULL;
    long x, y, z, w, h, d;
    SCE_SVoxelOctreeNode *node = NULL;
    TerrainChunk *tc = NULL;
    SCE_SLongRect3 rect;
    int expected = SCE_FALSE;
    const unsigned char *packet = p;

    game = NetClient_GetData (client);

    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    w = SCE_Decode_Long (&packet[12]);
    h = SCE_Decode_Long (&packet[16]);
    d = SCE_Decode_Long (&packet[20]);

    SCE_Rectangle3_SetFromOriginl (&rect, x, y, z, w, h, d);

    if (size - 24 != SCE_Rectangle3_GetAreal (&rect)) {
        SCEE_SendMsg ("TLP_EDIT_TERRAIN: packet corrupted: invalid size\n");
        return;
    }

    if (SCE_VWorld_SetRegion (game->vw, &rect, &packet[24]) < 0)
        SCEE_LogSrc ();
    if (SCE_VWorld_GenerateAllLOD (game->vw, 0, &rect) < 0)
        SCEE_LogSrc ();
}


#if 0
static void
Game_snp_ (NetClient *client, void *cmddata, const char *packet, size_t size)
{
    GAME_GETCLIENT()
    (void)cmddata;
}
#endif

static NetClientCmd sc_tcpcmds[TLP_NUM_COMMANDS];
static size_t sc_numtcp = 0;

static void Game_InitAllCommands (void)
{
    size_t i = 0;

    /* TCP commands */
#define SC_SETTCPCMD(id, fun) do {                     \
        NetClient_InitCmd (&sc_tcpcmds[i]);               \
        NetClient_SetCmdID (&sc_tcpcmds[i], id);          \
        NetClient_SetCmdCallback (&sc_tcpcmds[i], fun);   \
        i++;                                           \
    } while (0)
    SC_SETTCPCMD (TLP_GET_CLIENT_NUM, Game_tlp_get_client_num);
    SC_SETTCPCMD (TLP_CONNECT_ACCEPTED, Game_tlp_connect_accepted);
    SC_SETTCPCMD (TLP_CONNECT_REFUSED, Game_tlp_connect_refused);
    SC_SETTCPCMD (TLP_CONNECT, Game_tlp_connect);

    SC_SETTCPCMD (TLP_NUM_LOD, Game_tlp_num_lod);
    SC_SETTCPCMD (TLP_CHUNK_SIZE, Game_tlp_chunk_size);

    SC_SETTCPCMD (TLP_QUERY_OCTREE, Game_tlp_query_octree);
    SC_SETTCPCMD (TLP_QUERY_CHUNK, Game_tlp_query_chunk);
    SC_SETTCPCMD (TLP_NO_OCTREE, Game_tlp_no_octree);
    SC_SETTCPCMD (TLP_NO_CHUNK, Game_tlp_no_chunk);
    SC_SETTCPCMD (TLP_EDIT_TERRAIN, Game_tlp_edit_terrain);
#undef SC_SETTCPCMD
    sc_numtcp = i;
}

int Init_Game (void)
{
    Game_InitAllCommands ();
    return SCE_OK;
}

static void Game_AssignCommands (NetClient *client)
{
    size_t i;
    for (i = 0; i < sc_numtcp; i++)
        NetClient_AddTCPCmd (client, &sc_tcpcmds[i]);
}


void Game_Init (Game *game)
{
    Game_InitConfig (&game->config);
    Game_InitClient (&game->self);
    NetClient_SetData (&game->self.client, game);
    game->connected = SCE_FALSE;
    strcpy (game->server_ip, "0.0.0.0");
    Game_AssignCommands (&game->self.client);
    game->vt = NULL;
    game->scene = NULL;
    game->deferred = NULL;

    /* fsys doesn't need to be initialized */
    SCE_FileCache_InitCache (&game->fcache);
    memset (game->world_path, 0, sizeof game->world_path);
    game->vw = NULL;
    game->chunk_size = 0;
    game->n_lod = 0;
    SCE_List_Init (&game->queued_chunks);
    SCE_List_Init (&game->dl_chunks);
    SCE_List_Init (&game->queued_trees);
    SCE_List_Init (&game->dl_trees);
    game->view_distance = 0;
    game->view_threshold = 0;
}
void Game_Clear (Game *game)
{
    Game_ClearConfig (&game->config);
    Game_ClearClient (&game->self);
    SCE_VTerrain_Delete (game->vt);
    SCE_Scene_Delete (game->scene);
    SCE_Deferred_Delete (game->deferred);

    SCE_FileCache_ClearCache (&game->fcache);
    SCE_VWorld_Delete (game->vw);
    SCE_List_Clear (&game->queued_chunks);
    SCE_List_Clear (&game->dl_chunks);
}
Game* Game_New (void)
{
    Game *game = NULL;
    if (!(game = SCE_malloc (sizeof *game)))
        SCEE_LogSrc ();
    else
        Game_Init (game);
    return game;
}
void Game_Free (Game *game)
{
    if (game) {
        Game_Clear (game);
        SCE_free (game);
    }
}



int Game_InitSubsystem (Game *game)
{
    srand (time (NULL));
    
    if (SDL_Init (SDL_INIT_VIDEO) < 0) {
        fprintf (stderr, "cannot initialize SDL: %s\n", SDL_GetError ());
        return SCE_ERROR;
    }
    SDL_WM_SetCaption ("TuneL v0.0.1 Alpha", NULL);

    if (SDL_SetVideoMode (game->config.screen_w,
                          game->config.screen_h,
                          32, SDL_OPENGL) == NULL) {
        fprintf (stderr, "cannot initialize SDL: %s\n", SDL_GetError ());
        SDL_Quit ();
        return SCE_ERROR;
    }
    SDL_EnableKeyRepeat (100, 15);

    SCE_Init_Interface (stderr, 0);

    SCE_OBJ_ActivateIndicesGeneration (SCE_TRUE);

    return SCE_OK;
}

#define DELAY 10

static int Game_InitConnection (Game *game)
{
    in_addr_t address;
    int port;

    /* setup network */
    Socket_GetAddressAndPortFromStringv (game->server_ip, &address, &port);
    if (NetClient_Connect (&game->self.client, address, port) < 0) {
        SCEE_LogSrc ();
        return SCE_ERROR;
    }

    NetClient_SendTCPString (&game->self.client, TLP_CONNECT, game->self.nick);
    if (NetClient_WaitTCPPacket (&game->self.client, TLP_CONNECT_ACCEPTED,
                                 DELAY) < 0) {
        SCEE_Log (786);
        SCEE_LogMsg ("TLP_CONNECT_ACCEPTED: timeout");
        return SCE_ERROR;
    }

    return SCE_OK;
}

#define MAX_LOD 5

#define SERVER_TERRAINS "terrains"
#define VWORLD_PREFIX "voxeldata"
#define VWORLD_FNAME "vworld.bin"

static int Game_InitTerrain (Game *game)
{
    char path[256] = {0};
    SCE_SFileCache *fcache = NULL;
    SCE_SFileSystem *fsys = NULL;
    SCEuint size;
    NetClient *client = NULL;
    SCE_SVoxelWorld *vw = NULL;

    client = &game->self.client;
    /* retrieve basic terrain data */
    NetClient_SendTCP (client, TLP_CHUNK_SIZE, NULL, 0);
    if (NetClient_WaitTCPPacket (client, TLP_CHUNK_SIZE, DELAY) < 0) {
        SCEE_Log (786);
        SCEE_LogMsg ("TLP_CHUNK_SIZE: timeout");
        return SCE_ERROR;
    }
    NetClient_SendTCP (client, TLP_NUM_LOD, NULL, 0);
    if (NetClient_WaitTCPPacket (client, TLP_NUM_LOD, DELAY) < 0) {
        SCEE_Log (786);
        SCEE_LogMsg ("TLP_NUM_LOD: timeout");
        return SCE_ERROR;
    }

    fcache = &game->fcache;
    fsys = &game->fsys;

    /* create voxel world */
    game->vw = vw = SCE_VWorld_Create ();
    if (!vw) goto fail;

    strcpy (path, "data/"SERVER_TERRAINS"/");
    /* TODO: with some magic (like IP address of the server or some
       generated ID), retrieve server's specific path for terrain data */
    strcat (path, "potager/");
    strcat (path, VWORLD_PREFIX);

    SCE_VWorld_SetPrefix (vw, path);
    *fsys = sce_cachefs;
    fsys->udata = fcache;
    SCE_VWorld_SetFileSystem (vw, fsys);
    SCE_FileCache_SetMaxCachedFiles (fcache, 256);
    SCE_VWorld_SetFileCache (vw, fcache);
    SCE_VWorld_SetMaxCachedNodes (vw, 256);

    strcpy (game->world_path, path);
    strcat (path, "/");
    strcat (path, VWORLD_FNAME);
    /* strcpy (game->world_file, path); */

    if (SCE_VWorld_Load (vw, path) < 0) {
        SCEE_Clear ();
        /* first connection on this server, gotta create the folder.. hmhm */

        /* TODO: create server's folder */

        size = game->chunk_size;
        SCE_VWorld_SetDimensions (vw, size, size, size);
        SCE_VWorld_SetNumLevels (vw, game->n_lod);
    } else {
        /* server folder already exists, check whether dimensions or LOD
           levels have changed since last connection, in which case we'd have
           to remove and download everything */
        if (game->chunk_size != SCE_VWorld_GetWidth (vw) ||
            game->n_lod      != SCE_VWorld_GetNumLevels (vw)) {
            SCEE_SendMsg ("server has changed number of lods and/or "
                          "chunk size.\n");
            /* TODO: REMOVE EVERYTHING!!11 */
            /* reset the world */
            size = game->chunk_size;
            SCE_VWorld_SetDimensions (vw, size, size, size);
            SCE_VWorld_SetNumLevels (vw, game->n_lod);
        }
    }

    if (SCE_VWorld_Build (vw) < 0)
        goto fail;

    return SCE_OK;
fail:
    SCEE_LogSrc ();
    return SCE_ERROR;
}

static int Game_InitDeferred (Game *game)
{
    if (!(game->deferred = SCE_Deferred_Create ()))
        goto fail;
    SCE_Deferred_SetDimensions (game->deferred, game->config.screen_w,
                                game->config.screen_h);
    SCE_Deferred_SetShadowMapsDimensions (game->deferred, 1024, 1024);
    SCE_Deferred_SetCascadedSplits (game->deferred, 3);
    SCE_Deferred_SetCascadedFar (game->deferred, 200.0);
    {
        const char *fnames[SCE_NUM_LIGHT_TYPES];
        fnames[SCE_POINT_LIGHT] = "data/point.glsl";
        fnames[SCE_SUN_LIGHT] = "data/sun.glsl";
        fnames[SCE_SPOT_LIGHT] = "data/spot.glsl";
        if (SCE_Deferred_Build (game->deferred, fnames) < 0)
            goto fail;
    }
    verif (SCEE_HaveError ())
    SCE_Scene_SetDeferred (game->scene, game->deferred);

    return SCE_OK;
fail:
    SCEE_LogSrc ();
    return SCE_ERROR;
}


#define GAME_MAX_DOWNLOADING_PACKETS 5

/* download a single (single really?) queued tree (if any) */
static void Game_DownloadTree (Game *game)
{
    long x, y, z;
    unsigned char buffer[32] = {0};
    TerrainTree *tt = NULL;

    /* TODO: this scheme needs to be improved, let there be a given number of
       elements in dl_trees instead of 0 or 1 */
    if (SCE_List_GetLength (&game->dl_trees) < GAME_MAX_DOWNLOADING_PACKETS &&
        SCE_List_HasElements (&game->queued_trees)) {

        tt = SCE_List_GetData (SCE_List_GetFirst (&game->queued_trees));
        SCE_List_Remove (&tt->it);
        SCE_List_Appendl (&game->dl_trees, &tt->it);
        SCE_VWorld_GetTreeOriginv (tt->tree, &x, &y, &z);

        SCE_Encode_Long (x, buffer);
        SCE_Encode_Long (y, &buffer[4]);
        SCE_Encode_Long (z, &buffer[8]);

        /* TODO: sha1? see server.c:tlp_query_octree() */
        NetClient_SendTCP (&game->self.client, TLP_QUERY_OCTREE, buffer, 12);
    }
}
/* download a single (single really?) queued chunk (if any) */
static void Game_DownloadChunk (Game *game)
{
    long x, y, z;
    /* yeah sha1 sums are less than 32 bytes length but hey. */
    unsigned char buffer[16 + 32] = {0};
    TerrainChunk *tc = NULL;
    SCE_TSha1 sha1;

    if (SCE_List_GetLength (&game->dl_chunks) < GAME_MAX_DOWNLOADING_PACKETS &&
        SCE_List_HasElements (&game->queued_chunks)) {

        tc = SCE_List_GetData (SCE_List_GetFirst (&game->queued_chunks));
        SCE_List_Remove (&tc->it);
        SCE_List_Appendl (&game->dl_chunks, &tc->it);

        SCE_VOctree_GetNodeOriginv (tc->node, &x, &y, &z);
        SCE_Encode_Long (SCE_VOctree_GetNodeLevel (tc->node), buffer);
        SCE_Encode_Long (x, &buffer[4]);
        SCE_Encode_Long (y, &buffer[8]);
        SCE_Encode_Long (z, &buffer[12]);

        if (SCE_Sha1_FileSum (sha1,SCE_VOctree_GetNodeFilename(tc->node)) < 0) {
            if (SCEE_GetCode () != SCE_FILE_NOT_FOUND) {
                SCEE_LogSrc ();
                SCEE_Out ();
                return;         /* :} */
            }
            SCEE_Clear ();
            NetClient_SendTCP (&game->self.client, TLP_QUERY_CHUNK, buffer, 16);
        } else {
            strncpy (&buffer[16], sha1, SCE_SHA1_SIZE);
            NetClient_SendTCP (&game->self.client, TLP_QUERY_CHUNK, buffer,
                               16 + SCE_SHA1_SIZE);
        }
    }
}


static int Game_query_tree (Game *game, SCE_SVoxelWorldTree *wt)
{
    TerrainTree *tree = NULL;

    tree = SCE_VOctree_GetData (SCE_VWorld_GetOctree (wt));
    if (!tree) {
        if (!(tree = TTree_New ())) {
            SCEE_LogSrc ();
            return SCE_ERROR;
        }
        tree->tree = wt;
        SCE_VOctree_SetData (SCE_VWorld_GetOctree (wt), tree);
        SCE_VOctree_SetFreeFunc (SCE_VWorld_GetOctree (wt), TTree_Free);
    }

    if (tree->status == TERRAIN_UNAVAILABLE) {
        SCE_List_Appendl (&game->queued_trees, &tree->it);
        tree->status = TERRAIN_QUEUED;
    }
    return SCE_OK;
}
static int Game_query_chunk (Game *game, SCE_SVoxelOctreeNode *node)
{
    TerrainChunk *chunk = NULL;
    SCEuint level;
    long x, y, z;

    level = SCE_VOctree_GetNodeLevel (node);
    SCE_VOctree_GetNodeOriginv (node, &x, &y, &z);

    chunk = SCE_VOctree_GetNodeData (node);
    if (!chunk) {
        if (!(chunk = TChunk_New ())) {
            SCEE_LogSrc ();
            return SCE_ERROR;
        }
        chunk->node = node;
        SCE_VOctree_SetNodeData (node, chunk);
        SCE_VOctree_SetNodeFreeFunc (node, TChunk_Free);
    }

    if (chunk->status == TERRAIN_UNAVAILABLE) {
        SCE_EVoxelOctreeStatus status = SCE_VOctree_GetNodeStatus (node);
        /* dont query empty or full nodes */
        if (status == SCE_VOCTREE_NODE_EMPTY || status == SCE_VOCTREE_NODE_FULL)
            chunk->status = TERRAIN_AVAILABLE;
        else {
            SCE_List_Appendl (&game->queued_chunks, &chunk->it);
            chunk->status = TERRAIN_QUEUED;
        }
    }
    return SCE_OK;
}

/* run once at every connection to a server to download the terrain */
static int Game_DownloadTerrain (Game *game)
{
    SCE_SLongRect3 rect;
    long distance;
    long x, y, z, d;
    SCE_SList list;
    SCE_SListIterator *it = NULL;
    int i;

    x = game->self.pos[0];
    y = game->self.pos[1];
    z = game->self.pos[2];
    distance = game->view_distance + game->view_threshold;

#ifdef DEBUG
    SCEE_SendMsg ("Game_DownloadTerrain(): downloading trees...\n");
#endif

    /* get needed trees */
    d = distance;
    SCE_Rectangle3_SetFromCenterl (&rect, x, y, z, d, d, d);
    SCE_List_Init (&list);
    if (SCE_VWorld_FetchTrees (game->vw, game->n_lod - 1, &rect, &list) < 0)
        goto fail;
    /* cycle through to queue them */
    SCE_List_ForEach (it, &list) {
        SCE_SVoxelWorldTree *wt = SCE_List_GetData (it);
        if (Game_query_tree (game, wt) < 0)
            goto fail;
    }
    SCE_List_Flush (&list);

    /* download ALL the trees. */
    /* download before queuing nodes, because without the trees we wouldn't
       have any node to queue :) */
    while (SCE_List_HasElements (&game->queued_trees) ||
           SCE_List_HasElements (&game->dl_trees)) {
        Game_DownloadTree (game);
        if (NetClient_WaitTCP (&game->self.client, 1, 0) < 0)
            goto fail;
        if (NetClient_TCPStep (&game->self.client, NULL) < 0)
            goto fail;
    }

#ifdef DEBUG
    SCEE_SendMsg ("Game_DownloadTerrain(): downloading chunks...\n");
#endif
    /* get needed chunks */
    /* download low LOD chunks first, dont download LOD 0 chunks */
    for (i = game->n_lod - 1; i >= 1; i--) {

        SCE_List_Init (&list);
        if (SCE_VWorld_FetchAllNodes (game->vw, i, &list) < 0)
            goto fail;
        /* cycle through to queue them */
        SCE_List_ForEach (it, &list) {
            if (Game_query_chunk (game, SCE_List_GetData (it)) < 0)
                goto fail;
        }
        SCE_List_Flush (&list);
    }

    /* download ALL the chunks. */
    while (SCE_List_HasElements (&game->queued_chunks) ||
           SCE_List_HasElements (&game->dl_chunks)) {
        Game_DownloadChunk (game);
        if (NetClient_WaitTCP (&game->self.client, 1, 0) < 0)
            goto fail;
        if (NetClient_TCPStep (&game->self.client, NULL) < 0)
            goto fail;
    }

    return SCE_OK;
fail:
    SCE_List_Flush (&list);
    SCEE_LogSrc ();
    return SCE_ERROR;
}


static int Game_LOD0ChunksPls (Game *game)
{
    SCE_SLongRect3 rect;
    SCE_SList list;
    SCE_SListIterator *it = NULL;

    SCE_VTerrain_GetRectangle (game->vt, 0, &rect);

    SCE_List_Init (&list);
    if (SCE_VWorld_FetchNodes (game->vw, 0, &rect, &list) < 0)
        goto fail;
    /* cycle through to queue them */
    SCE_List_ForEach (it, &list) {
        if (Game_query_chunk (game, SCE_List_GetData (it)) < 0)
            goto fail;
    }
    SCE_List_Flush (&list);

    /* download ALL the chunks. */
    while (SCE_List_HasElements (&game->queued_chunks) ||
           SCE_List_HasElements (&game->dl_chunks)) {
        Game_DownloadChunk (game);
        if (NetClient_WaitTCP (&game->self.client, 1, 0) < 0)
            goto fail;
        if (NetClient_TCPStep (&game->self.client, NULL) < 0)
            goto fail;
    }

    return SCE_OK;
fail:
    SCE_List_Flush (&list);
    SCEE_LogSrc ();
    return SCE_ERROR;
}


static int Game_UpdateTerrain (Game *game)
{
    SCE_SLongRect3 rect;
    long distance;
    long x, y, z, d;
    SCE_SList list;
    SCE_SListIterator *it = NULL;

    x = game->self.pos[0];
    y = game->self.pos[1];
    z = game->self.pos[2];
    distance = game->view_distance + game->view_threshold;

    /* get needed trees */
    d = distance;
    SCE_Rectangle3_SetFromCenterl (&rect, x, y, z, d, d, d);
    SCE_List_Init (&list);
    if (SCE_VWorld_FetchTrees (game->vw, game->n_lod - 1, &rect, &list) < 0)
        goto fail;
    /* cycle through to queue them */
    SCE_List_ForEach (it, &list) {
        SCE_SVoxelWorldTree *wt = SCE_List_GetData (it);
        if (Game_query_tree (game, wt) < 0)
            goto fail;
        /* TODO: if we need a new tree, we should download all its content
           except LOD 0 chunks */
    }
    SCE_List_Flush (&list);

    /* get needed chunks (only LOD 0 chunks) */
    d = distance;
    SCE_Rectangle3_SetFromCenterl (&rect, x, y, z, d, d, d);
#if 0
    {
        long p1[3], p2[3];
        SCE_Rectangle3_GetPointslv (&rect, p1, p2);
        SCEE_SendMsg ("rect: %ld %ld %ld, %ld %ld %ld\n",
                      p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
    }
#endif
    SCE_List_Init (&list);
    if (SCE_VWorld_FetchNodes (game->vw, 0, &rect, &list) < 0)
        goto fail;
    /* cycle through to queue them */
    SCE_List_ForEach (it, &list) {
        if (Game_query_chunk (game, SCE_List_GetData (it)) < 0)
            goto fail;
    }
    SCE_List_Flush (&list);

    Game_DownloadTree (game);
    Game_DownloadChunk (game);

    return SCE_OK;
fail:
    SCE_List_Flush (&list);
    SCEE_LogSrc ();
    return SCE_ERROR;
}

static int is_region_available (SCE_SVoxelWorld *vw, SCEuint level,
                                const SCE_SLongRect3 *r)
{
    SCE_SList list;
    SCE_SListIterator *it = NULL;

    /* check if all needed octrees are available */
    SCE_List_Init (&list);
    SCE_VWorld_FetchTrees (vw, level, r, &list);
    SCE_List_ForEach (it, &list) {
        TerrainTree *tt = NULL;
        SCE_SVoxelWorldTree *wt = SCE_List_GetData (it);
        tt = SCE_VOctree_GetData (SCE_VWorld_GetOctree (wt));
        if (!tt || tt->status != TERRAIN_AVAILABLE) {
            SCE_List_Flush (&list);
            return SCE_FALSE;
        }
    }
    SCE_List_Flush (&list);

    /* check if all needed nodes are available */
    SCE_List_Init (&list);
    SCE_VWorld_FetchNodes (vw, level, r, &list);
    SCE_List_ForEach (it, &list) {
        TerrainChunk *tc = NULL;
        SCE_SVoxelOctreeNode *node = SCE_List_GetData (it);
        tc = SCE_VOctree_GetNodeData (node);
        if (!tc || tc->status != TERRAIN_AVAILABLE) {
            SCE_List_Flush (&list);
            return SCE_FALSE;
        }
    }
    SCE_List_Flush (&list);

    return SCE_TRUE;
}

static int update_grid (SCE_SVoxelWorld *vw, SCE_SVoxelTerrain *vt,
                        SCEuint level, SCE_EBoxFace f)
{
    long x, y, z;
    long w, h, d;
    long origin_x, origin_y, origin_z;

    /* TODO: let's hope GW >= GH */
    unsigned char buf[GW * GW] = {0};

    SCE_SLongRect3 r;

    w = SCE_VTerrain_GetWidth (vt);
    h = SCE_VTerrain_GetHeight (vt);
    d = SCE_VTerrain_GetDepth (vt);

    SCE_VTerrain_GetOrigin (vt, level, &x, &y, &z);

    switch (f) {
    case SCE_BOX_POSX:
        x += w + 1;
    case SCE_BOX_NEGX:
        x--;
        SCE_Rectangle3_SetFromOriginl (&r, x, y, z, 1, h, d);
        break;

    case SCE_BOX_POSY:
        y += h + 1;
    case SCE_BOX_NEGY:
        y--;
        SCE_Rectangle3_SetFromOriginl (&r, x, y, z, w, 1, d);
        break;

    case SCE_BOX_POSZ:
        z += d + 1;
    case SCE_BOX_NEGZ:
        z--;
        SCE_Rectangle3_SetFromOriginl (&r, x, y, z, w, h, 1);
    }

    if (!is_region_available (vw, level, &r))
        return SCE_FALSE;

    SCE_VWorld_GetRegion (vw, level, &r, buf);
    SCE_VTerrain_AppendSlice (vt, level, f, buf);

    return SCE_TRUE;
}

int Game_Launch (Game *game)
{
    int loop = 1;
    long x, y, z;
    float angle_y = 0., angle_x = 0., back_x = 0., back_y = 0.;
    int mouse_pressed = 0, wait, temps = 0, tm, i, j;
    SCE_SInertVar rx, ry;
    SDL_Event ev;
    SCE_SCamera *cam = NULL;
    float *matrix = NULL;
    SCE_SShader *lodshader = NULL;
    SCE_STexture *diffuse = NULL;
    SCE_SLight *l = NULL;
    SCE_SVoxelWorld *vw = NULL;
    SCE_SLongRect3 rect;

    float dist = GRID_SIZE;
    SCEubyte *buf = NULL;
    int shadows = SCE_FALSE;
    int first_draw = SCE_FALSE;
    int apply_mode = SCE_FALSE;

    /* initialize connection */
    if (Game_InitConnection (game) < 0)
        goto fail;

    /* initialize terrain */
    if (Game_InitTerrain (game) < 0)
        goto fail;

    game->view_distance = GW;
    game->view_threshold = GW / 10;
    /* download terrain */
    if (Game_DownloadTerrain (game) < 0)
        goto fail;

    /* initialize scene */
    game->scene = SCE_Scene_Create ();
    cam = SCE_Camera_Create ();
    matrix = SCE_Camera_GetView (cam);
    SCE_Camera_SetViewport (cam, 0, 0, game->config.screen_w,
                            game->config.screen_h);
    SCE_Camera_SetProjection (cam, 70. * RAD,
                              (float)game->config.screen_w/game->config.screen_h,
                              .1, 1000.);
    SCE_Scene_AddCamera (game->scene, cam);
    
    if (Game_InitDeferred (game) < 0)
        goto fail;

    lodshader = SCE_Shader_Load ("data/render.glsl", SCE_FALSE);
    SCE_Deferred_BuildShader (game->deferred, lodshader);
    SCE_Deferred_BuildPointShadowShader (game->deferred, lodshader);

    game->vt = SCE_VTerrain_Create ();
    /* distance between voxels is 0.5 meters */
    SCE_VTerrain_SetUnit (game->vt, 0.5);
    SCE_VTerrain_SetDimensions (game->vt, GW, GH, GD);
    /* game->view_distance = SCE_VTerrain_GetWidth (game->vt); */

    SCE_VTerrain_CompressPosition (game->vt, SCE_TRUE);
    SCE_VTerrain_CompressNormal (game->vt, SCE_TRUE);
    SCE_VTerrain_SetAlgorithm (game->vt, SCE_VRENDER_MARCHING_CUBES);

    SCE_VTerrain_SetNumLevels (game->vt, game->n_lod);

#if 0
    SCE_VTerrain_SetSubRegionDimension (game->vt, 11);
    SCE_VTerrain_SetNumSubRegions (game->vt, 5);
#elif 1
    SCE_VTerrain_SetSubRegionDimension (game->vt, 21);
    SCE_VTerrain_SetNumSubRegions (game->vt, 5);
#endif
    SCE_VTerrain_BuildShader (game->vt, lodshader);
    SCE_VTerrain_SetShader (game->vt, lodshader);

    if (SCE_VTerrain_Build (game->vt) < 0)
        goto fail;

    /* load and setup diffuse texture of the terrain */
    if (!(diffuse = SCE_Texture_Load (SCE_TEX_2D, 0, 0, 0, 0,
                                      "data/grass2.jpg", NULL)))
        goto fail;
    SCE_Texture_Build (diffuse, SCE_TRUE);
    SCE_VTerrain_SetTopDiffuseTexture (game->vt, diffuse);

    if (!(diffuse = SCE_Texture_Load (SCE_TEX_2D, 0, 0, 0, 0,
                                      "data/rock1.jpg", NULL)))
        goto fail;
    SCE_Texture_Build (diffuse, SCE_TRUE);
    SCE_VTerrain_SetSideDiffuseTexture (game->vt, diffuse);
    if (!(diffuse = SCE_Texture_Load (SCE_TEX_2D, 0, 0, 0, 0,
                                      "data/noise.png", NULL))) {
        SCEE_Out ();
        return 42;
    }
    SCE_Texture_Build (diffuse, SCE_TRUE);
    SCE_VTerrain_SetNoiseTexture (game->vt, diffuse);

    SCE_Scene_SetVoxelTerrain (game->scene, game->vt);

    buf = SCE_malloc (GW * GH * GD * 4);

    /* set position so that GetTheoreticalOrigin() can work */
    x = game->self.pos[0];
    y = game->self.pos[1];
    z = game->self.pos[2];

    SCE_VTerrain_SetPosition (game->vt, x, y, z);

    /* query for visible LOD 0 chunks */
    if (Game_LOD0ChunksPls (game) < 0)
        goto fail;

    for (i = 0; i < game->n_lod; i++) {
        SCE_VTerrain_UpdateGrid (game->vt, i, SCE_FALSE);
        SCE_VTerrain_GetRectangle (game->vt, i, &rect);
        SCE_VWorld_AddUpdatedRegion (game->vw, i, &rect);
    }

    /* sky lighting */
    l = SCE_Light_Create ();
    SCE_Light_SetColor (l, 0.7, 0.8, 1.0);
    SCE_Light_SetIntensity (l, 0.3);
    SCE_Light_SetType (l, SCE_SUN_LIGHT);
    SCE_Light_SetPosition (l, 0., 0., 1.);
    SCE_Scene_AddLight (game->scene, l);

    /* sun lighting */
    l = SCE_Light_Create ();
    SCE_Light_SetColor (l, 1.0, 0.9, 0.85);
    SCE_Light_SetType (l, SCE_SUN_LIGHT);
    SCE_Light_SetPosition (l, 2., 2., 2.);
    SCE_Light_SetShadows (l, shadows);
    SCE_Scene_AddLight (game->scene, l);

    verif (SCEE_HaveError ())

    SCE_Inert_Init (&rx);
    SCE_Inert_Init (&ry);

    SCE_Inert_SetCoefficient (&rx, 0.1);
    SCE_Inert_SetCoefficient (&ry, 0.1);
    SCE_Inert_Accum (&rx, 1);
    SCE_Inert_Accum (&ry, 1);

    game->scene->state->deferred = SCE_TRUE;
    game->scene->state->lighting = SCE_TRUE;
    game->scene->state->frustum_culling = SCE_TRUE;
    game->scene->state->lod = SCE_TRUE;
    /* sky color */
    game->scene->rclear = 0.6;
    game->scene->gclear = 0.7;
    game->scene->bclear = 1.0;
    temps = 0;

    while (loop) {
        int level, res;

        tm = SDL_GetTicks ();

        /* flush pending packets */
        do {
            res = NetClient_PollTCP (&game->self.client);
            if (res < 0) {
                SCEE_LogSrc ();
                goto fail;
            }
            if (res)
                NetClient_TCPStep (&game->self.client, NULL);
        } while (res > 0);

#ifdef DEBUG
        if ((time (NULL) - NetClient_LastPacket (&game->self.client)) > 30) {
            SCEE_SendMsg ("it has been more than 30s since the last packet\n");
        }
#endif

        while (SDL_PollEvent (&ev)) {
            switch (ev.type) {
            case SDL_QUIT: loop = 0; break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button != SDL_BUTTON_WHEELUP &&
                    ev.button.button != SDL_BUTTON_WHEELDOWN) {
                    back_x = ev.button.x;
                    back_y = ev.button.y;
                    mouse_pressed = 1;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                switch (ev.button.button) {
                case SDL_BUTTON_WHEELUP: dist -= 1.; break;
                case SDL_BUTTON_WHEELDOWN: dist += 1.; break;
                default: mouse_pressed = 0;
                }
                break;

            case SDL_MOUSEMOTION:
                if (mouse_pressed) {
                    angle_y += back_x-ev.motion.x;
                    angle_x += back_y-ev.motion.y;
                    SCE_Inert_Operator (&ry, +=, back_x-ev.motion.x);
                    SCE_Inert_Operator (&rx, +=, back_y-ev.motion.y);
                    back_x = ev.motion.x;
                    back_y = ev.motion.y;
                }
                break;

            case SDL_KEYDOWN:
#define SPEED 2
                switch (ev.key.keysym.sym) {
                case SDLK_z: game->self.pos[1] += SPEED; break;
                case SDLK_s: game->self.pos[1] -= SPEED; break;
                case SDLK_d: game->self.pos[0] += SPEED; break;
                case SDLK_q: game->self.pos[0] -= SPEED; break;
                case SDLK_a: game->self.pos[2] += SPEED; break;
                case SDLK_e: game->self.pos[2] -= SPEED; break;
                default:;
                }
                break;

            case SDL_KEYUP:
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: loop = 0; break;
                case SDLK_t:
                    printf ("fps : %.2f\n", 1000./temps);
                    printf ("update time : %d and %d ms\n", i, j);
                    printf ("total time : %d ms\n", temps);
                    break;
                case SDLK_l:
                    game->vt->trans_enabled = !game->vt->trans_enabled;
                    printf ("smooth lod transition: %s\n",
                            game->vt->trans_enabled ? "ENABLED" : "DISABLED");
                    break;
                case SDLK_h:
                    shadows = !shadows;
                    SCE_Light_SetShadows (l, shadows);
                    break;
                case SDLK_SPACE:
                    apply_mode = !apply_mode;
                    break;
                case SDLK_v:
                {
                    unsigned int v = SCE_VRender_GetMaxV ();
                    unsigned int i = SCE_VRender_GetMaxI ();
                    unsigned int lv = SCE_VRender_GetLimitV ();
                    unsigned int li = SCE_VRender_GetLimitI ();
                    printf ("%d of %d (%.1f\%) ; %d of %d (%.1f\%)\n",
                            v, lv, (float)100.0 * v / lv,
                            i, li, (float)100.0 * i / li);
                }
                break;
                default:;
                }
            default:;
            }
        }

        SCE_Inert_Compute (&rx);
        SCE_Inert_Compute (&ry);

        SCE_Matrix4_Translate (matrix, 0., 0., -dist);
        SCE_Matrix4_MulRotX (matrix, -(SCE_Inert_Get (&rx) * 0.2) * RAD);
        SCE_Matrix4_MulRotZ (matrix, -(SCE_Inert_Get (&ry) * 0.2) * RAD);
        SCE_Matrix4_MulTranslate (matrix, 0.0, 0.0, -30.);

        /* integer version of our position */
        x = game->self.pos[0];
        y = game->self.pos[1];
        z = game->self.pos[2];

        if (apply_mode) {
            unsigned char packet[24] = {0};
            SCE_Encode_Long (x, packet);
            SCE_Encode_Long (y, &packet[4]);
            SCE_Encode_Long (z, &packet[8]);
            SCE_Encode_Long (0, &packet[12]);
            SCE_Encode_Long (4, &packet[16]);
            SCE_Encode_Long (TBRUSH_ADD, &packet[20]);
            NetClient_SendTCP (&game->self.client, TLP_EDIT_TERRAIN,
                               packet, 24);
        }

        /* update terrain (check whether we need some parts of the terrain,
           stuff like that) */
        Game_UpdateTerrain (game);

        i = SDL_GetTicks ();

        {
            long missing[3], k;

            SCE_VTerrain_SetPosition (game->vt, x, y, z);

            for (k = 0; k < SCE_VTerrain_GetNumLevels (game->vt); k++) {
                SCE_VTerrain_GetMissingSlices (game->vt, k, &missing[0],
                                               &missing[1], &missing[2]);

                if (0 /* sum of abs(missing) is too big */) {
                    /* update the whole grid */
                } else {
                    while (missing[0] > 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_POSX))
                            break;
                        missing[0]--;
                    }
                    while (missing[0] < 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_NEGX))
                            break;
                        missing[0]++;
                    }
                    while (missing[1] > 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_POSY))
                            break;
                        missing[1]--;
                    }
                    while (missing[1] < 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_NEGY))
                            break;
                        missing[1]++;
                    }
                    while (missing[2] > 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_POSZ))
                            break;
                        missing[2]--;
                    }
                    while (missing[2] < 0) {
                        if (!update_grid (game->vw, game->vt, k, SCE_BOX_NEGZ))
                            break;
                        missing[2]++;
                    }
                }
            }
        }

        while ((level = SCE_VWorld_GetNextUpdatedRegion (game->vw, &rect)) >= 0) {
            SCE_SIntRect3 terrain_ri;
            long origin_x, origin_y, origin_z;
            SCE_SGrid *grid = SCE_VTerrain_GetLevelGrid (game->vt, level);
            int p1[3], p2[3];

            SCE_Rectangle3_IntFromLong (&terrain_ri, &rect);

            /* move the updated area in the terrain grid coordinates */
            SCE_VTerrain_GetOrigin (game->vt, level, &origin_x, &origin_y, &origin_z);
            SCE_Rectangle3_Move (&terrain_ri, -origin_x, -origin_y, -origin_z);

            memset (buf, 0, SCE_Rectangle3_GetAreal (&rect));
            if (SCE_VWorld_GetRegion (game->vw, level, &rect, buf) < 0) {
                SCEE_LogSrc ();
                SCEE_Out ();
                return 434;
            }

            SCE_Grid_SetRegion (grid, &terrain_ri, SCE_VOCTREE_VOXEL_ELEMENTS, buf);
            SCE_VTerrain_UpdateSubGrid (game->vt, level, &terrain_ri, first_draw);
        }

        if (SCE_VWorld_UpdateCache (game->vw) < 0)
            goto fail;
        if (game->fcache.n_cached > game->fcache.max_cached)
            printf ("n_cached = %d\n", game->fcache.n_cached);
        SCE_FileCache_Update (&game->fcache);

        first_draw = SCE_TRUE;

        j = SDL_GetTicks ();
        i = j - i;

        SCE_VTerrain_Update (game->vt);

        j = SDL_GetTicks () - j;

        SCE_Scene_Update (game->scene, cam, NULL, 0);
        SCE_Scene_Render (game->scene, cam, NULL, 0);

#if 1
        {
            SCE_TVector3 center;
        SCE_RLoadMatrix (SCE_MAT_OBJECT, sce_matrix4_id);
        SCE_Scene_UseCamera (cam);
        glDisable (GL_DEPTH_TEST);
        glPointSize (3.0);
        glBegin (GL_POINTS);
        glColor3f (1.0, 0.0, 0.0);
        SCE_Vector3_Copy (center, game->self.pos);
        SCE_Vector3_Operator1 (center, /=, 2.0);
        glVertex3fv (center);
        glEnd ();
        glColor3f (1.0, 1.0, 1.0);
        glEnable (GL_DEPTH_TEST);
        }
#endif


        SDL_GL_SwapBuffers ();

        verif (SCEE_HaveError ())
        temps = SDL_GetTicks () - tm;
        wait = (1000.0/FPS) - temps;
        if (wait > 0)
            SDL_Delay (wait);
    }
    
    NetClient_SendTCP (&game->self.client, TLP_DISCONNECT, NULL, 0);
    NetClient_Disconnect (&game->self.client);

    SCE_Camera_Delete (cam);

    return SCE_OK;
fail:
    SCEE_LogSrc ();
    return SCE_ERROR;
}
