:man_page: bson_context_new

bson_context_new()
==================

Synopsis
--------

.. code-block:: c

  bson_context_t *
  bson_context_new (bson_context_flags_t flags);

Parameters
----------

* ``flags``: A :symbol:`bson_context_flags_t <bson_context_t>`.

The following ``flags`` may be used:
* ``BSON_CONTEXT_NONE`` meaning creating ObjectIDs with this context is not a thread-safe operation.
* ``BSON_CONTEXT_THREAD_SAFE`` meaning creating ObjectIDs with this context is a thread-safe operation.
* ``BSON_CONTEXT_DISABLE_PID_CACHE`` meaning creating ObjectIDs will also check if the process has
changed by calling ``getpid()`` on every ObjectID generation.

To use multiple flags, xor them together.

Description
-----------

Creates a new :symbol:`bson_context_t`. This is rarely needed as :symbol:`bson_context_get_default()` serves most use-cases.

Returns
-------

A newly allocated :symbol:`bson_context_t` that should be freed with :symbol:`bson_context_destroy`.

