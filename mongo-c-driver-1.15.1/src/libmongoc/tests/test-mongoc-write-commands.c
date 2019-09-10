#include <bson/bcon.h>
#include <mongoc/mongoc.h>

#include "mongoc/mongoc-client-private.h"
#include "mongoc/mongoc-collection-private.h"
#include "mongoc/mongoc-write-command-private.h"
#include "mongoc/mongoc-write-concern-private.h"

#include "TestSuite.h"

#include "test-libmongoc.h"
#include "test-conveniences.h"
#include "mock_server/mock-server.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"


static void
test_split_insert (void)
{
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_oid_t oid;
   bson_t **docs;
   bson_t reply = BSON_INITIALIZER;
   bson_error_t error;
   mongoc_server_stream_t *server_stream;
   int i;
   bool r;

   client = test_framework_client_new ();
   BSON_ASSERT (client);

   collection = get_test_collection (client, "test_split_insert");
   BSON_ASSERT (collection);

   docs = (bson_t **) bson_malloc (sizeof (bson_t *) * 3000);

   for (i = 0; i < 3000; i++) {
      docs[i] = bson_new ();
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (docs[i], "_id", &oid);
   }

   _mongoc_write_result_init (&result);

   _mongoc_write_command_init_insert (&command,
                                      docs[0],
                                      NULL,
                                      write_flags,
                                      ++client->cluster.operation_id,
                                      true);

   for (i = 1; i < 3000; i++) {
      _mongoc_write_command_insert_append (&command, docs[i]);
   }

   server_stream =
      mongoc_cluster_stream_for_writes (&client->cluster, NULL, NULL, &error);
   ASSERT_OR_PRINT (server_stream, error);
   _mongoc_write_command_execute (&command,
                                  client,
                                  server_stream,
                                  collection->db,
                                  collection->collection,
                                  NULL,
                                  0,
                                  NULL,
                                  &result);

   r = MONGOC_WRITE_RESULT_COMPLETE (&result,
                                     2,
                                     collection->write_concern,
                                     (mongoc_error_domain_t) 0,
                                     &reply,
                                     &error);
   ASSERT_OR_PRINT (r, error);
   BSON_ASSERT (result.nInserted == 3000);

   _mongoc_write_command_destroy (&command);
   _mongoc_write_result_destroy (&result);

   ASSERT_OR_PRINT (mongoc_collection_drop (collection, &error), error);

   for (i = 0; i < 3000; i++) {
      bson_destroy (docs[i]);
   }

   bson_destroy (&reply);
   bson_free (docs);
   mongoc_server_stream_cleanup (server_stream);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
test_invalid_write_concern (void)
{
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_write_concern_t *write_concern;
   mongoc_server_stream_t *server_stream;
   bson_t *doc;
   bson_t reply = BSON_INITIALIZER;
   bson_error_t error;
   bool r;

   client = test_framework_client_new ();
   BSON_ASSERT (client);

   collection = get_test_collection (client, "test_invalid_write_concern");
   BSON_ASSERT (collection);

   write_concern = mongoc_write_concern_new ();
   BSON_ASSERT (write_concern);
   mongoc_write_concern_set_w (write_concern, 0);
   mongoc_write_concern_set_journal (write_concern, true);
   BSON_ASSERT (!mongoc_write_concern_is_valid (write_concern));

   doc = BCON_NEW ("_id", BCON_INT32 (0));

   _mongoc_write_command_init_insert (
      &command, doc, NULL, write_flags, ++client->cluster.operation_id, true);
   _mongoc_write_result_init (&result);
   server_stream =
      mongoc_cluster_stream_for_writes (&client->cluster, NULL, NULL, &error);
   ASSERT_OR_PRINT (server_stream, error);
   _mongoc_write_command_execute (&command,
                                  client,
                                  server_stream,
                                  collection->db,
                                  collection->collection,
                                  write_concern,
                                  0,
                                  NULL,
                                  &result);

   r = MONGOC_WRITE_RESULT_COMPLETE (&result,
                                     2,
                                     collection->write_concern,
                                     (mongoc_error_domain_t) 0,
                                     &reply,
                                     &error);

   BSON_ASSERT (!r);
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_COMMAND);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_COMMAND_INVALID_ARG);

   _mongoc_write_command_destroy (&command);
   _mongoc_write_result_destroy (&result);

   bson_destroy (doc);
   bson_destroy (&reply);
   mongoc_server_stream_cleanup (server_stream);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mongoc_write_concern_destroy (write_concern);
}

static void
test_bypass_validation (void *context)
{
   mongoc_collection_t *collection2;
   mongoc_collection_t *collection;
   bson_t reply;
   mongoc_bulk_operation_t *bulk;
   mongoc_database_t *database;
   mongoc_write_concern_t *wr;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t *options;
   char *collname;
   char *dbname;
   int r;
   int i;

   client = test_framework_client_new ();
   BSON_ASSERT (client);

   dbname = gen_collection_name ("dbtest");
   collname = gen_collection_name ("bypass");
   database = mongoc_client_get_database (client, dbname);
   collection = mongoc_database_get_collection (database, collname);
   BSON_ASSERT (collection);

   options = tmp_bson (
      "{'validator': {'number': {'$gte': 5}}, 'validationAction': 'error'}");
   collection2 =
      mongoc_database_create_collection (database, collname, options, &error);
   ASSERT_OR_PRINT (collection2, error);
   mongoc_collection_destroy (collection2);

   /* {{{ Default fails validation */
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, NULL);
   for (i = 0; i < 3; i++) {
      bson_t *doc = tmp_bson ("{'number': 3, 'high': %d }", i);

      mongoc_bulk_operation_insert (bulk, doc);
   }
   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   bson_destroy (&reply);
   ASSERT (!r);

   ASSERT_ERROR_CONTAINS (
      error, MONGOC_ERROR_COMMAND, 121, "Document failed validation");
   mongoc_bulk_operation_destroy (bulk);
   /* }}} */

   /* {{{ bypass_document_validation=false Fails validation */
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, NULL);
   mongoc_bulk_operation_set_bypass_document_validation (bulk, false);
   for (i = 0; i < 3; i++) {
      bson_t *doc = tmp_bson ("{'number': 3, 'high': %d }", i);

      mongoc_bulk_operation_insert (bulk, doc);
   }
   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   bson_destroy (&reply);
   ASSERT (!r);

   ASSERT_ERROR_CONTAINS (
      error, MONGOC_ERROR_COMMAND, 121, "Document failed validation");
   mongoc_bulk_operation_destroy (bulk);
   /* }}} */

   /* {{{ bypass_document_validation=true ignores validation */
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, NULL);
   mongoc_bulk_operation_set_bypass_document_validation (bulk, true);
   for (i = 0; i < 3; i++) {
      bson_t *doc = tmp_bson ("{'number': 3, 'high': %d }", i);

      mongoc_bulk_operation_insert (bulk, doc);
   }
   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   bson_destroy (&reply);
   ASSERT_OR_PRINT (r, error);
   mongoc_bulk_operation_destroy (bulk);
   /* }}} */

   /* {{{ w=0 and bypass_document_validation=set fails */
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, NULL);
   wr = mongoc_write_concern_new ();
   mongoc_write_concern_set_w (wr, 0);
   mongoc_bulk_operation_set_write_concern (bulk, wr);
   mongoc_bulk_operation_set_bypass_document_validation (bulk, true);
   for (i = 0; i < 3; i++) {
      bson_t *doc = tmp_bson ("{'number': 3, 'high': %d }", i);

      mongoc_bulk_operation_insert (bulk, doc);
   }
   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   bson_destroy (&reply);
   ASSERT_OR_PRINT (!r, error);
   ASSERT_ERROR_CONTAINS (
      error,
      MONGOC_ERROR_COMMAND,
      MONGOC_ERROR_COMMAND_INVALID_ARG,
      "Cannot set bypassDocumentValidation for unacknowledged writes");
   mongoc_bulk_operation_destroy (bulk);
   mongoc_write_concern_destroy (wr);
   /* }}} */

   ASSERT_OR_PRINT (mongoc_collection_drop (collection, &error), error);

   bson_free (dbname);
   bson_free (collname);
   mongoc_database_destroy (database);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}

static void
test_bypass_command_started (const mongoc_apm_command_started_t *event)
{
   ASSERT_HAS_NOT_FIELD (mongoc_apm_command_started_get_command (event),
                         "bypassDocumentValidation");
}

static void
test_bypass_not_sent ()
{
   mongoc_collection_t *collection;
   mongoc_bulk_operation_t *bulk;
   mongoc_find_and_modify_opts_t *opts;
   mongoc_client_t *client;
   mongoc_database_t *database;
   mongoc_apm_callbacks_t *callbacks;
   bson_error_t error;
   bool r;
   bson_t *doc;
   const bson_t *agg_doc;
   bson_t reply;
   bson_t *update;
   bson_t *query;
   bson_t *pipeline;
   bson_t *agg_opts;
   mongoc_cursor_t *cursor;
   char *collname;
   char *dbname;

   client = test_framework_client_new ();

   /* set up command monitoring for started commands */
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, test_bypass_command_started);
   mongoc_client_set_apm_callbacks (client, callbacks, NULL);
   mongoc_apm_callbacks_destroy (callbacks);

   dbname = "test";
   collname = gen_collection_name ("bypass");
   database = mongoc_client_get_database (client, dbname);
   collection = mongoc_database_get_collection (database, collname);

   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, NULL);

   /* we explicitly set this to false to test that it isn't sent */
   mongoc_bulk_operation_set_bypass_document_validation (bulk, false);

   /* insert a doc */
   doc = BCON_NEW ("x", BCON_INT32 (31));
   mongoc_bulk_operation_insert (bulk, doc);
   bson_destroy (doc);
   r = (bool) mongoc_bulk_operation_execute (bulk, &reply, &error);
   bson_destroy (&reply);
   ASSERT_OR_PRINT (r, error);
   mongoc_bulk_operation_destroy (bulk);

   opts = mongoc_find_and_modify_opts_new ();

   /* we explicitly set this to false to test that it isn't sent */
   mongoc_find_and_modify_opts_set_bypass_document_validation (opts, false);

   /* find the doc we inserted earlier and modify it */
   update = BCON_NEW ("$set", "{", "x", BCON_INT32 (32), "}");
   mongoc_find_and_modify_opts_set_update (opts, update);
   bson_destroy (update);
   query = BCON_NEW ("x", BCON_INT32 (31));
   r = mongoc_collection_find_and_modify_with_opts (
      collection, query, opts, &reply, &error);
   bson_destroy (&reply);
   ASSERT_OR_PRINT (r, error);
   bson_destroy (query);
   mongoc_find_and_modify_opts_destroy (opts);

   /* we explicitly set this to false to test that it isn't sent */
   agg_opts = BCON_NEW ("bypassDocumentValidation", BCON_BOOL (false));

   /* aggregate match */
   pipeline = BCON_NEW ("pipeline", "[", "]");
   cursor = mongoc_collection_aggregate (
      collection, MONGOC_QUERY_NONE, pipeline, agg_opts, NULL);
   bson_destroy (pipeline);
   bson_destroy (agg_opts);

   /* iterate through aggregation results */
   while (mongoc_cursor_next (cursor, &agg_doc)) {
   }

   mongoc_cursor_destroy (cursor);

   /* cleanup */
   bson_free (collname);
   mongoc_collection_destroy (collection);
   mongoc_database_destroy (database);
   mongoc_client_destroy (client);
}

static void
test_split_opquery_with_options (void)
{
   mock_server_t *server;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   bson_t **docs;
   int i;
   bson_error_t error;
   future_t *future;
   request_t *request;
   const bson_t *insert;
   bson_t opts;
   mongoc_write_concern_t *wc;
   int n_docs;

   /* Use a reduced maxBsonObjectSize, and wire version for OP_QUERY */
   const char *ismaster = "{'ok': 1.0,"
                          " 'ismaster': true,"
                          " 'minWireVersion': 0,"
                          " 'maxWireVersion': 5,"
                          " 'maxBsonObjectSize': 100}";

   server = mock_server_new ();
   mock_server_auto_ismaster (server, ismaster);
   mock_server_run (server);

   /* Create an insert with two batches. Because of the reduced
   * maxBsonObjectSize, each document must be less than 100 bytes.
   * Because of the hardcoded allowance (see SERVER-10643), and our current
   * incorrect batching logic (see CDRIVER-3310) the complete insert
   * command can be can be 16k + 100 bytes.
   * After CDRIVER-3310, update this test, since the allowance will not be
   * taken into consideration for document batching.
   * So create enough documents to fill up at least one batch.
   */
   n_docs = ((BSON_OBJECT_ALLOWANCE) / tmp_bson ("{ '_id': 1 }")->len) +
            1; /* inexact, but errs towards more than enough documents. */
   docs = bson_malloc (sizeof (bson_t *) * n_docs);
   for (i = 0; i < n_docs; i++) {
      docs[i] = BCON_NEW ("_id", BCON_INT64 (i));
   }

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   coll = mongoc_client_get_collection (client, "db", "coll");

   /* Add a write concern, to ensure that it is taken into account during
    * splitting. */
   bson_init (&opts);
   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, 100);
   mongoc_write_concern_append (wc, &opts);

   future = future_collection_insert_many (
      coll, (const bson_t **) docs, n_docs, &opts, NULL, &error);
   /* Mock server recieves first insert. */
   request = mock_server_receives_request (server);
   BSON_ASSERT (request);
   insert = request_get_doc (request, 0);
   /* The total command size is just a hair under BSON_OBJECT_ALLOWANCE (16384)
    * + 100 */
   BSON_ASSERT (insert->len == 16482);
   mock_server_replies_ok_and_destroys (request);

   /* Mock server recieves second insert. */
   request = mock_server_receives_request (server);
   BSON_ASSERT (request);
   insert = request_get_doc (request, 0);
   /* The size doesn't really matter for the purpose of the test, but check it
    * anyway. */
   BSON_ASSERT (insert->len == 10433);
   mock_server_replies_ok_and_destroys (request);
   BSON_ASSERT (future_get_bool (future));

   future_destroy (future);
   for (i = 0; i < n_docs; i++) {
      bson_destroy (docs[i]);
   }
   bson_free (docs);
   bson_destroy (&opts);
   mongoc_collection_destroy (coll);
   mongoc_write_concern_destroy (wc);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

static void
test_opmsg_disconnect_mid_batch_helper (int wire_version)
{
   mock_server_t *server;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   bson_t **docs;
   int i;
   bson_error_t error;
   future_t *future;
   request_t *request;
   int n_docs;

   /* Use a reduced maxBsonObjectSize, and wire version for OP_QUERY */
   const char *ismaster = "{'ok': 1.0,"
                          " 'ismaster': true,"
                          " 'minWireVersion': 0,"
                          " 'maxWireVersion': %d,"
                          " 'maxBsonObjectSize': 100}";

   server = mock_server_new ();
   mock_server_auto_ismaster (server, ismaster, wire_version);
   mock_server_run (server);

   /* create enough documents for two batches. Note, because of our wonky
    * batch splitting behavior (to be fixed in CDRIVER-3310) we need add 16K
    * of documents. After CDRIVER-3310, we'll need to update this test. */
   n_docs = ((BSON_OBJECT_ALLOWANCE) / tmp_bson ("{ '_id': 1 }")->len) + 1;
   docs = bson_malloc (sizeof (bson_t *) * n_docs);
   for (i = 0; i < n_docs; i++) {
      docs[i] = BCON_NEW ("_id", BCON_INT64 (i));
   }

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   coll = mongoc_client_get_collection (client, "db", "coll");

   future = future_collection_insert_many (
      coll, (const bson_t **) docs, n_docs, NULL, NULL, &error);
   /* Mock server recieves first insert. */
   request = mock_server_receives_request (server);
   BSON_ASSERT (request);
   mock_server_hangs_up (request);
   request_destroy (request);

   BSON_ASSERT (!future_get_bool (future));
   future_destroy (future);
   ASSERT_ERROR_CONTAINS (
      error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET, "socket error");

   for (i = 0; i < n_docs; i++) {
      bson_destroy (docs[i]);
   }
   bson_free (docs);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}

static void
test_opmsg_disconnect_mid_batch (void)
{
   test_opmsg_disconnect_mid_batch_helper (WIRE_VERSION_OP_MSG);
   test_opmsg_disconnect_mid_batch_helper (WIRE_VERSION_OP_MSG - 1);
}

void
test_write_command_install (TestSuite *suite)
{
   TestSuite_AddLive (suite, "/WriteCommand/split_insert", test_split_insert);
   TestSuite_AddLive (
      suite, "/WriteCommand/bypass_not_sent", test_bypass_not_sent);
   TestSuite_AddLive (
      suite, "/WriteCommand/invalid_write_concern", test_invalid_write_concern);
   TestSuite_AddFull (suite,
                      "/WriteCommand/bypass_validation",
                      test_bypass_validation,
                      NULL,
                      NULL,
                      test_framework_skip_if_max_wire_version_less_than_4);
   TestSuite_AddMockServerTest (suite,
                                "/WriteCommand/split_opquery_with_options",
                                test_split_opquery_with_options);
   TestSuite_AddMockServerTest (suite,
                                "/WriteCommand/insert_disconnect_mid_batch",
                                test_opmsg_disconnect_mid_batch);
}
