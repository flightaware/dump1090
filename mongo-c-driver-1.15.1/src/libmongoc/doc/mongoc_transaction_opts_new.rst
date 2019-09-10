:man_page: mongoc_transaction_opts_new

mongoc_transaction_opts_new()
=============================

Synopsis
--------

.. code-block:: c

  mongoc_transaction_opt_t *
  mongoc_transaction_opts_new (void);

Create a :symbol:`mongoc_transaction_opt_t` to configure multi-document transactions. The struct must be freed with :symbol:`mongoc_transaction_opts_destroy`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
