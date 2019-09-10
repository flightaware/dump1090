:man_page: mongoc_collection_aggregate

mongoc_collection_aggregate()
=============================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_collection_aggregate (mongoc_collection_t *collection,
                               mongoc_query_flags_t flags,
                               const bson_t *pipeline,
                               const bson_t *opts,
                               const mongoc_read_prefs_t *read_prefs)
     BSON_GNUC_WARN_UNUSED_RESULT;

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``flags``: A :symbol:`mongoc_query_flags_t`.
* ``pipeline``: A :symbol:`bson:bson_t`, either a BSON array or a BSON document containing an array field named "pipeline".
* ``opts``: A :symbol:`bson:bson_t` containing options for the command, or ``NULL``.
* ``read_prefs``: A :symbol:`mongoc_read_prefs_t` or ``NULL``.

``opts`` may be NULL or a BSON document with additional command options:

* ``readConcern``: Construct a :symbol:`mongoc_read_concern_t` and use :symbol:`mongoc_read_concern_append` to add the read concern to ``opts``. See the example code for :symbol:`mongoc_client_read_command_with_opts`. Read concern requires MongoDB 3.2 or later, otherwise an error is returned.
* ``writeConcern``: For aggregations that include "$out", you can construct a :symbol:`mongoc_write_concern_t` and use :symbol:`mongoc_write_concern_append` to add the write concern to ``opts``. See the example code for :symbol:`mongoc_client_write_command_with_opts`.
* ``sessionId``: Construct a :symbol:`mongoc_client_session_t` with :symbol:`mongoc_client_start_session` and use :symbol:`mongoc_client_session_append` to add the session to ``opts``. See the example code for :symbol:`mongoc_client_session_t`.
* ``bypassDocumentValidation``: Set to ``true`` to skip server-side schema validation of the provided BSON documents.
* ``collation``: Configure textual comparisons. See :ref:`Setting Collation Order <setting_collation_order>`, and `the MongoDB Manual entry on Collation <https://docs.mongodb.com/manual/reference/collation/>`_. Collation requires MongoDB 3.2 or later, otherwise an error is returned.
* ``serverId``: To target a specific server, include an int32 "serverId" field. Obtain the id by calling :symbol:`mongoc_client_select_server`, then :symbol:`mongoc_server_description_id` on its return value.
* ``batchSize``: To specify the number of documents to return in each batch of a response from the server, include an int "batchSize" field.

For a list of all options, see `the MongoDB Manual entry on the aggregate command <http://docs.mongodb.org/manual/reference/command/aggregate/>`_.


.. include:: includes/retryable-read-aggregate.txt

Description
-----------

This function creates a cursor which sends the aggregate command on the underlying collection upon the first call to :symbol:`mongoc_cursor_next()`. For more information on building aggregation pipelines, see `the MongoDB Manual entry on the aggregate command <http://docs.mongodb.org/manual/reference/command/aggregate/>`_.

Read preferences, read and write concern, and collation can be overridden by various sources. The highest-priority sources for these options are listed first in the following table. In a transaction, read concern and write concern are prohibited in ``opts`` and the read preference must be primary or NULL. Write concern is applied from ``opts``, or if ``opts`` has no write concern and the aggregation pipeline includes "$out", the write concern is applied from ``collection``. The write concern is omitted for MongoDB before 3.4.

================== ============== ============== =========
Read Preferences   Read Concern   Write Concern  Collation
================== ============== ============== =========
``read_prefs``     ``opts``       ``opts``       ``opts``
Transaction        Transaction    Transaction
``collection``     ``collection`` ``collection``
================== ============== ============== =========

:ref:`See the example for transactions <mongoc_client_session_start_transaction_example>` and for :ref:`the "distinct" command with opts <mongoc_client_read_command_with_opts_example>`.

Returns
-------

This function returns a newly allocated :symbol:`mongoc_cursor_t` that should be freed with :symbol:`mongoc_cursor_destroy()` when no longer in use. The returned :symbol:`mongoc_cursor_t` is never ``NULL``; if the parameters are invalid, the :symbol:`bson:bson_error_t` in the :symbol:`mongoc_cursor_t` is filled out, and the :symbol:`mongoc_cursor_t` is returned before the server is selected. The user must call :symbol:`mongoc_cursor_next()` on the returned :symbol:`mongoc_cursor_t` to execute the aggregation pipeline.

.. warning::

  Failure to handle the result of this function is a programming error.

Example
-------

.. code-block:: c

  #include <bson/bson.h>
  #include <mongoc/mongoc.h>

  static mongoc_cursor_t *
  pipeline_query (mongoc_collection_t *collection)
  {
     mongoc_cursor_t *cursor;
     bson_t *pipeline;

     pipeline = BCON_NEW ("pipeline",
                          "[",
                          "{",
                          "$match",
                          "{",
                          "foo",
                          BCON_UTF8 ("A"),
                          "}",
                          "}",
                          "{",
                          "$match",
                          "{",
                          "bar",
                          BCON_BOOL (false),
                          "}",
                          "}",
                          "]");

     cursor = mongoc_collection_aggregate (
        collection, MONGOC_QUERY_NONE, pipeline, NULL, NULL);

     bson_destroy (pipeline);

     return cursor;
  }

Other Parameters
----------------

When using ``$out``, the pipeline stage that writes, the write_concern field of the :symbol:`mongoc_cursor_t` will be set to the :symbol:`mongoc_write_concern_t` parameter, if it is valid, and applied to the write command when :symbol:`mongoc_cursor_next()` is called. Pass any other parameters to the ``aggregate`` command, besides ``pipeline``, as fields in ``opts``:

.. code-block:: c

  mongoc_write_concern_t *write_concern = mongoc_write_concern_new ();
  mongoc_write_concern_set_w (write_concern, 3);

  pipeline =
     BCON_NEW ("pipeline", "[", "{", "$out", BCON_UTF8 ("collection2"), "}", "]");

  opts = BCON_NEW ("bypassDocumentValidation", BCON_BOOL (true));
  mongoc_write_concern_append (write_concern, opts);

  cursor = mongoc_collection_aggregate (
     collection1, MONGOC_QUERY_NONE, pipeline, opts, NULL);

