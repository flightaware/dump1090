:man_page: mongoc_database_find_collections

mongoc_database_find_collections()
==================================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_database_find_collections (mongoc_database_t *database,
                                    const bson_t *filter,
                                    bson_error_t *error)
     BSON_GNUC_DEPRECATED_FOR (mongoc_database_find_collections_with_opts);

Deprecated
----------

This function is deprecated and should not be used in new code.

Please use :symbol:`mongoc_database_find_collections_with_opts()` instead.

Description
-----------

Fetches a cursor containing documents, each corresponding to a collection on this database.

.. include:: includes/retryable-read.txt

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.
* ``filter``: A matcher used by the server to filter the returned collections. May be ``NULL``.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

A cursor where each result corresponds to the server's representation of a collection in this database.

