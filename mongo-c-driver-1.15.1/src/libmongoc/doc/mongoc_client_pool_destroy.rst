:man_page: mongoc_client_pool_destroy

mongoc_client_pool_destroy()
============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_pool_destroy (mongoc_client_pool_t *pool);

Release all resources associated with ``pool``, including freeing the structure.

This method is NOT thread safe, and must only be called by one thread. It should be called once the application is shutting down, and after all other threads that use clients have been joined.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.

