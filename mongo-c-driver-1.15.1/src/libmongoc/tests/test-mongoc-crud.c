#include <mongoc/mongoc.h>

#include "json-test.h"
#include "json-test-operations.h"
#include "test-libmongoc.h"

static bool
crud_test_operation_cb (json_test_ctx_t *ctx,
                        const bson_t *test,
                        const bson_t *operation)
{
   bson_t reply;
   bool res;

   res =
      json_test_operation (ctx, test, operation, ctx->collection, NULL, &reply);

   bson_destroy (&reply);

   return res;
}

static void
test_crud_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;
   config.run_operation_cb = crud_test_operation_cb;
   config.command_started_events_only = true;
   config.scenario = scenario;
   run_json_general_test (&config);
}

static void
test_all_spec_tests (TestSuite *suite)
{
   char resolved[PATH_MAX];

   test_framework_resolve_path (JSON_DIR "/crud", resolved);

   install_json_test_suite_with_check (suite,
                                       resolved,
                                       &test_crud_cb,
                                       test_framework_skip_if_no_sessions);
}

void
test_crud_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
}
