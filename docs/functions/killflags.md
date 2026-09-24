# killflags

currents underdeveloped and mostly used for debugging in lightweight editors or text editors

# flags
```_
1. -silent ..doesnt push the default exit details
2. -debug ..lightweight debugger for lightweights like vscode or vim/text editors
3. -force ..will guarentee termination but is NOT SAFE
4. -bypass ..allows functions other than main to be able to kill a program
5. -cleanup ..ensure all data is clean before killing with a 0
6. -success ..similar to -cleanup
7. -panic 
8. -error
9. -wait ..waits, similar to -cleanup
10. -flush ..similar to -cleanup
11. -r"" ..r meaning reason
```
## debug subflags

these flags can only work/be activated without and error if -debug is included FIRST in the kill flag path

```_
1. -compiletime
2. -runtime
3. -libsused
4. -filelen
5. -log: file.txt
```
### log 



## using multiple flags

# numerics

for those familiar with c, you may prefer typical numerical convensions without the access to the debugger.
numerics and named flags may conflict but can be used together 
```_
1. 0 ..clean run
2. 1 ..script failure
3. 2+ ..error codes
4. -1 ..internal error
5. -2+ ..system termination
```