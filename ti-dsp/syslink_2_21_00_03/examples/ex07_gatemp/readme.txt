GateMP Example

Program Logic:
The GPP creates a UInt variable in a Shared Region and also creates a local 
GateMP. The slave opens a handle to the GPP's GateMP.

The GPP gives the UInt variable an initial value and then sends its shared
region pointer to the slave. The slave translates the received shared region
pointer to its local address space. 

The slave enters the GateMP, alters the shared region UInt variable and then 
exits the GateMP. This process occurs 10 times. The GPP simultaneously enters 
the GateMP, reads the shared region UInt variable and then exits the GateMP. 
This process is also repeated 10 times.
