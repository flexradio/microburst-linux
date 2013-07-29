ListMP Example

Program Logic:
The GPP creates a local ListMP and adds three nodes to the ListMP. The node's
data structure includes a string buffer which contains a lowercase string. 

The slave creates its own local ListMP. The slave then opens the GPP's ListMP 
and grabs each node from the list. For each of the node's string buffer, the 
slave converts the lowercase string to uppercase. The slave then places this 
modified node on its own locally created ListMP. 

Once notified, the GPP retrieves the nodes from the slave's ListMP and prints 
the transformed string buffer.
