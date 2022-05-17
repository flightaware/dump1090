:man_page: mongoc_client_session_get_client

mongoc_client_session_get_client()
==================================

Synopsis
--------

.. code-block:: c

  mongoc_client_t *
  mongoc_client_session_get_client (const mongoc_client_session_t *session);

Returns the :symbol:`mongoc_client_t` from which this session was created with :symbol:`mongoc_client_start_session()`.

Parameters
----------

* ``session``: A :symbol:`mongoc_client_session_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
