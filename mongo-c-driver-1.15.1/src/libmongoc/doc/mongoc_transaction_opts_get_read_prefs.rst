:man_page: mongoc_transaction_opts_get_read_prefs

mongoc_transaction_opts_get_read_prefs()
========================================

Synopsis
--------

.. code-block:: c

  const mongoc_read_prefs_t *
  mongoc_transaction_opts_get_read_prefs (const mongoc_transaction_opt_t *opts);

Return the transaction options' :symbol:`mongoc_read_prefs_t`. The returned value is valid only for the lifetime of ``opts``. See :symbol:`mongoc_transaction_opts_set_read_prefs()`.

Parameters
----------

* ``opts``: A :symbol:`mongoc_transaction_opt_t`.

.. only:: html

  .. taglist:: See Also:
    :tags: session
