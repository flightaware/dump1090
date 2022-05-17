:man_page: mongoc_client_pool_pop

mongoc_client_pool_pop()
========================

Synopsis
--------

.. code-block:: c

  mongoc_client_t *
  mongoc_client_pool_pop (mongoc_client_pool_t *pool);

Retrieve a :symbol:`mongoc_client_t` from the client pool, or create one. The total number of clients that can be created from this pool is limited by the URI option "maxPoolSize", default 100. If this number of clients has been created and all are in use, ``mongoc_client_pool_pop`` blocks until another thread returns a client with :symbol:`mongoc_client_pool_push`.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.

Returns
-------

A :symbol:`mongoc_client_t`.

.. include:: includes/mongoc_client_pool_thread_safe.txt
