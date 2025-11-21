Full file list:

-makefile:builds executable
-commands.txt: essential for code to run. Input file

-main.c //Handles file parsing as well as all of the inputs

-hash.c //responsible for actually building the underlying hash table
-hash.h

-timestamp.c // contains a function that returns a current time stamp
-timestamp.h

-workers.c // Handles calling the operations in the input. 
-workers.h // also handles semaphores waiting and awaking

-ops.c //house the actual operational process for acquiring and releasing locks given an input
-ops.h

-locks.c // contains the lock functions like acquiring, releasing, read, write, etc
-locks.h

-log.c // logging the condition of the semaphores.
-log.h

Use "make" to compile the executable (hashlogger.exe)
use "make run" to compile and run the executable (requires a comannds.txt file)
Use "make clean" to delete all object files as well as the output files output.txt and hash.log