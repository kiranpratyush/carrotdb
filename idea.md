How to send continously the PSYNC response and the content in bulk string format
- After writing the full resync 
- load the file (for now from the disk pre configured )
- read the file and encode it 
- First think that how can you extract these replication logic outside of the main call method ( what should be the design)


This should be part of server 

- Class ReplicationManager:
    private:
        
