# v6_file_system

  
 This program allows user to do five things: 
   1. openfs: open the file 
   2. initfs: Initilizes the file system 
   3. cpin: copy the content of an external file into v6 file
   4. cpout: copy the content of a v6 file out to a new external file
   5. mkdir: create the v6 directory. It should have two entries '.' and '..'
   6. remove: Delete the file v6_file from the v6 file system.
   7. cd: change working directory of the v6 file system to the v6 directory
   8. Quit: save all work and exit the program.

 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - v6Name (v6 file system name)
     - cpin (external file name) (v6 file name)
     - cpout (v6 file name) (external file name)
     - q
     - rm (v6 file)
     - mkdir (v6 directory)
     - cd (v6 directory)
     
 File name is limited to 28 characters.
 *******************************************
To run the code, execute following commands:

```
gcc -o mod-v6 mod-v6.c utility.c```

```
./mod-v6
```