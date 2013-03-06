/*------------------------------------------------------------------------------
    Tune Land - Sandbox RPG
    Copyright (C) 2012-2013
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

#include <SCE/core/SCECore.h>

#include <tunel/common/netclient.h>
#include <tunel/common/netprotocol.h>
#include "game.h"
#include <SDL.h>

#define PORT 13338

#if 0
int main (int argc, char **argv)
{
    NetClient client;
    char ip[128] = {0};
    in_addr_t server_address;
    int server_port;

    SCE_Init_Core (stderr, 0);

    NetClient_Init (&client);
    sprintf (ip, "127.0.0.1:%d", PORT);

    Socket_GetAddressAndPortFromStringv (ip, &server_address, &server_port);
    if (NetClient_Connect (&client, server_address, server_port) < 0) {
        SCEE_LogSrc ();
        SCEE_Out ();
        SCEE_SendMsg ("ONOES\n");
        return SCE_ERROR;
    }

#define DELAY 5
    NetClient_SendTCPString (&client, TLP_CONNECT, "yno");
    if (NetClient_WaitTCPPacket (&client, TLP_CONNECT_ACCEPTED, DELAY) < 0) {
        SCEE_SendMsg ("ONOES\n");
        return 1;
    }

    printf ("connexion accepted lol\n");

    NetClient_SendTCP (&client, TLP_DISCONNECT, NULL, 0);
    NetClient_Disconnect (&client);

    return 0;
}

#else

int main (int argc, char **argv)
{
    GameConfig config;
    Game *game = NULL;

    SCE_Init_Core (stderr, 0);
    Init_Game ();

    if (!(game = Game_New ()))
        goto fail;

    Game_InitSubsystem (game);
    Game_InitConfig (&config);

    if (argv[1])
        strcpy (game->self.nick, argv[1]);
    else
        strcpy (game->self.nick, "Fooo");
    sprintf (game->server_ip, "127.0.0.1:%d", PORT);

    SDL_Delay (100);
    if (Game_Launch (game) < 0)
        goto fail;
    Game_Free (game);
    SCEE_SendMsg ("exiting program code 0\n");

    SCE_Quit_Core ();

    return 0;
fail:
    SCEE_LogSrc ();
    SCEE_Out ();
    SCEE_Clear ();
    Game_Free (game);
    SCEE_SendMsg ("exiting program with an error\n");
    SCE_Quit_Core ();
    return EXIT_FAILURE;
}
#endif
