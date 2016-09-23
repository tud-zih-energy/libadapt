# Unit tests
To use the unit tests you need to execute the test.sh script. This will compile
the libadapt and the test programm for you

Command line options for test.c:

| option    | explanation                                 |
| --------- | ------------------------------------------- |
| verbose   | verbose output                              |
| machine   | parseable output                            |
| dct       | enable dct or threads testing               |
| dvfs      | enable dvfs or frequency scaling testing    |
| file      | enable test for file writing                |
| x86_adapt | enable testing with x86_adapt library       |
| all       | enable all tests                            |

Command line options for test.sh:

| option    | explanation                                     |
| --------- | ----------------------------------------------- |
| travis    | use for travis build                            |
| testsys   | use for testsys, will enable dvfs and use logs  |
| self      | don't recompile in ../build                     |
| log       | use logs                                        |
| verbose   | verbose output                                  |
 
All other command line options will directly passed to test.c

Without any command line options a minimal test with dct and file will be executed.

The default build flags are:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes -DNO_CPUFREQ=yes ../
``````
If you want your own build make it and it will be recognized.


