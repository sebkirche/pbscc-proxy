# pbscc-proxy version 2 #

New version of the pbscc-proxy is available.

The main change in this version is how PBSCC controls object locking.

Now pbscc uses native **svn lock** ability for object locking.

However, you can use file _scc.ini_ in the root of your work-copy where you can specify the way how to manage current svn-project.

**scc.ini sample**
```
[config]
; lock strategy: 
;   "lock"  using svn lock (default from version 2.x)
;   "prop"  using svn properties (old behavior)
lock.strategy=prop
```

When you will connect to your project repository, pbscc will request you to create scc.ini file.

# note #

To switch between two lock strategies you have to remove all current locks (lockby properties and svn locks) in your svn project.

Current version 2.0.x will not display locks of other users. This will be implemented in the version 2.1.x.

The problem is that this information is not available in local work copy and must be requested from svn server and could take long time.

For correct team work everybody must use the new version





