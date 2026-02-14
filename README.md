# Architecture details 
main components 
- Eventloop
- Acceptor :  IEventHandler
- TcpConnection : IEventHandler
- MasterConnection : IEventHandler
- Client : Connection encapsulates Clients
- SlaveClient (This is confusing change this)
- IEventHandler has handle_read(),handle_write() and the associated callbacks

- On new connection on server fd, `onNewConnection` callback is called which handles all the new connection (TCP connection )creation
- Each TCP connection encapsulates the client object to which all the read data and write data is appended 
- Similarly MasterConnection encapsulates SlaveClients (confusing) and they handle the replication acceptance and propagation of commands


PUB SUB implementation:
- Subscriber subscribe to a channel 
- Publisher publish to the channel
- At most once delivery

Clients : should have modes normal mode or pub sub mode and the channels client has subscribed to
DB : should have mapping between channels and the shared clients 
When 