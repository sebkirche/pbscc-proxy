## commands for "svn lock" object locking strategy ##

  * `SccQueryInfo`
    * svn stat -u -v
  * `SccCheckout`
    * svn update (force)
    * svn lock
    * copy files
  * `SccUncheckout`
    * svn update (force)
    * svn unlock
  * `SccCheckin`
    * svn update (force)
    * copy files
    * svn commit
  * `SccAdd`
    * svn update (force)
    * copy files
    * svn add
    * svn commit
  * `SccGet`
    * svn update (force)
    * copy files
  * `SccRemove`
    * svn update (force)
    * svn delete
    * svn commit
  * `SccHistory`
    * tortoiseproc.exe /command:log
  * `SccDiff` (only for visual diff)
    * tortoiseproc.exe /command:diff