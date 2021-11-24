Compilation: -$ gcc -o fsaccess fsaccess.c
Run using: -$ ./fsaccess


 This program allows user to do five things: 
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.
   3. cpin: copy the content of an external file into v6 file
   4. cpout: copy the content of a v6 file out to a new external file
   5. v6Name: set up the current working v6 file system
   6. mkdir: create the v6 directory. It should have two entries '.' and '..'
   7. remove: Delete the file v6_file from the v6 file system.
   8. cd: change working directory of the v6 file system to the v6 directory
   9. ls: display all the files in current directory
   
 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - v6Name (v6 file system name)
     - cpin (external file name) (v6 file name)
     - cpout (v6 file name) (external file name)
     - q
     - rm (v6 file)
     - mkdir (v6 directory)
     - cd (v6 directory)
     - ls
     - help
     
 File name is limited to 14 characters.