#RDB Persistance 
1. First parse the options from command line argument (initialize code ) (DONE)

# Parsing RDB file 
1. Parsing Header
2. Parse Metadata

#Expose an IO interface which will take the file provide the mechansim to read byte wise 
```
read(unsigned_size bytes):std::vector<unisgned char>
write(std::vector<unsigned char>):appends to the file
```

#TODO
Fix: Immediate RDB file and with ReplConf is creating issue ( timing).