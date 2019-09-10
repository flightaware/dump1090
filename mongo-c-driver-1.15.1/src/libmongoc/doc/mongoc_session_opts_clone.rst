:man_page: mongoc_session_opts_clone

mongoc_session_opts_clone()
===========================

Synopsis
--------

.. code-block:: c

  mongoc_session_opt_t *
  mongoc_session_opts_clone (const mongoc_session_opt_t *opts);

Create a copy of a session options struct.

Parameters
----------

* ``opts``: A :symbol:`mongoc_session_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
