Replication handling 
- Master client should be part of the event loop 
- Replica status should be maintained (Is ping sent|ReplConf|last part of the handshake)
- If we get response on the master (check the current status of replication)
- for master -> there will be clients connected for replication 
- for a slave -> there is a master client

Server:
    - Replication Clients 
Slave : Replication client (master)
handle_master(fd,config):
    if()
