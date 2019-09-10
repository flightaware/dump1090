/*
 * Copyright 2015 MongoDB, Inc.
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

#include "mongoc/mongoc-prelude.h"

#ifndef MONGOC_URI_PRIVATE_H
#define MONGOC_URI_PRIVATE_H

#include "mongoc/mongoc-uri.h"


BSON_BEGIN_DECLS


bool
mongoc_uri_upsert_host_and_port (mongoc_uri_t *uri,
                                 const char *host_and_port,
                                 bson_error_t *error);
bool
mongoc_uri_upsert_host (mongoc_uri_t *uri,
                        const char *host,
                        uint16_t port,
                        bson_error_t *error);
void
mongoc_uri_remove_host (mongoc_uri_t *uri, const char *host, uint16_t port);

bool
mongoc_uri_parse_host (mongoc_uri_t *uri, const char *str);
bool
mongoc_uri_parse_options (mongoc_uri_t *uri,
                          const char *str,
                          bool from_dns,
                          bson_error_t *error);
int32_t
mongoc_uri_get_local_threshold_option (const mongoc_uri_t *uri);

bool
_mongoc_uri_requires_auth_negotiation (const mongoc_uri_t *uri);

const char *
mongoc_uri_canonicalize_option (const char *key);

mongoc_uri_t *
_mongoc_uri_copy_and_replace_host_list (const mongoc_uri_t *original,
                                        const char *host);

BSON_END_DECLS


#endif /* MONGOC_URI_PRIVATE_H */
