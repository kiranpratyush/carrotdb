How to Implement Wait with blocking
- We have Replication Manager which handles all the replication mechanism
- will add blockedReplication client vectors to it as weak ptrs
- Add the next timeout to get the minimum timeout the epoll should wait
- add handle_wait method in the replication manager (for now write 0 in the write buffer of the client)



#TODO
- Move the command parsing outside of each handle , remove the dependency of current read /write position




#Current resource I am studying to refactor the current implementation
# next implement the acceptor connection type which will only have the handle_read ( on new connection call back)
