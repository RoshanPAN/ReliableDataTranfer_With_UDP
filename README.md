# ReliableDataTranfer With UDP
Course Project for CSE5431

The receiving folder is hardcoded! XD.
Create a "ReceivedFile" folder before you start to use.

- Run server
> ./server <port_number>
  Server will be serving through this port, and the requested file name(path) should be inside the same folder as server or be a relative path to this folder. 

- Run client (for receiving files)
> ./client <server hostname> <server portnumber> <name of file to be requested>
