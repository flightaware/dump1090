#include "mongoc/mongoc.h"
#include "mongoc/mongoc-read-concern-private.h"
#include "mongoc/mongoc-util-private.h"

#include "json-test.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"


static mongoc_uri_t *
_get_test_uri (void)
{
   mongoc_uri_t *uri;

   /* Use a URI with retryWrites off */
   uri = test_framework_get_uri ();
   mongoc_uri_set_option_as_bool (uri, "retryWrites", false);

   return uri;
}

static void
_setup_test_with_client (mongoc_client_t *client)
{
   mongoc_write_concern_t *wc;
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   bson_error_t error;
   bson_t *opts;

   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, -1);
   opts = bson_new ();
   ASSERT (mongoc_write_concern_append (wc, opts));

   /* Drop the "step-down.step-down" collection and re-create it */
   coll = mongoc_client_get_collection (client, "step-down", "step-down");
   if (!mongoc_collection_drop (coll, &error)) {
      if (strcmp (error.message, "ns not found")) {
         ASSERT_OR_PRINT (false, error);
      }
   }

   db = mongoc_client_get_database (client, "step-down");
   mongoc_collection_destroy (coll);
   coll = mongoc_database_create_collection (db, "step-down", opts, &error);
   ASSERT_OR_PRINT (coll, error);

   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_write_concern_destroy (wc);
   bson_destroy (opts);
}

static int
_connection_count (mongoc_database_t *db)
{
   mongoc_read_prefs_t *read_prefs;
   bson_error_t error;
   bson_iter_t iter;
   bson_iter_t child;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bool res;
   int conns;

   ASSERT (db);

   BSON_APPEND_INT32 (&cmd, "serverStatus", 1);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   res = mongoc_database_command_simple (db, &cmd, read_prefs, &reply, &error);
   ASSERT_OR_PRINT (res, error);

   ASSERT (bson_iter_init (&iter, &reply));
   ASSERT (
      bson_iter_find_descendant (&iter, "connections.totalCreated", &child));
   conns = bson_iter_int32 (&child);

   bson_destroy (&cmd);
   bson_destroy (&reply);
   mongoc_read_prefs_destroy (read_prefs);

   return conns;
}

typedef void (*_test_fn_t) (mongoc_client_t *);

static void
_run_test_single_and_pooled (_test_fn_t test)
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;

   uri = _get_test_uri ();

   /* Run in single-threaded mode */
   client = mongoc_client_new_from_uri (uri);
   _setup_test_with_client (client);
   test (client);
   mongoc_client_destroy (client);

   /* Run in pooled mode */
   pool = mongoc_client_pool_new (uri);
   client = mongoc_client_pool_pop (pool);
   _setup_test_with_client (client);
   test (client);
   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);

   mongoc_uri_destroy (uri);
}

static void
test_getmore_iteration (mongoc_client_t *client)
{
   mongoc_write_concern_t *wc;
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   bson_t *insert;
   bson_t *opts;
   bool res;
   int conn_count;
   int i;

   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, -1);
   opts = bson_new ();
   ASSERT (mongoc_write_concern_append (wc, opts));

   coll = mongoc_client_get_collection (client, "step-down", "step-down");

   db = mongoc_client_get_database (client, "admin");
   conn_count = _connection_count (db);

   /* Insert 5 documents */
   for (i = 0; i < 5; i++) {
      insert = bson_new ();

      bson_append_int32 (insert, "a", -1, i);
      ASSERT (mongoc_collection_insert_one (coll, insert, opts, NULL, NULL));

      bson_destroy (insert);
   }

   /* Retrieve the first batch of 2 documents */
   cursor =
      mongoc_collection_find_with_opts (coll, tmp_bson ("{}"), NULL, NULL);

   ASSERT (cursor);
   ASSERT (mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_next (cursor, &doc));

   /* Send a stepdown to the primary, ensure it succeeds */
   res = mongoc_database_command_simple (
      db,
      tmp_bson ("{ 'replSetStepDown': 5, 'force': true}"),
      NULL,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   /* Retrieve the next results from the cursor,
      ensure it succeeds */
   ASSERT (mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_next (cursor, &doc));

   /* Verify that no new connections have been created */
   ASSERT (conn_count == _connection_count (db));

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_write_concern_destroy (wc);
   bson_destroy (opts);
}

static void
test_getmore_iteration_runner (void *ctx)
{
   /* Only run on 4.2 or higher */
   if (!test_framework_max_wire_version_at_least (8)) {
      return;
   }

   _run_test_single_and_pooled (test_getmore_iteration);
}

static void
test_not_master_keep_pool (mongoc_client_t *client)
{
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   bson_error_t error;
   bool res;
   int conn_count;

   /* Configure fail points */
   db = mongoc_client_get_database (client, "admin");
   conn_count = _connection_count (db);
   res = mongoc_database_command_simple (
      db,
      tmp_bson ("{'configureFailPoint': 'failCommand', "
                "'mode': {'times': 1}, "
                "'data': {'failCommands': ['insert'], 'errorCode': 10107}}"),
      NULL,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   /* Capture logs to swallow warnings about endSessions */
   capture_logs (true);

   coll = mongoc_client_get_collection (client, "step-down", "step-down");

   /* Execute an insert, verify that it fails with 10107 */
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'test': 1}"), NULL, NULL, &error);
   ASSERT (!res);
   ASSERT (error.code == 10107);
   ASSERT_CONTAINS (error.message,
                    "Failing command due to 'failCommand' failpoint");

   /* Execute a second insert, verify that it succeeds */
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'test': 1}"), NULL, NULL, &error);
   ASSERT (res);

   /* Verify that the connection pool has not been cleared */
   ASSERT (conn_count == _connection_count (db));

   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
}

static void
test_not_master_keep_pool_runner (void *ctx)
{   
   /* Only run on 4.2 and higher */
   if (!test_framework_max_wire_version_at_least (8)) {
      return;
   }

   _run_test_single_and_pooled (test_not_master_keep_pool);
}

static void
test_not_master_reset_pool (mongoc_client_t *client)
{
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   mongoc_read_prefs_t *read_prefs;
   bson_error_t error;
   bool res;
   int conn_count;

   /* Configure fail points */
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   db = mongoc_client_get_database (client, "admin");
   conn_count = _connection_count (db);
   res = mongoc_database_command_simple (
      db,
      tmp_bson ("{'configureFailPoint': 'failCommand', "
                "'mode': {'times': 1}, "
                "'data': {'failCommands': ['insert'], 'errorCode': 10107}}"),
      read_prefs,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   /* Capture logs to swallow warnings about endSessions */
   capture_logs (true);

   coll = mongoc_client_get_collection (client, "step-down", "step-down");

   /* Execute an insert, verify that it fails with 10107 */
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'test': 1}"), NULL, NULL, &error);
   ASSERT (!res);
   ASSERT (error.code == 10107);
   ASSERT_CONTAINS (error.message,
                    "Failing command due to 'failCommand' failpoint");

   /* Verify that the pool has been cleared */
   ASSERT ((conn_count + 1) == _connection_count (db));

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
}

static void
test_not_master_reset_pool_runner (void *ctx)
{
   int64_t max_wire_version;

   /* Only run if version 4.0 */
   test_framework_get_max_wire_version (&max_wire_version);
   if (max_wire_version != 7) {
      return;
   }

   _run_test_single_and_pooled (test_not_master_reset_pool);
}

static void
test_shutdown_reset_pool (mongoc_client_t *client)
{
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   mongoc_read_prefs_t *read_prefs;
   bson_error_t error;
   bool res;
   int conn_count;

   /* Configure fail points */
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   db = mongoc_client_get_database (client, "admin");
   conn_count = _connection_count (db);
   res = mongoc_database_command_simple (
      db,
      tmp_bson ("{'configureFailPoint': 'failCommand', "
                "'mode': {'times': 1}, "
                "'data': {'failCommands': ['insert'], 'errorCode': 91}}"),
      read_prefs,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   coll = mongoc_client_get_collection (client, "step-down", "step-down");

   /* Execute an insert, verify that it fails with 91 */
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'test': 1}"), NULL, NULL, &error);
   ASSERT (!res);
   ASSERT (error.code == 91);
   ASSERT_CONTAINS (error.message,
                    "Failing command due to 'failCommand' failpoint");

   /* Verify that the pool has been cleared */
   ASSERT ((conn_count + 1) == _connection_count (db));

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
}

static void
test_shutdown_reset_pool_runner (void *ctx)
{   
   int64_t max_wire_version;

   /* Only run if version >= 4.0 */
   test_framework_get_max_wire_version (&max_wire_version);
   if (max_wire_version < 7) {
      return;
   }

   _run_test_single_and_pooled (test_shutdown_reset_pool);
}

static void
test_interrupted_shutdown_reset_pool (mongoc_client_t *client)
{
   mongoc_database_t *db;
   mongoc_collection_t *coll;
   mongoc_read_prefs_t *read_prefs;
   bson_error_t error;
   bool res;
   int conn_count;

   /* Configure fail points */
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   db = mongoc_client_get_database (client, "admin");
   conn_count = _connection_count (db);
   res = mongoc_database_command_simple (
      db,
      tmp_bson ("{'configureFailPoint': 'failCommand', "
                "'mode': {'times': 1}, "
                "'data': {'failCommands': ['insert'], 'errorCode': 11600}}"),
      read_prefs,
      NULL,
      &error);
   ASSERT_OR_PRINT (res, error);

   coll = mongoc_client_get_collection (client, "step-down", "step-down");

   /* Execute an insert, verify that it fails with 11600 */
   res = mongoc_collection_insert_one (
      coll, tmp_bson ("{'test': 1}"), NULL, NULL, &error);
   ASSERT (!res);
   ASSERT (error.code == 11600);
   ASSERT_CONTAINS (error.message,
                    "Failing command due to 'failCommand' failpoint");

   /* Verify that the pool has been cleared */
   ASSERT ((conn_count + 1) == _connection_count (db));

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
}

static void
test_interrupted_shutdown_reset_pool_runner (void *ctx)
{
   int64_t max_wire_version;

   /* Only run if version >= 4.0 */
   test_framework_get_max_wire_version (&max_wire_version);
   if (max_wire_version < 7) {
      return;
   }

   _run_test_single_and_pooled (test_interrupted_shutdown_reset_pool);
}

void
test_primary_stepdown_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/Stepdown/getmore",
                      test_getmore_iteration_runner,
                      NULL,
                      NULL,
                      test_framework_skip_if_auth,
                      test_framework_skip_if_not_replset);

   TestSuite_AddFull (suite,
                      "/Stepdown/not_master_keep",
                      test_not_master_keep_pool_runner,
                      NULL,
                      NULL,
                      test_framework_skip_if_auth,
                      test_framework_skip_if_not_replset);

   TestSuite_AddFull (suite,
                      "/Stepdown/not_master_reset",
                      test_not_master_reset_pool_runner,
                      NULL,
                      NULL,
                      test_framework_skip_if_auth,
                      test_framework_skip_if_not_replset);

   TestSuite_AddFull (suite,
                      "/Stepdown/shutdown_reset_pool",
                      test_shutdown_reset_pool_runner,
                      NULL,
                      NULL,
                      test_framework_skip_if_auth,
                      test_framework_skip_if_not_replset);

   TestSuite_AddFull (suite,
                      "/Stepdown/interrupt_shutdown",
                      test_interrupted_shutdown_reset_pool_runner,
                      NULL,
                      NULL,
                      test_framework_skip_if_auth,
                      test_framework_skip_if_not_replset);
}
