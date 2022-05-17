:man_page: mongoc_transaction_opts_clone

mongoc_transaction_opts_clone()
===============================

Synopsis
--------

.. code-block:: c

  mongoc_transaction_opt_t *
  mongoc_transaction_opts_clone (const mongoc_transaction_opt_t *opts);

Create a copy of a transaction options struct.

Parameters
----------

* ``opts``: A :symbol:`mongoc_transaction_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
