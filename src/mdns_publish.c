/*
Copyright (c) 2019, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/***
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include "mdns_publish.h"

#include <assert.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/timeval.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct _MdnsClientInfo {
    AvahiClient *client;
    AvahiEntryGroup *group;
    AvahiSimplePoll *simple_poll;
    char *name;
    char *service_name;
    char *ws_url_path;
    char *device_id;
    int port_number;
} MdnsClientInfo;

// mDNS Client info
// It is used by the mDNS Avahi client,
// it must be initialized at the beginning of the program,
// and malloc only once during the process operation.
static MdnsClientInfo *_mdnsInfo = NULL;

// forward declaration
static void mdns_create_service(MdnsClientInfo *mi);

static void mdns_entry_group_callback(AvahiEntryGroup *g,
                                      AvahiEntryGroupState state,
                                      void *userdata) {
    MdnsClientInfo *mi = (MdnsClientInfo *)userdata;

    assert(g == mi->group || mi->group == NULL);

    /* Called whenever the entry group state changes */
    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            /* The entry group has been established successfully */
            fprintf(stderr, "mDNS: Service '%s' successfully established.\n",
                    mi->name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION: {
            char *n;

            /* A service name collision with a remote service
             * happened. Let's pick a new name */
            n = avahi_alternative_service_name(mi->name);
            avahi_free(mi->name);
            mi->name = n;

            fprintf(stderr,
                    "mDNS: Service name collision, renaming service to '%s'\n",
                    mi->name);

            /* And recreate the services */
            mi->client = (avahi_entry_group_get_client(g));
            mdns_create_service(mi);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE:
            fprintf(stderr, "mDNS: Entry group failure: %s\n",
                    avahi_strerror(
                        avahi_client_errno(avahi_entry_group_get_client(g))));

            /* Some kind of failure happened while we were registering our
             * services */
            avahi_simple_poll_quit(mi->simple_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
    }
}

static void mdns_create_service(MdnsClientInfo *mi) {
    assert(mi->client);
    int ret;
    char *n;

    /* If this is the first time we're called, let's create a new
     * entry group if necessary */

    if (!mi->group) {
        if (!(mi->group = avahi_entry_group_new(
                  mi->client, mdns_entry_group_callback, mi))) {
            fprintf(stderr, "mDNS: avahi_entry_group_new() failed: %s\n",
                    avahi_strerror(avahi_client_errno(mi->client)));
            avahi_simple_poll_quit(mi->simple_poll);
        }
    }

    /* If the group is empty (either because it was just created, or
     * because it was reset previously, add our entries.  */

    if (avahi_entry_group_is_empty(mi->group)) {
        fprintf(stderr, "mDNS: Adding service '%s'\n", mi->name);

        /* Add the service for IPP */
        if ((ret = avahi_entry_group_add_service(
                 mi->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, mi->name,
                 mi->service_name, NULL, NULL, mi->port_number, mi->ws_url_path,
                 mi->device_id, NULL)) < 0) {
            if (ret == AVAHI_ERR_COLLISION) {
                /* A service name collision with a local service happened. Let's
                 * pick a new name */
                n = avahi_alternative_service_name(mi->name);
                avahi_free(mi->name);
                mi->name = n;

                fprintf(
                    stderr,
                    "mDNS: Service name collision, renaming service to '%s'\n",
                    mi->name);
                avahi_entry_group_reset(mi->group);
                mdns_create_service(mi);
                return;
            }

            fprintf(stderr, "mDNS: Failed to add _wstreamer._tcp service: %s\n",
                    avahi_strerror(ret));
            avahi_simple_poll_quit(mi->simple_poll);
        }

        /* Tell the server to register the service */
        if ((ret = avahi_entry_group_commit(mi->group)) < 0) {
            fprintf(stderr, "mDNS: Failed to commit entry group: %s\n",
                    avahi_strerror(ret));
            avahi_simple_poll_quit(mi->simple_poll);
        }
    }
    return;
}

static void mdns_client_callback(AvahiClient *c, AvahiClientState state,
                                 void *userdata) {
    MdnsClientInfo *mi;
    assert(c);

    mi = (MdnsClientInfo *)userdata;
    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            mi->client = c;
            mdns_create_service(mi);
            break;

        case AVAHI_CLIENT_FAILURE:
            fprintf(stderr, "mDNS: Client failure: %s\n",
                    avahi_strerror(avahi_client_errno(c)));
            avahi_simple_poll_quit(mi->simple_poll);

            break;

        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */
            if (mi->group) avahi_entry_group_reset(mi->group);
            break;

        case AVAHI_CLIENT_CONNECTING:
            break;
    }
}

int mdns_init_clientinfo(int port_number, const char *ws,
                         const char *device_id) {
    int error = 0;
    char strbuf[128];

    if (_mdnsInfo != NULL) {
        fprintf(stderr, "mDNS: Warning, client info already initialized.\n");
        return MDNS_SUCCESS;
    }

    if ((_mdnsInfo = avahi_malloc(sizeof(MdnsClientInfo))) == NULL) {
        fprintf(stderr, "mDNS: Failed to allocate mDNS client info .\n");
        return MDNS_FAILED;
    }

    memset((char *)_mdnsInfo, 0x00, sizeof(MdnsClientInfo));

    // initlaize client info with some default values
    _mdnsInfo->name = avahi_strdup("WebRTC Streamer");
    _mdnsInfo->service_name = avahi_strdup("_wstreamer._tcp");

    // initialize client info with  parameter values
    _mdnsInfo->port_number = port_number;
    snprintf(strbuf, sizeof(strbuf), "ws_url_path=%s", ws);
    _mdnsInfo->ws_url_path = avahi_strdup(strbuf);
    snprintf(strbuf, sizeof(strbuf), "deviceid=%s", device_id);
    _mdnsInfo->device_id = avahi_strdup(strbuf);

    /* Allocate main loop object */
    if (!(_mdnsInfo->simple_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "mDNS: Failed to create simple poll object.\n");
        mdns_destroy_clientinfo();
        return MDNS_FAILED;
    }

    /* Allocate a new client */
    _mdnsInfo->client =
        avahi_client_new(avahi_simple_poll_get(_mdnsInfo->simple_poll), 0,
                         mdns_client_callback, _mdnsInfo, &error);

    /* Check wether creating the client object succeeded */
    if (_mdnsInfo->client == NULL) {
        fprintf(stderr, "mDNS: Failed to create client: %s\n",
                avahi_strerror(error));
        mdns_destroy_clientinfo();
        return MDNS_FAILED;
    }

    return MDNS_SUCCESS;
}

void mdns_destroy_clientinfo(void) {
    if (_mdnsInfo != NULL) {
        if (_mdnsInfo->client) avahi_client_free(_mdnsInfo->client);
        if (_mdnsInfo->group) avahi_entry_group_free(_mdnsInfo->group);
        if (_mdnsInfo->simple_poll)
            avahi_simple_poll_free(_mdnsInfo->simple_poll);

        if (_mdnsInfo->name) avahi_free(_mdnsInfo->name);
        if (_mdnsInfo->service_name) avahi_free(_mdnsInfo->service_name);
        if (_mdnsInfo->ws_url_path) avahi_free(_mdnsInfo->ws_url_path);
        if (_mdnsInfo->device_id) avahi_free(_mdnsInfo->device_id);

        avahi_free(_mdnsInfo);
        _mdnsInfo = NULL;
        return;
    }
    fprintf(stderr,
            "mDNS: Error, Trying to destroy mDNS client info without "
            "initialization.\n");
}

// timeout unit is ms.
int mdns_run_loop(int timeout) {
    int retvalue;
    assert(_mdnsInfo);
    assert(_mdnsInfo->client);
    assert(_mdnsInfo->simple_poll);

    if (timeout == 0) timeout = 5;
    if ((retvalue =
             avahi_simple_poll_iterate(_mdnsInfo->simple_poll, timeout)) != 0) {
        if (retvalue >= 0 || errno != EINTR)
            fprintf(stderr, "mDNS: simple poll returned non-zero value: %d\n",
                    retvalue);
        return MDNS_FAILED;
    }
    return MDNS_SUCCESS;
}
