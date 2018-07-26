//
// Created by bant on 7/26/18.
//

#include "dump1090.h"

int writeJsonToMongo(const char *json) {
    const char *uri_string = Modes.mongo_uri;
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *collection;
    bson_error_t error;

    /*
     * Required to initialize libmongoc's internals
     */
    mongoc_init();

    /*
     * Safely create a MongoDB URI object from the given string
     */
    uri = mongoc_uri_new_with_error(uri_string, &error);
    if (!uri) {
        fprintf(stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
        return EXIT_FAILURE;
    }

    /*
     * Create a new client instance
     */
    client = mongoc_client_new_from_uri(uri);
    if (!client) {
        return EXIT_FAILURE;
    }

    /*
     * Register the application name so we can track it in the profile logs
     * on the server. This can also be done from the URI (see other examples).
     */
    mongoc_client_set_appname(client, "dump1090_conn");

    /*
     * Get a handle on the database "db_name" and collection "coll_name"
     */
    database = mongoc_client_get_database(client, "db_name");
    collection = mongoc_client_get_collection(client, "db_name", "coll_name");

    /*
     * Parse JSON into BSON
     */
    bson_t *bson;

    bson = bson_new_from_json((const uint8_t *) json, -1, &error);

    if (!bson) {
        fprintf(stderr, "%s\n", error.message);
        return EXIT_FAILURE;
    }

    if (!mongoc_collection_insert_one(collection, bson, NULL, NULL, &error)) {
        fprintf(stderr, "%s\n", error.message);
    }

    bson_destroy(bson);

    /*
     * Release our handles and clean up libmongoc
     */
    mongoc_collection_destroy(collection);
    mongoc_database_destroy(database);
    mongoc_uri_destroy(uri);
    mongoc_client_destroy(client);
    mongoc_cleanup();

    return EXIT_SUCCESS;
}