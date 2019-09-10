/*
 * Copyright 2015-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <mongoc/mongoc.h>
#include "mongoc/mongoc-collection-private.h"
#include "mongoc/mongoc-util-private.h"
#include "mongoc/mongoc-uri-private.h"

#include "json-test.h"
#include "json-test-operations.h"
#include "json-test-monitoring.h"
#include "TestSuite.h"
#include "test-libmongoc.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#endif


mongoc_topology_description_type_t
topology_type_from_test (const char *type)
{
   if (strcmp (type, "ReplicaSetWithPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_WITH_PRIMARY;
   } else if (strcmp (type, "ReplicaSetNoPrimary") == 0) {
      return MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else if (strcmp (type, "Unknown") == 0) {
      return MONGOC_TOPOLOGY_UNKNOWN;
   } else if (strcmp (type, "Single") == 0) {
      return MONGOC_TOPOLOGY_SINGLE;
   } else if (strcmp (type, "Sharded") == 0) {
      return MONGOC_TOPOLOGY_SHARDED;
   }

   fprintf (stderr, "can't parse this: %s", type);
   BSON_ASSERT (0);
   return 0;
}

mongoc_server_description_type_t
server_type_from_test (const char *type)
{
   if (strcmp (type, "RSPrimary") == 0) {
      return MONGOC_SERVER_RS_PRIMARY;
   } else if (strcmp (type, "RSSecondary") == 0) {
      return MONGOC_SERVER_RS_SECONDARY;
   } else if (strcmp (type, "Standalone") == 0) {
      return MONGOC_SERVER_STANDALONE;
   } else if (strcmp (type, "Mongos") == 0) {
      return MONGOC_SERVER_MONGOS;
   } else if (strcmp (type, "PossiblePrimary") == 0) {
      return MONGOC_SERVER_POSSIBLE_PRIMARY;
   } else if (strcmp (type, "RSArbiter") == 0) {
      return MONGOC_SERVER_RS_ARBITER;
   } else if (strcmp (type, "RSOther") == 0) {
      return MONGOC_SERVER_RS_OTHER;
   } else if (strcmp (type, "RSGhost") == 0) {
      return MONGOC_SERVER_RS_GHOST;
   } else if (strcmp (type, "Unknown") == 0) {
      return MONGOC_SERVER_UNKNOWN;
   }
   fprintf (stderr, "ERROR: Unknown server type %s\n", type);
   BSON_ASSERT (0);
   return 0;
}


static mongoc_read_mode_t
read_mode_from_test (const char *mode)
{
   if (bson_strcasecmp (mode, "Primary") == 0) {
      return MONGOC_READ_PRIMARY;
   } else if (bson_strcasecmp (mode, "PrimaryPreferred") == 0) {
      return MONGOC_READ_PRIMARY_PREFERRED;
   } else if (bson_strcasecmp (mode, "Secondary") == 0) {
      return MONGOC_READ_SECONDARY;
   } else if (bson_strcasecmp (mode, "SecondaryPreferred") == 0) {
      return MONGOC_READ_SECONDARY_PREFERRED;
   } else if (bson_strcasecmp (mode, "Nearest") == 0) {
      return MONGOC_READ_NEAREST;
   } else {
      test_error ("Unknown read preference mode \"%s\"", mode);
   }

   return MONGOC_READ_PRIMARY;
}


static mongoc_ss_optype_t
optype_from_test (const char *op)
{
   if (strcmp (op, "read") == 0) {
      return MONGOC_SS_READ;
   } else if (strcmp (op, "write") == 0) {
      return MONGOC_SS_WRITE;
   }

   return MONGOC_SS_READ;
}


/*
 *-----------------------------------------------------------------------
 *
 * server_description_by_hostname --
 *
 *      Return a reference to a mongoc_server_description_t or NULL.
 *
 *-----------------------------------------------------------------------
 */
mongoc_server_description_t *
server_description_by_hostname (mongoc_topology_description_t *topology,
                                const char *address)
{
   mongoc_set_t *set = topology->servers;
   mongoc_server_description_t *server_iter;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = (mongoc_server_description_t *) mongoc_set_get_item (
         topology->servers, i);

      if (strcasecmp (address, server_iter->connection_address) == 0) {
         return server_iter;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------
 *
 * process_sdam_test_ismaster_responses --
 *
 *      Update a topology description with the ismaster responses in a "phase"
 *      from an SDAM or SDAM Monitoring test, like:
 *
 *      [
 *          [
 *              "a:27017",
 *              {
 *                  "ok": 1,
 *                  "ismaster": false
 *              }
 *          ]
 *      ]
 *
 * See:
 * https://github.com/mongodb/specifications/tree/master/source/server-discovery-and-monitoring/tests
 *
 *-----------------------------------------------------------------------
 */
void
process_sdam_test_ismaster_responses (bson_t *phase,
                                      mongoc_topology_description_t *td)
{
   mongoc_server_description_t *sd;
   bson_t ismasters;
   bson_t ismaster;
   bson_t response;
   bson_iter_t phase_field_iter;
   bson_iter_t ismaster_iter;
   bson_iter_t ismaster_field_iter;
   const char *hostname;

   /* grab ismaster responses out and feed them to topology */
   BSON_ASSERT (bson_iter_init_find (&phase_field_iter, phase, "responses"));
   bson_iter_bson (&phase_field_iter, &ismasters);
   bson_iter_init (&ismaster_iter, &ismasters);

   while (bson_iter_next (&ismaster_iter)) {
      bson_iter_bson (&ismaster_iter, &ismaster);

      bson_iter_init_find (&ismaster_field_iter, &ismaster, "0");
      hostname = bson_iter_utf8 (&ismaster_field_iter, NULL);
      sd = server_description_by_hostname (td, hostname);

      /* if server has been removed from topology, skip */
      if (!sd) {
         continue;
      }

      bson_iter_init_find (&ismaster_field_iter, &ismaster, "1");
      bson_iter_bson (&ismaster_field_iter, &response);

      /* send ismaster through the topology description's handler */
      capture_logs (true);
      mongoc_topology_description_handle_ismaster (
         td, sd->id, &response, 1, NULL);
      if (td->servers->items_len == 0) {
         ASSERT_CAPTURED_LOG ("topology",
                              MONGOC_LOG_LEVEL_WARNING,
                              "Last server removed from topology");
      }
      capture_logs (false);
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * test_server_selection_logic_cb --
 *
 *      Runs the JSON tests for server selection logic that are
 *      included with the Server Selection spec.
 *
 *-----------------------------------------------------------------------
 */
void
test_server_selection_logic_cb (bson_t *test)
{
   bool expected_error;
   bson_error_t error;
   int32_t heartbeat_msec;
   mongoc_topology_description_t topology;
   mongoc_server_description_t *sd;
   mongoc_read_prefs_t *read_prefs;
   mongoc_read_mode_t read_mode;
   mongoc_ss_optype_t op;
   bson_iter_t iter;
   bson_iter_t topology_iter;
   bson_iter_t server_iter;
   bson_iter_t sd_iter;
   bson_iter_t read_pref_iter;
   bson_iter_t tag_sets_iter;
   bson_iter_t last_write_iter;
   bson_iter_t expected_servers_iter;
   bson_t first_tag_set;
   bson_t test_topology;
   bson_t test_servers;
   bson_t server;
   bson_t test_read_pref;
   bson_t test_tag_sets;
   uint32_t i = 0;
   bool matched_servers[50];
   mongoc_array_t selected_servers;

   _mongoc_array_init (&selected_servers,
                       sizeof (mongoc_server_description_t *));

   BSON_ASSERT (test);

   expected_error =
      bson_iter_init_find (&iter, test, "error") && bson_iter_as_bool (&iter);

   heartbeat_msec = MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED;

   if (bson_iter_init_find (&iter, test, "heartbeatFrequencyMS")) {
      heartbeat_msec = bson_iter_int32 (&iter);
   }

   /* pull out topology description field */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "topology_description"));
   bson_iter_bson (&iter, &test_topology);

   /* set topology state from test */
   BSON_ASSERT (bson_iter_init_find (&topology_iter, &test_topology, "type"));

   mongoc_topology_description_init (&topology, heartbeat_msec);
   topology.type =
      topology_type_from_test (bson_iter_utf8 (&topology_iter, NULL));

   /* for each server description in test, add server to our topology */
   BSON_ASSERT (
      bson_iter_init_find (&topology_iter, &test_topology, "servers"));
   bson_iter_bson (&topology_iter, &test_servers);

   bson_iter_init (&server_iter, &test_servers);
   while (bson_iter_next (&server_iter)) {
      bson_iter_bson (&server_iter, &server);

      /* initialize new server description with given address */
      sd = (mongoc_server_description_t *) bson_malloc0 (sizeof *sd);
      BSON_ASSERT (bson_iter_init_find (&sd_iter, &server, "address"));
      mongoc_server_description_init (sd, bson_iter_utf8 (&sd_iter, NULL), i++);

      BSON_ASSERT (bson_iter_init_find (&sd_iter, &server, "type"));
      sd->type = server_type_from_test (bson_iter_utf8 (&sd_iter, NULL));

      if (bson_iter_init_find (&sd_iter, &server, "avg_rtt_ms")) {
         sd->round_trip_time_msec = bson_iter_int32 (&sd_iter);
      } else if (sd->type != MONGOC_SERVER_UNKNOWN) {
         test_error ("%s has no avg_rtt_ms", sd->host.host_and_port);
         abort ();
      }

      if (bson_iter_init_find (&sd_iter, &server, "maxWireVersion")) {
         sd->max_wire_version = (int32_t) bson_iter_as_int64 (&sd_iter);
      }

      if (bson_iter_init_find (&sd_iter, &server, "lastUpdateTime")) {
         sd->last_update_time_usec = bson_iter_as_int64 (&sd_iter) * 1000;
      }

      if (bson_iter_init_find (&sd_iter, &server, "lastWrite")) {
         BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&sd_iter) &&
                      bson_iter_recurse (&sd_iter, &last_write_iter) &&
                      bson_iter_find (&last_write_iter, "lastWriteDate") &&
                      BSON_ITER_HOLDS_INT (&last_write_iter));
         sd->last_write_date_ms = bson_iter_as_int64 (&last_write_iter);
      }

      if (bson_iter_init_find (&sd_iter, &server, "tags")) {
         bson_destroy (&sd->tags);
         bson_iter_bson (&sd_iter, &sd->tags);
      }

      /* add new server to our topology description */
      mongoc_set_add (topology.servers, sd->id, sd);
   }

   /* create read preference document from test */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "read_preference"));
   bson_iter_bson (&iter, &test_read_pref);

   if (bson_iter_init_find (&read_pref_iter, &test_read_pref, "mode")) {
      read_mode = read_mode_from_test (bson_iter_utf8 (&read_pref_iter, NULL));
      ASSERT_CMPINT (read_mode, !=, 0);
   } else {
      read_mode = MONGOC_READ_PRIMARY;
   }

   read_prefs = mongoc_read_prefs_new (read_mode);

   if (bson_iter_init_find (&read_pref_iter, &test_read_pref, "tag_sets")) {
      /* ignore  "tag_sets: [{}]" */
      if (bson_iter_recurse (&read_pref_iter, &tag_sets_iter) &&
          bson_iter_next (&tag_sets_iter) &&
          BSON_ITER_HOLDS_DOCUMENT (&tag_sets_iter)) {
         bson_iter_bson (&tag_sets_iter, &first_tag_set);
         if (!bson_empty (&first_tag_set)) {
            /* not empty */
            bson_iter_bson (&read_pref_iter, &test_tag_sets);
            mongoc_read_prefs_set_tags (read_prefs, &test_tag_sets);
         }
      }
   }

   if (bson_iter_init_find (
          &read_pref_iter, &test_read_pref, "maxStalenessSeconds")) {
      mongoc_read_prefs_set_max_staleness_seconds (
         read_prefs, bson_iter_as_int64 (&read_pref_iter));
   }

   /* get operation type */
   op = MONGOC_SS_READ;

   if (bson_iter_init_find (&iter, test, "operation")) {
      op = optype_from_test (bson_iter_utf8 (&iter, NULL));
   }

   if (expected_error) {
      BSON_ASSERT (!mongoc_read_prefs_is_valid (read_prefs) ||
                   !mongoc_topology_compatible (&topology, read_prefs, &error));
      goto DONE;
   }

   /* no expected error */
   BSON_ASSERT (mongoc_read_prefs_is_valid (read_prefs));
   BSON_ASSERT (mongoc_topology_compatible (&topology, read_prefs, &error));

   /* read in latency window servers */
   BSON_ASSERT (bson_iter_init_find (&iter, test, "in_latency_window"));

   /* TODO: use topology_select instead? */
   mongoc_topology_description_suitable_servers (
      &selected_servers,
      op,
      &topology,
      read_prefs,
      MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS);

   /* check each server in expected_servers is in selected_servers */
   memset (matched_servers, 0, sizeof (matched_servers));
   bson_iter_recurse (&iter, &expected_servers_iter);
   while (bson_iter_next (&expected_servers_iter)) {
      bool found = false;
      bson_iter_t host;

      BSON_ASSERT (bson_iter_recurse (&expected_servers_iter, &host));
      BSON_ASSERT (bson_iter_find (&host, "address"));

      for (i = 0; i < selected_servers.len; i++) {
         sd = _mongoc_array_index (
            &selected_servers, mongoc_server_description_t *, i);

         if (strcmp (sd->host.host_and_port, bson_iter_utf8 (&host, NULL)) ==
             0) {
            found = true;
            break;
         }
      }

      if (!found) {
         test_error ("Should have been selected but wasn't: %s",
                     bson_iter_utf8 (&host, NULL));
         abort ();
      }

      matched_servers[i] = true;
   }

   /* check each server in selected_servers is in expected_servers */
   for (i = 0; i < selected_servers.len; i++) {
      if (!matched_servers[i]) {
         sd = _mongoc_array_index (
            &selected_servers, mongoc_server_description_t *, i);

         test_error ("Shouldn't have been selected but was: %s",
                     sd->host.host_and_port);
         abort ();
      }
   }

DONE:
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_topology_description_destroy (&topology);
   _mongoc_array_destroy (&selected_servers);
}

/*
 *-----------------------------------------------------------------------
 *
 * assemble_path --
 *
 *       Given a parent directory and filename, compile a full path to
 *       the child file.
 *
 *       "dst" receives the joined path, delimited by "/" even on Windows.
 *
 *-----------------------------------------------------------------------
 */
void
assemble_path (const char *parent_path,
               const char *child_name,
               char *dst /* OUT */)
{
   char *p;
   int path_len = (int) strlen (parent_path);
   int name_len = (int) strlen (child_name);

   BSON_ASSERT (path_len + name_len + 1 < MAX_TEST_NAME_LENGTH);

   memset (dst, '\0', MAX_TEST_NAME_LENGTH * sizeof (char));
   strncat (dst, parent_path, path_len);
   strncat (dst, "/", 1);
   strncat (dst, child_name, name_len);

   for (p = dst; *p; ++p) {
      if (*p == '\\') {
         *p = '/';
      }
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * collect_tests_from_dir --
 *
 *       Recursively search the directory at @dir_path for files with
 *       '.json' in their filenames. Append all found file paths to
 *       @paths, and return the number of files found.
 *
 *-----------------------------------------------------------------------
 */
int
collect_tests_from_dir (char (*paths)[MAX_TEST_NAME_LENGTH] /* OUT */,
                        const char *dir_path,
                        int paths_index,
                        int max_paths)
{
#ifdef _MSC_VER
   char *dir_path_plus_star;
   intptr_t handle;
   struct _finddata_t info;

   char child_path[MAX_TEST_NAME_LENGTH];

   dir_path_plus_star = bson_strdup_printf ("%s/*", dir_path);
   handle = _findfirst (dir_path_plus_star, &info);

   if (handle == -1) {
      bson_free (dir_path_plus_star);
      return 0;
   }

   while (1) {
      BSON_ASSERT (paths_index < max_paths);

      if (info.attrib & _A_SUBDIR) {
         /* recursively call on child directories */
         if (strcmp (info.name, "..") != 0 && strcmp (info.name, ".") != 0) {
            assemble_path (dir_path, info.name, child_path);
            paths_index = collect_tests_from_dir (
               paths, child_path, paths_index, max_paths);
         }
      } else if (strstr (info.name, ".json")) {
         /* if this is a JSON test, collect its path */
         assemble_path (dir_path, info.name, paths[paths_index++]);
      }

      if (_findnext (handle, &info) == -1) {
         break;
      }
   }

   bson_free (dir_path_plus_star);
   _findclose (handle);

   return paths_index;
#else
   struct dirent *entry;
   struct stat dir_stat;
   char child_path[MAX_TEST_NAME_LENGTH];
   DIR *dir;

   dir = opendir (dir_path);
   if (!dir) {
      MONGOC_ERROR ("Cannot open \"%s\"", dir_path);
      MONGOC_ERROR ("Run test-libmongoc in repository root directory.\n");
      abort ();
   }

   while ((entry = readdir (dir))) {
      BSON_ASSERT (paths_index < max_paths);
      if (strcmp (entry->d_name, "..") == 0 ||
          strcmp (entry->d_name, ".") == 0) {
         continue;
      }

      assemble_path (dir_path, entry->d_name, child_path);

      if (0 == stat (child_path, &dir_stat) && S_ISDIR (dir_stat.st_mode)) {
         /* recursively call on child directories */
         paths_index =
            collect_tests_from_dir (paths, child_path, paths_index, max_paths);
      } else if (strstr (entry->d_name, ".json")) {
         /* if this is a JSON test, collect its path */
         assemble_path (dir_path, entry->d_name, paths[paths_index++]);
      }
   }

   closedir (dir);

   return paths_index;
#endif
}

/*
 *-----------------------------------------------------------------------
 *
 * get_bson_from_json_file --
 *
 *        Open the file at @filename and store its contents in a
 *        bson_t. This function assumes that @filename contains a
 *        single JSON object.
 *
 *        NOTE: caller owns returned bson_t and must free it.
 *
 *-----------------------------------------------------------------------
 */
bson_t *
get_bson_from_json_file (char *filename)
{
   FILE *file;
   long length;
   bson_t *data;
   bson_error_t error;
   const char *buffer;

   file = fopen (filename, "rb");
   if (!file) {
      return NULL;
   }

   /* get file length */
   fseek (file, 0, SEEK_END);
   length = ftell (file);
   fseek (file, 0, SEEK_SET);
   if (length < 1) {
      return NULL;
   }

   /* read entire file into buffer */
   buffer = (const char *) bson_malloc0 (length);
   if (fread ((void *) buffer, 1, length, file) != length) {
      abort ();
   }

   fclose (file);
   if (!buffer) {
      return NULL;
   }

   /* convert to bson */
   data = bson_new_from_json ((const uint8_t *) buffer, length, &error);
   if (!data) {
      fprintf (stderr, "Cannot parse %s: %s\n", filename, error.message);
      abort ();
   }

   bson_free ((void *) buffer);

   return data;
}

static bool
check_version_info (const bson_t *scenario, bool print_reason)
{
   const char *s;
   char *padded;
   server_version_t test_version, server_version;

   if (bson_has_field (scenario, "maxServerVersion")) {
      s = bson_lookup_utf8 (scenario, "maxServerVersion");
      /* s is like "3.0", don't skip if server is 3.0.x but skip 3.1+ */
      padded = bson_strdup_printf ("%s.99", s);
      test_version = test_framework_str_to_version (padded);
      bson_free (padded);
      server_version = test_framework_get_server_version ();
      if (server_version > test_version) {
         if (print_reason && test_suite_debug_output ()) {
            printf ("      SKIP, maxServerVersion=\"%s\"\n", s);
            fflush (stdout);
         }

         return false;
      }
   }

   if (bson_has_field (scenario, "minServerVersion")) {
      s = bson_lookup_utf8 (scenario, "minServerVersion");
      test_version = test_framework_str_to_version (s);
      server_version = test_framework_get_server_version ();
      if (server_version < test_version) {
         if (print_reason && test_suite_debug_output ()) {
            printf ("      SKIP, minServerVersion=\"%s\"\n", s);
            fflush (stdout);
         }

         return false;
      }
   }

   if (bson_has_field (scenario, "topology")) {
      bson_iter_t iter;
      bson_t topology;
      char *current_topology;

      BSON_ASSERT (bson_iter_init_find (&iter, scenario, "topology"));
      BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (&iter));

      bson_iter_bson (&iter, &topology);

      /* Determine cluster type */
      if (test_framework_is_mongos ()) {
         current_topology = "sharded";
      } else if (test_framework_is_replset ()) {
         current_topology = "replicaset";
      } else {
         current_topology = "single";
      }

      bson_iter_init (&iter, &topology);
      while (bson_iter_next (&iter)) {
         const char *test_topology;

         BSON_ASSERT (BSON_ITER_HOLDS_UTF8 (&iter));
         test_topology = bson_iter_utf8 (&iter, NULL);

         if (strcmp (test_topology, current_topology) == 0) {
            return true;
         }
      }

      /* If we didn't match any of the listed topologies, skip */
      if (print_reason && test_suite_debug_output ()) {
         printf ("     SKIP, test topologies do not match current %s setup\n",
                 current_topology);
         fflush (stdout);
      }

      return false;
   }

   return true;
}

static bool
check_scenario_version (const bson_t *scenario)
{
   /* version info can be nested inside "runOn" array */
   if (bson_has_field (scenario, "runOn")) {
      bson_iter_t iter;
      bson_t run_on;
      bson_t version_info;

      bson_lookup_doc (scenario, "runOn", &run_on);
      BSON_ASSERT (bson_iter_init (&iter, &run_on));

      while (bson_iter_next (&iter)) {
         bson_iter_bson (&iter, &version_info);
         if (check_version_info (&version_info, false)) {
            return true;
         }
      }

      if (test_suite_debug_output ()) {
         printf ("      SKIP, no matching topologies in runOn\n");
         fflush (stdout);
      }

      return false;
   }

   return check_version_info (scenario, true);
}


static int
check_test_version (const bson_t *test)
{
   const char *s;
   char *padded;
   server_version_t test_version, server_version;

   if (bson_has_field (test, "minServerVersion")) {
      s = bson_lookup_utf8 (test, "minServerVersion");
      test_version = test_framework_str_to_version (s);
      server_version = test_framework_get_server_version ();
      if (server_version < test_version) {
         if (test_suite_debug_output ()) {
            printf ("      SKIP, minServerVersion %s\n", s);
            fflush (stdout);
         }
         return false;
      }
   }

   if (bson_has_field (test, "ignore_if_server_version_greater_than")) {
      s = bson_lookup_utf8 (test, "ignore_if_server_version_greater_than");
      /* s is like "3.0", don't skip if server is 3.0.x but skip 3.1+ */
      padded = bson_strdup_printf ("%s.99", s);
      test_version = test_framework_str_to_version (padded);
      bson_free (padded);
      server_version = test_framework_get_server_version ();
      if (server_version > test_version) {
         if (test_suite_debug_output ()) {
            printf ("      SKIP, ignore_if_server_version_greater_than %s\n",
                    s);
            fflush (stdout);
         }
         return false;
      }
   }

   if (bson_has_field (test, "ignore_if_server_version_less_than")) {
      s = bson_lookup_utf8 (test, "ignore_if_server_version_less_than");
      test_version = test_framework_str_to_version (s);
      server_version = test_framework_get_server_version ();
      if (server_version < test_version) {
         if (test_suite_debug_output ()) {
            printf ("      SKIP, ignore_if_server_version_less_than %s\n", s);
            fflush (stdout);
         }
         return false;
      }
   }

   /* server version is ok, don't skip the test */
   return true;
}


/* is this test allowed to run against the current test topology? */
static bool
check_topology_type (const bson_t *test)
{
   bson_iter_t iter;
   bson_iter_t child;
   const char *s;
   bool compatible;
   bool is_mongos;
   bool is_replset;
   bool is_single;
   bool match;
   bool can_proceed;

   /* "topology" is an array of compatible topologies.
    * "ignore_if_topology_type" is an array of incompatible types.
    * So far, the only valid values are "single", "sharded", and  "replicaset"
    */

   if (bson_iter_init_find (&iter, test, "topology")) {
      compatible = true;
   } else if (bson_iter_init_find (&iter, test, "ignore_if_topology_type")) {
      compatible = false;
   } else {
      return true;
   }

   ASSERT (BSON_ITER_HOLDS_ARRAY (&iter));
   ASSERT (bson_iter_recurse (&iter, &child));

   is_mongos = test_framework_is_mongos ();
   is_replset = test_framework_is_replset ();
   is_single = !is_mongos && !is_replset;
   match = false;

   while (bson_iter_next (&child)) {
      if (BSON_ITER_HOLDS_UTF8 (&child)) {
         s = bson_iter_utf8 (&child, NULL);
         if (!strcmp (s, "sharded") && is_mongos) {
            match = true;
         } else if (!strcmp (s, "replicaset") && is_replset) {
            match = true;
         } else if (!strcmp (s, "single") && is_single) {
            match = true;
         }
      }
   }

   can_proceed = (compatible == match);

   if (!can_proceed && test_suite_debug_output ()) {
      printf ("      SKIP, incompatible topology type\n");
      fflush (stdout);
   }

   return can_proceed;
}


static void
_insert_data (mongoc_collection_t *collection, bson_t *documents)
{
   bson_iter_t iter;
   mongoc_bulk_operation_t *bulk;
   bson_t *majority = tmp_bson ("{'writeConcern': {'w': 'majority'}}");
   bool r;
   uint32_t server_id;
   bson_error_t error;

   mongoc_collection_delete_many (
      collection, tmp_bson ("{}"), majority, NULL, NULL);

   if (!bson_count_keys (documents)) {
      return;
   }

   bson_iter_init (&iter, documents);
   bulk =
      mongoc_collection_create_bulk_operation_with_opts (collection, majority);

   while (bson_iter_next (&iter)) {
      bson_t document;
      bson_t opts = BSON_INITIALIZER;

      bson_iter_bson (&iter, &document);
      r = mongoc_bulk_operation_insert_with_opts (
         bulk, &document, &opts, &error);
      ASSERT_OR_PRINT (r, error);

      bson_destroy (&opts);
   }

   server_id = mongoc_bulk_operation_execute (bulk, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);

   mongoc_bulk_operation_destroy (bulk);
}


/* insert the documents in a spec test scenario's "data" array */
static void
insert_data (const char *db_name,
             const char *collection_name,
             const bson_t *scenario)
{
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *collection;
   mongoc_collection_t *tmp_collection;
   bson_error_t error;
   bson_t documents;
   bson_iter_t iter;
   bson_t *majority = tmp_bson ("{'writeConcern': {'w': 'majority'}}");

   /* use a fresh client to prepare the collection */
   client = test_framework_client_new ();

   db = mongoc_client_get_database (client, db_name);
   collection = mongoc_database_get_collection (db, collection_name);
   mongoc_collection_delete_many (
      collection, tmp_bson ("{}"), majority, NULL, NULL);

   /* ignore failure if it already exists */
   tmp_collection =
      mongoc_database_create_collection (db, collection_name, majority, &error);

   if (tmp_collection) {
      mongoc_collection_destroy (tmp_collection);
   }

   if (!bson_has_field (scenario, "data")) {
      goto DONE;
   }

   bson_iter_init_find (&iter, scenario, "data");

   if (BSON_ITER_HOLDS_ARRAY (&iter)) {
      bson_lookup_doc (scenario, "data", &documents);
      _insert_data (collection, &documents);
   } else {
      /* go through collection: [] */
      bson_iter_recurse (&iter, &iter);
      while (bson_iter_next (&iter)) {
         bson_t collection_documents;

         mongoc_collection_destroy (collection);
         collection =
            mongoc_database_get_collection (db, bson_iter_key (&iter));
         bson_iter_bson (&iter, &collection_documents);
         _insert_data (collection, &collection_documents);
      }
   }

DONE:
   mongoc_database_destroy (db);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
check_outcome_collection (mongoc_collection_t *collection, bson_t *test)
{
   bson_t data;
   bson_iter_t iter;
   mongoc_read_concern_t *rc;
   mongoc_cursor_t *cursor;
   bson_t query = BSON_INITIALIZER;
   mongoc_read_prefs_t *prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   bson_lookup_doc (test, "outcome.collection.data", &data);
   ASSERT (bson_iter_init (&iter, &data));

   rc = mongoc_read_concern_new ();

   /* If the collection has had its read_concern set by a test,
      make sure it's set to LOCAL for this check. */
   if (mongoc_read_concern_get_level (collection->read_concern)) {
      mongoc_read_concern_set_level (rc, MONGOC_READ_CONCERN_LEVEL_LOCAL);
      mongoc_collection_set_read_concern (collection, rc);
   }

   cursor = mongoc_collection_find_with_opts (collection, &query, NULL, prefs);

   while (bson_iter_next (&iter)) {
      bson_t expected_doc;
      const bson_t *actual_doc;

      bson_iter_bson (&iter, &expected_doc);
      ASSERT_CURSOR_NEXT (cursor, &actual_doc);
      ASSERT (match_bson (actual_doc, &expected_doc, false));
   }

   ASSERT_CURSOR_DONE (cursor);

   bson_destroy (&data);
   mongoc_read_prefs_destroy (prefs);
   bson_destroy (&query);
   mongoc_read_concern_destroy (rc);
   mongoc_cursor_destroy (cursor);
}


static void
execute_test (const json_test_config_t *config,
              mongoc_client_t *client,
              mongoc_database_t *db,
              mongoc_collection_t *collection,
              bson_t *test)
{
   json_test_ctx_t ctx;
   uint32_t server_id;
   bson_error_t error;
   mongoc_collection_t *other_collection;

   if (test_suite_debug_output ()) {
      const char *description = bson_lookup_utf8 (test, "description");
      printf ("  - %s\n", description);
      fflush (stdout);
   }

   if (!check_test_version (test) || !check_topology_type (test)) {
      return;
   }

   /* Select a primary for testing */
   server_id = mongoc_topology_select_server_id (
      collection->client->topology, MONGOC_SS_WRITE, NULL, &error);
   ASSERT_OR_PRINT (server_id, error);

   json_test_ctx_init (&ctx, test, client, db, collection, config);

   if (config->before_test_cb) {
      config->before_test_cb (&ctx, test);
   }

   if (bson_has_field (test, "failPoint")) {
      activate_fail_point (client, server_id, test, "failPoint");
   }

   set_apm_callbacks (&ctx, collection->client);

   json_test_operations (&ctx, test);

   if (config->after_test_cb) {
      config->after_test_cb (&ctx, test);
   }

   json_test_ctx_end_sessions (&ctx);

   if (bson_has_field (test, "expectations")) {
      bson_t expectations;

      bson_lookup_doc (test, "expectations", &expectations);
      check_json_apm_events (&ctx, &expectations);
      if (config->events_check_cb) {
         config->events_check_cb (&ctx.events);
      }
   }

   if (bson_has_field (test, "outcome.collection")) {
      if (bson_has_field (test, "outcome.collection.name")) {
         other_collection = mongoc_database_get_collection (
            db, bson_lookup_utf8 (test, "outcome.collection.name"));
         check_outcome_collection (other_collection, test);
         mongoc_collection_destroy (other_collection);
      } else {
         check_outcome_collection (collection, test);
      }
   }

   mongoc_client_set_apm_callbacks (collection->client, NULL, NULL);
   json_test_ctx_cleanup (&ctx);
   deactivate_fail_points (collection->client, server_id);
}


void
activate_fail_point (mongoc_client_t *client,
                     const uint32_t server_id,
                     const bson_t *test,
                     const char *key)
{
   bson_t command;
   bson_error_t error;
   bool r;

   BSON_ASSERT (client);
   BSON_ASSERT (server_id);

   bson_lookup_doc (test, key, &command);

   ASSERT_CMPSTR (_mongoc_get_command_name (&command), "configureFailPoint");
   r = mongoc_client_command_simple_with_server_id (
      client, "admin", &command, NULL, server_id, NULL, &error);
   ASSERT_OR_PRINT (r, error);
}

/*
 *-----------------------------------------------------------------------
 *
 * deactivate_fail_points --
 *
 *      Deactivate the onPrimaryTransactionalWrite fail point, and all
 *      future fail points used in JSON tests.
 *
 *-----------------------------------------------------------------------
 */
void
deactivate_fail_points (mongoc_client_t *client, uint32_t server_id)
{
   mongoc_server_description_t *sd;
   bson_t *command;
   bool r;
   bson_error_t error;

   sd = mongoc_client_get_server_description (client, server_id);
   BSON_ASSERT (sd);

   if (sd->type == MONGOC_SERVER_RS_PRIMARY &&
       sd->max_wire_version >= WIRE_VERSION_RETRY_WRITES) {
      command =
         tmp_bson ("{'configureFailPoint': 'onPrimaryTransactionalWrite',"
                   " 'mode': 'off'}");

      r = mongoc_client_command_simple_with_server_id (
         client, "admin", command, NULL, server_id, NULL, &error);
      ASSERT_OR_PRINT (r, error);

      command = tmp_bson ("{'configureFailPoint': 'failCommand',"
                          " 'mode': 'off'}");

      r = mongoc_client_command_simple_with_server_id (
         client, "admin", command, NULL, server_id, NULL, &error);

      /* ignore error from servers that predate "failCommand" fail point */
      if (!r && !strstr (error.message, "failCommand not found")) {
         ASSERT_OR_PRINT (r, error);
      }
   }

   mongoc_server_description_destroy (sd);
}


static void
set_uri_opts_from_bson (mongoc_uri_t *uri, const bson_t *opts)
{
   bson_iter_t iter;

   BSON_ASSERT (bson_iter_init (&iter, opts));
   while (bson_iter_next (&iter)) {
      /* can't use bson_lookup_write_concern etc. with clientOptions format */
      if (!strcmp (bson_iter_key (&iter), "w")) {
         mongoc_write_concern_t *wc = mongoc_write_concern_new ();
         if (BSON_ITER_HOLDS_UTF8 (&iter)) {
            mongoc_write_concern_set_wtag (wc, bson_iter_utf8 (&iter, NULL));
         } else if (BSON_ITER_HOLDS_INT (&iter)) {
            mongoc_write_concern_set_w (wc,
                                        (int32_t) bson_iter_as_int64 (&iter));
         } else {
            test_error ("Unrecognized type for 'w': %d",
                        bson_iter_type (&iter));
         }

         mongoc_uri_set_write_concern (uri, wc);
         mongoc_write_concern_destroy (wc);
      } else if (!strcmp (bson_iter_key (&iter), "readConcernLevel")) {
         mongoc_read_concern_t *rc = mongoc_read_concern_new ();
         mongoc_read_concern_set_level (rc, bson_iter_utf8 (&iter, NULL));
         mongoc_uri_set_read_concern (uri, rc);
         mongoc_read_concern_destroy (rc);
      } else if (!strcmp (bson_iter_key (&iter), "readPreference")) {
         mongoc_read_prefs_t *read_prefs = mongoc_read_prefs_new (
            read_mode_from_test (bson_iter_utf8 (&iter, NULL)));
         mongoc_uri_set_read_prefs_t (uri, read_prefs);
         mongoc_read_prefs_destroy (read_prefs);
      } else if (!strcmp (bson_iter_key (&iter), "retryWrites")) {
         mongoc_uri_set_option_as_bool (
            uri, "retryWrites", bson_iter_bool (&iter));
      } else if (!strcmp (bson_iter_key (&iter), "heartbeatFrequencyMS")) {
         mongoc_uri_set_option_as_int32 (
            uri, "heartbeatFrequencyMS", bson_iter_int32 (&iter));
      } else if (!strcmp (bson_iter_key (&iter), "retryReads")) {
         mongoc_uri_set_option_as_bool (
            uri, "retryReads", bson_iter_bool (&iter));
      } else {
         MONGOC_ERROR ("Unsupported clientOptions field \"%s\" in %s",
                       bson_iter_key (&iter),
                       bson_as_json (opts, NULL));
         abort ();
      }
   }
}


static bool
_should_skip_due_to_server_39704 (const bson_t *test)
{
   const char *desc = bson_lookup_utf8 (test, "description");
   const char *bad_tests[] = {
      "only first countDocuments includes readConcern",
      "only first find includes readConcern",
      "only first aggregate includes readConcern",
      "only first distinct includes readConcern",
      "only first runCommand includes readConcern",
      "transaction options inherited from defaultTransactionOptions",
      "startTransaction options override defaults",
      "defaultTransactionOptions override client options",
      "readConcern snapshot in startTransaction options",
      "withTransaction inherits transaction options from "
      "defaultTransactionOptions",
      "withTransaction explicit transaction options",
      "withTransaction explicit transaction options override "
      "defaultTransactionOptions",
      "withTransaction explicit transaction options override client options"};
   int i;

   /* Only an issue for sharded clusters. */
   if (!test_framework_is_mongos ()) {
      return false;
   }

   for (i = 0; i < sizeof (bad_tests) / sizeof (char *); i++) {
      if (0 == strcmp (desc, bad_tests[i])) {
         return true;
      }
   }

   return false;
}


/*
 *-----------------------------------------------------------------------
 *
 * run_json_general_test --
 *
 *      Run a JSON test scenario from the CRUD, Command Monitoring,
 *      Retryable Writes, Change Stream, or Transactions Spec.
 *
 *      Call json_test_config_cleanup on @config after the last call
 *      to run_json_general_test.
 *
 *-----------------------------------------------------------------------
 */
void
run_json_general_test (const json_test_config_t *config)
{
   const bson_t *scenario = config->scenario;
   bson_iter_t scenario_iter;
   bson_iter_t tests_iter;
   const char *db_name;
   const char *collection_name;

   ASSERT (scenario);

   if (!check_scenario_version (scenario)) {
      return;
   }

   db_name = bson_has_field (scenario, "database_name")
                ? bson_lookup_utf8 (scenario, "database_name")
                : "test";
   collection_name = bson_has_field (scenario, "collection_name")
                        ? bson_lookup_utf8 (scenario, "collection_name")
                        : "test";

   ASSERT (bson_iter_init_find (&scenario_iter, scenario, "tests"));
   ASSERT (BSON_ITER_HOLDS_ARRAY (&scenario_iter));
   ASSERT (bson_iter_recurse (&scenario_iter, &tests_iter));

   while (bson_iter_next (&tests_iter)) {
      bson_t test;
      char *selected_test;
      const char *description;
      bson_iter_t client_opts_iter;
      mongoc_uri_t *uri;
      mongoc_client_t *client;
      mongoc_database_t *db;
      mongoc_collection_t *collection;
      uint32_t server_id;
      bson_error_t error;
      bool r;
      bson_iter_t uri_iter;

      ASSERT (BSON_ITER_HOLDS_DOCUMENT (&tests_iter));
      bson_iter_bson (&tests_iter, &test);

      selected_test = test_framework_getenv ("MONGOC_JSON_SUBTEST");
      description = bson_lookup_utf8 (&test, "description");
      if (selected_test && strcmp (selected_test, description) != 0) {
         fprintf (
            stderr, "  - %s SKIPPED by MONGOC_JSON_SUBTEST\n", description);
         bson_free (selected_test);
         continue;
      }

      if (bson_has_field (&test, "skipReason")) {
         fprintf (stderr,
                  " - %s SKIPPED, reason: %s\n",
                  description,
                  bson_lookup_utf8 (&test, "skipReason"));
         continue;
      }

      if (_should_skip_due_to_server_39704 (&test)) {
         fprintf (stderr,
                  " - %s SKIPPED, reason: SERVER-39704 causes sharded tests to "
                  "fail when using readConcern: snapshot\n",
                  description);
         continue;
      }

      bson_free (selected_test);

      uri = test_framework_get_uri ();

      /* If we are using multiple mongos, hardcode them in, for now, but keep
       * the other URI components (CDRIVER-3285) */
      if (bson_iter_init_find (&uri_iter, &test, "useMultipleMongoses") &&
          bson_iter_as_bool (&uri_iter)) {
         ASSERT_OR_PRINT (
            mongoc_uri_upsert_host_and_port (uri, "localhost:27017", &error),
            error);
         ASSERT_OR_PRINT (
            mongoc_uri_upsert_host_and_port (uri, "localhost:27018", &error),
            error);
      }

      if (bson_iter_init_find (&client_opts_iter, &test, "clientOptions")) {
         bson_t client_opts;

         ASSERT (BSON_ITER_HOLDS_DOCUMENT (&tests_iter));
         bson_iter_bson (&client_opts_iter, &client_opts);
         set_uri_opts_from_bson (uri, &client_opts);
      }

      client = mongoc_client_new_from_uri (uri);
      mongoc_client_set_error_api (client, 2);
      test_framework_set_ssl_opts (client);
      /* reconnect right away, if a fail point causes a disconnect */
      client->topology->min_heartbeat_frequency_msec = 0;
      mongoc_uri_destroy (uri);

      /* clean up in case a previous test aborted */
      server_id = mongoc_topology_select_server_id (
         client->topology, MONGOC_SS_WRITE, NULL, &error);
      ASSERT_OR_PRINT (server_id, error);
      deactivate_fail_points (client, server_id);
      r = mongoc_client_command_with_opts (client,
                                           "admin",
                                           tmp_bson ("{'killAllSessions': []}"),
                                           NULL,
                                           NULL,
                                           NULL,
                                           &error);

      /* expect "operation was interrupted", ignore "command not found" */
      if (!r && (error.domain != MONGOC_ERROR_SERVER ||
                 (error.code != 11601 && error.code != 59))) {
         MONGOC_WARNING ("Error in killAllSessions: %s", error.message);
      }

      insert_data (db_name, collection_name, scenario);

      db = mongoc_client_get_database (client, db_name);
      collection = mongoc_database_get_collection (db, collection_name);
      execute_test (config, client, db, collection, &test);

      mongoc_collection_destroy (collection);
      mongoc_database_destroy (db);
      mongoc_client_destroy (client);
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * json_test_config_cleanup --
 *
 *      Free memory after run_json_general_test.
 *
 *-----------------------------------------------------------------------
 */

void
json_test_config_cleanup (json_test_config_t *config)
{
   /* no-op */
}


/* Tests on unsupported operations are automatically skipped with a message
 * indicating why. */
static bson_t *
_skip_if_unsupported (const char *test_name, bson_t *original)
{
   int i;
   bool skip = false;
   const char *unsupported_tests[] = {
      "/retryable_reads/gridfs-downloadByName",
      "/retryable_reads/gridfs-downloadByName-serverErrors",
      "/retryable_reads/listCollectionObjects",
      "/retryable_reads/listCollectionObjects-serverErrors",
      "/retryable_reads/listDatabaseObjects",
      "/retryable_reads/listDatabaseObjects-serverErrors",
      "/retryable_reads/listIndexNames",
      "/retryable_reads/listIndexNames-serverErrors",
      "/retryable_reads/mapReduce"};

   for (i = 0; i < sizeof (unsupported_tests) / sizeof (unsupported_tests[0]);
        i++) {
      if (0 == strcmp (test_name, unsupported_tests[i])) {
         skip = true;
         break;
      }
   }

   if (skip) {
      /* Modify the test file to give all entries in "tests" a skipReason */
      bson_t *modified = bson_new ();
      bson_t modified_tests;
      bson_iter_t iter;

      bson_copy_to_excluding_noinit (original, modified, "tests", NULL);
      BSON_APPEND_ARRAY_BEGIN (modified, "tests", &modified_tests);
      BSON_ASSERT (bson_iter_init_find (&iter, original, "tests"));
      for (bson_iter_recurse (&iter, &iter); bson_iter_next (&iter);) {
         bson_t original_test;
         bson_t modified_test;

         bson_iter_bson (&iter, &original_test);
         BSON_APPEND_DOCUMENT_BEGIN (
            &modified_tests, bson_iter_key (&iter), &modified_test);
         bson_concat (&modified_test, &original_test);
         BSON_APPEND_UTF8 (&modified_test,
                           "skipReason",
                           "libmongoc does not support required operation.");
         bson_append_document_end (&modified_tests, &modified_test);
      }
      bson_append_array_end (modified, &modified_tests);
      bson_destroy (original);
      return modified;
   }
   return original;
}

/*
 *-----------------------------------------------------------------------
 *
 * install_json_test_suite --
 *
 *      Given a path to a directory containing JSON tests, import each
 *      test into a BSON blob and call the provided callback for
 *      evaluation.
 *
 *      It is expected that the callback will BSON_ASSERT on failure, so if
 *      callback returns quietly the test is considered to have passed.
 *
 *-----------------------------------------------------------------------
 */
void
_install_json_test_suite_with_check (TestSuite *suite,
                                     const char *dir_path,
                                     test_hook callback,
                                     ...)
{
   char test_paths[MAX_NUM_TESTS][MAX_TEST_NAME_LENGTH];
   int num_tests;
   int i;
   bson_t *test;
   char *skip_json;
   char *ext;
   va_list ap;

   num_tests =
      collect_tests_from_dir (&test_paths[0], dir_path, 0, MAX_NUM_TESTS);

   for (i = 0; i < num_tests; i++) {
      test = get_bson_from_json_file (test_paths[i]);
      skip_json = COALESCE (strstr (test_paths[i], "/json"),
                            strstr (test_paths[i], "\\json"));
      BSON_ASSERT (skip_json);
      skip_json += strlen ("/json");
      ext = strstr (skip_json, ".json");
      BSON_ASSERT (ext);
      ext[0] = '\0';

      test = _skip_if_unsupported (skip_json, test);
      /* list of "check" functions that decide whether to skip the test */
      va_start (ap, callback);
      _V_TestSuite_AddFull (suite,
                            skip_json,
                            (void (*) (void *)) callback,
                            (void (*) (void *)) bson_destroy,
                            test,
                            ap);

      va_end (ap);
   }
}


/*
 *-----------------------------------------------------------------------
 *
 * install_json_test_suite --
 *
 *      Given a path to a directory containing JSON tests, import each
 *      test into a BSON blob and call the provided callback for
 *      evaluation.
 *
 *      It is expected that the callback will BSON_ASSERT on failure, so if
 *      callback returns quietly the test is considered to have passed.
 *
 *-----------------------------------------------------------------------
 */
void
install_json_test_suite (TestSuite *suite,
                         const char *dir_path,
                         test_hook callback)
{
   install_json_test_suite_with_check (
      suite, dir_path, callback, TestSuite_CheckLive);
}
