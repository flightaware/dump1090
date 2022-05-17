:man_page: mongoc_collection_find_indexes_with_opts

mongoc_collection_find_indexes_with_opts()
==========================================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_collection_find_indexes_with_opts (mongoc_collection_t *collection,
                                            const bson_t *opts);

Fetches a cursor containing documents, each corresponding to an index on this collection.

.. include:: includes/retryable-read.txt

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``opts``: A :symbol:`bson:bson_t` containing additional options.

.. |opts-source| replace:: ``collection``

.. include:: includes/generic-opts.txt

Errors
------

Use :symbol:`mongoc_cursor_error` on the returned cursor to check for errors.

Returns
-------

A cursor where each result corresponds to the server's representation of an index on this collection. If the collection does not exist on the server, the cursor will be empty.

The cursor functions :symbol:`mongoc_cursor_set_limit`, :symbol:`mongoc_cursor_set_batch_size`, and :symbol:`mongoc_cursor_set_max_await_time_ms` have no use on the returned cursor.

