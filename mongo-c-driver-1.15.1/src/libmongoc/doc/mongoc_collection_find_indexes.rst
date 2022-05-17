:man_page: mongoc_collection_find_indexes

mongoc_collection_find_indexes()
================================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_collection_find_indexes (mongoc_collection_t *collection,
                                  bson_error_t *error);

Deprecated
----------

This function is deprecated and should not be used in new code.

Please use :symbol:`mongoc_collection_find_indexes_with_opts()` instead.

Fetches a cursor containing documents, each corresponding to an index on this collection.

.. include:: includes/retryable-read.txt

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

A cursor where each result corresponds to the server's representation of an index on this collection. If the collection does not exist on the server, the cursor will be empty.
