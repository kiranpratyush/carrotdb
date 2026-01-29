# Progress on Redis implementation
## Current Implementation 
- Full RDB synchronization

What the heck is a stream in general 
- Streams are sequence of bytes that flows between the program and I/O devices 
- stream classes in c++ provide an abstraction (Gives same interface) to interact with I/O devices
- I have to use IfStream and OfStream to read and write data to and fro from the file




- Just figure out to encode into the RDB file format and send it 
