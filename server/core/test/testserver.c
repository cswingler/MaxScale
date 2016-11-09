/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl.
 *
 * Change Date: 2019-07-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 *
 * @verbatim
 * Revision History
 *
 * Date         Who                 Description
 * 08-10-2014   Martin Brampton     Initial implementation
 *
 * @endverbatim
 */

// To ensure that ss_info_assert asserts also when builing in non-debug mode.
#if !defined(SS_DEBUG)
#define SS_DEBUG
#endif
#if defined(NDEBUG)
#undef NDEBUG
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <maxscale/alloc.h>
#include <maxscale/server.h>
#include <maxscale/log_manager.h>
#include <maxscale/gwdirs.h>

// This is pretty ugly but it's required to test internal functions
#include "../config.c"

/**
 * test1    Allocate a server and do lots of other things
 *
  */
static int
test1()
{
    SERVER   *server;
    int     result;
    char    *status;

    /* Server tests */
    ss_dfprintf(stderr, "testserver : creating server called MyServer");
    set_libdir(MXS_STRDUP_A("../../modules/authenticator/"));
    server = server_alloc("MyServer", "HTTPD", 9876, "NullAuthAllow", NULL);
    ss_info_dassert(server, "Allocating the server should not fail");
    mxs_log_flush_sync();

    //ss_info_dassert(NULL != service, "New server with valid protocol and port must not be null");
    //ss_info_dassert(0 != service_isvalid(service), "Service must be valid after creation");

    ss_dfprintf(stderr, "\t..done\nTest Parameter for Server.");
    ss_info_dassert(NULL == serverGetParameter(server, "name"), "Parameter should be null when not set");
    serverAddParameter(server, "name", "value");
    mxs_log_flush_sync();
    ss_info_dassert(0 == strcmp("value", serverGetParameter(server, "name")),
                    "Parameter should be returned correctly");
    ss_dfprintf(stderr, "\t..done\nTesting Unique Name for Server.");
    ss_info_dassert(NULL == server_find_by_unique_name("uniquename"),
                    "Should not find non-existent unique name.");
    server_set_unique_name(server, "uniquename");
    mxs_log_flush_sync();
    ss_info_dassert(server == server_find_by_unique_name("uniquename"), "Should find by unique name.");
    ss_dfprintf(stderr, "\t..done\nTesting Status Setting for Server.");
    status = server_status(server);
    mxs_log_flush_sync();
    ss_info_dassert(0 == strcmp("Running", status), "Status of Server should be Running by default.");
    if (NULL != status)
    {
        MXS_FREE(status);
    }
    server_set_status(server, SERVER_MASTER);
    status = server_status(server);
    mxs_log_flush_sync();
    ss_info_dassert(0 == strcmp("Master, Running", status), "Should find correct status.");
    server_clear_status(server, SERVER_MASTER);
    MXS_FREE(status);
    status = server_status(server);
    mxs_log_flush_sync();
    ss_info_dassert(0 == strcmp("Running", status),
                    "Status of Server should be Running after master status cleared.");
    if (NULL != status)
    {
        MXS_FREE(status);
    }
    ss_dfprintf(stderr, "\t..done\nRun Prints for Server and all Servers.");
    printServer(server);
    printAllServers();
    mxs_log_flush_sync();
    ss_dfprintf(stderr, "\t..done\nFreeing Server.");
    ss_info_dassert(0 != server_free(server), "Free should succeed");
    ss_dfprintf(stderr, "\t..done\n");
    return 0;

}

#define TEST(A, B) do { if(!(A)){ printf(B"\n"); return false; }} while(false)

bool test_load_config(const char *input, SERVER *server)
{
    DUPLICATE_CONTEXT dcontext;

    if (duplicate_context_init(&dcontext))
    {
        CONFIG_CONTEXT ccontext = {.object = ""};

        if (config_load_single_file(input, &dcontext, &ccontext))
        {
            CONFIG_CONTEXT *obj = ccontext.next;
            CONFIG_PARAMETER *param = obj->parameters;

            TEST(strcmp(obj->object, server->unique_name) == 0, "Server names differ");
            TEST(strcmp(server->name, config_get_param(param, "address")->value) == 0, "Server addresses differ");
            TEST(strcmp(server->protocol, config_get_param(param, "protocol")->value) == 0, "Server protocols differ");
            TEST(strcmp(server->authenticator, config_get_param(param, "authenticator")->value) == 0,
                 "Server authenticators differ");
            TEST(strcmp(server->auth_options, config_get_param(param, "authenticator_options")->value) == 0,
                 "Server authenticator options differ");
            TEST(server->port == atoi(config_get_param(param, "port")->value), "Server ports differ");
            TEST(create_new_server(obj) == 0, "Failed to create server from loaded config");
        }
    }

    return true;
}

bool test_serialize()
{
    char name[] = "serialized-server";
    SERVER *server = server_alloc("127.0.0.1", "HTTPD", 9876, "NullAuthAllow", "fake=option");
    TEST(server, "Server allocation failed");
    server_set_unique_name(server, name);

    /** Make sure the file doesn't exist */
    unlink("./server.cnf");

    /** Serialize server to disk */
    TEST(server_serialize(server, "./server.cnf"), "Failed to synchronize original server");

    /** Load it again */
    TEST(test_load_config("./server.cnf", server), "Failed to load the serialized server");

    /** We should have two identical servers */
    SERVER *created = server_find_by_unique_name(name);
    TEST(created->next == server, "We should end up with two servers");

    /** Make sure the file doesn't exist */
    unlink("./server-created.cnf");

    /** Serialize the loaded server to disk */
    TEST(server_serialize(created, "./server-created.cnf"), "Failed to synchronize the copied server");

    /** Check that they serialize to identical files */
    TEST(system("diff ./server.cnf ./server-created.cnf") == 0, "The files are not identical");

    return true;
}

int main(int argc, char **argv)
{
    int result = 0;

    result += test1();

    if (!test_serialize())
    {
        result++;
    }

    exit(result);
}
