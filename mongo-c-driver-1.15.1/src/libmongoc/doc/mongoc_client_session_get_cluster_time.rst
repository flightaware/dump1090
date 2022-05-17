:man_page: mongoc_client_session_get_cluster_time

mongoc_client_session_get_cluster_time()
========================================

Synopsis
--------

.. code-block:: c

  const bson_t *
  mongoc_client_session_get_cluster_time (const mongoc_client_session_t *session);

Get the session's clusterTime, as a BSON document.

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.

Returns
-------

A :symbol:`bson:bson_t` you must not modify or free. If the session has not been used for any operation and you have not called :symbol:`mongoc_client_session_advance_cluster_time`, then the returned value is NULL.

.. only:: html

  .. taglist:: See Also:
    :tags: session
