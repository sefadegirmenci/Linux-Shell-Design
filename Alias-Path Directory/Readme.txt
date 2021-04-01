In shell.c file, line 9 contains PATH definition. This path should be changed to wherever you want the short directory text file to be stored. We could directly use /home/ but we are not sure that every computer has that folder, or permissions. Also relative path cannot be used because seashell travels around directories. So writing a specific path is left to user.

Apart from that, there is not another trick.
Makefile can be used or,
Compilation: gcc -o shell shell.c
Run: ./shell
It will bring you to seashell screen, you can type all of the commands i.e. shortdir set,shortdir list,shortdir jump