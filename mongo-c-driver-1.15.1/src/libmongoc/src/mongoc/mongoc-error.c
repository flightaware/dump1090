/*
 * Copyright 2018-present MongoDB, Inc.
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

#include <bson/bson.h>

#include "mongoc/mongoc-error.h"
#include "mongoc/mongoc-error-private.h"
#include "mongoc/mongoc-rpc-private.h"

bool
mongoc_error_has_label (const bson_t *reply, const char *label)
{
   bson_iter_t iter;
   bson_iter_t error_labels;

   BSON_ASSERT (reply);
   BSON_ASSERT (label);

   if (bson_iter_init_find (&iter, reply, "errorLabels") &&
       bson_iter_recurse (&iter, &error_labels)) {
      while (bson_iter_next (&error_labels)) {
         if (BSON_ITER_HOLDS_UTF8 (&error_labels) &&
             !strcmp (bson_iter_utf8 (&error_labels, NULL), label)) {
            return true;
         }
      }
   }

   return false;
}


/*--------------------------------------------------------------------------
 *
 * _mongoc_read_error_get_type --
 *
 *       Checks if the error or reply from a read command is considered
 *       retryable according to the retryable reads spec. Checks both
 *       for a client error (a network exception) and a server error in
 *       the reply. @cmd_ret and @cmd_err come from the result of a
 *       read_command function.
 *
 *
 * Return:
 *       A mongoc_read_error_type_t indicating the type of error (if any).
 *
 *--------------------------------------------------------------------------
 */
mongoc_read_err_type_t
_mongoc_read_error_get_type (bool cmd_ret,
                             const bson_error_t *cmd_err,
                             const bson_t *reply)
{
   bson_error_t error;

   /* check for a client error. */
   if (!cmd_ret && cmd_err && cmd_err->domain == MONGOC_ERROR_STREAM) {
      /* Retryable reads spec: "considered retryable if [...] any network
       * exception (e.g. socket timeout or error) */
      return MONGOC_READ_ERR_RETRY;
   }

   /* check for a server error. */
   if (_mongoc_cmd_check_ok_no_wce (
          reply, MONGOC_ERROR_API_VERSION_2, &error)) {
      return MONGOC_READ_ERR_NONE;
   }

   switch (error.code) {
   case 11600: /* InterruptedAtShutdown */
   case 11602: /* InterruptedDueToReplStateChange */
   case 10107: /* NotMaster */
   case 13435: /* NotMasterNoSlaveOk */
   case 13436: /* NotMasterOrSecondary */
   case 189:   /* PrimarySteppedDown */
   case 91:    /* ShutdownInProgress */
   case 7:     /* HostNotFound */
   case 6:     /* HostUnreachable */
   case 89:    /* NetworkTimeout */
   case 9001:  /* SocketException */
      return MONGOC_READ_ERR_RETRY;
   default:
      if (strstr (error.message, "not master") ||
          strstr (error.message, "node is recovering")) {
         return MONGOC_READ_ERR_RETRY;
      }
      return MONGOC_READ_ERR_OTHER;
   }
}
