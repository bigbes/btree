-------------------------------------------------------------------------------
                      Sophia backend for HW1 (fall14.BDB) API
-------------------------------------------------------------------------------

1. Compiling:

   .. code-block:: bash

        git clone git://github.com/bigbes/fall14BDB.git --recursive
        cd fall14BDB/example
        make all

2. Testing:

   .. code-block:: bash

        make gen_workload # If no WL was generated before
                          # Run if you think that this backend
                          # is reliable
                          # (Sophia is reliable backend ;) )
        make runner

    If no assertion was thrown, then your DB is ok
