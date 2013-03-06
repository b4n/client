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

#ifndef H_GAME
#define H_GAME

#include <SCE/interface/SCEInterface.h>
#include <tunel/common/netclient.h>

#define GAME_MAX_NICK_LENGTH 128
#define GAME_MAX_WORLD_PATH_LENGTH 256
#define GAME_IP_LENGTH 24

typedef struct gameconfig GameConfig;
struct gameconfig {
    int screen_w, screen_h;
};

typedef struct gameclient GameClient;
struct gameclient {
    SockID id;                  /* stupid type. */
    NetClient client;
    char nick[GAME_MAX_NICK_LENGTH];
    SCE_TVector3 pos;           /* TODO: of course float sucks but whatever. */
};

typedef struct game Game;
struct game {
    /* config */
    GameConfig config;

    /* network stuff and.. stuff. */
    int connected;
    char server_ip[GAME_IP_LENGTH];
    GameClient self;

    /* rendering stuff */
    SCE_SVoxelTerrain *vt;
    SCE_SScene *scene;
    SCE_SDeferred *deferred;

    /* terrain stuff */
    SCE_SFileCache fcache;
    SCE_SFileSystem fsys;
    /* path of the terrain folder */
    char world_path[GAME_MAX_WORLD_PATH_LENGTH];
    SCE_SVoxelWorld *vw;
    SCEuint chunk_size;
    SCEuint n_lod;
    SCE_SList queued_chunks;    /* queued chunks for download */
    SCE_SList dl_chunks;        /* downloading chunks */
    SCE_SList queued_trees;     /* queued trees for download */
    SCE_SList dl_trees;         /* downloading trees */
    SCEulong view_distance;     /* view distance in voxels */
    SCEulong view_threshold;    /* bonus to view_distance */
};

void Game_InitConfig (GameConfig*);
void Game_ClearConfig (GameConfig*);

void Game_InitClient (GameClient*);
void Game_ClearClient (GameClient*);

int Init_Game (void);

void Game_Init (Game*);
void Game_Clear (Game*);
Game* Game_New (void);
void Game_Free (Game*);

int Game_InitSubsystem (Game*);
int Game_Launch (Game*);

#endif /* guard */
