:man_page: mongoc_client_reset

mongoc_client_reset()
=====================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_reset (mongoc_client_t *client);

Causes the client to clear its session pool without sending endSessions, and to close all its connections. Call this method in the child after forking.

This method increments an internal generation counter on the given client. After this method is called, cursors from previous generations will not issue a killCursors command when they are destroyed.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.

