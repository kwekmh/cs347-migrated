# migrated

To compile the **migrated** binary into the *bin/* subdirectory, you can run the following command:

```
make
```

If you encounter an error that says `No such file or directory`, you can either run the daemon as root (`sudo bin/migrated`) or create the directory */var/migrated* and ensure that the user the daemon is running under has both read and write permissions to that directory.
