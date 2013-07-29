RingIO Example

Program Logic:
The GPP creates a local RingIO and also opens the newly created RingIO in 
writer mode. 

The slave opens the GPP's RingIO in reader mode.

The GPP copies a file (represented as an ASCII string) to the RingIO using an 
acquire size of 12. A file header attribute is set for the first acquire of 
the RingIO. The attribute contains a payload which specifies the size of the 
file being transfered. On the final acquire an end of file attribute is 
set.

The slave reads data from the RingIO using an acquire size of 16. The slave 
checks for both the file header and end of file attribute. The slave keeps 
track of the amount of data that has been acquired. It then stops when the 
amount of data acquired is equal to the file size passed by the file header
attribute.

Important Changes

Changes needed for cores running SYS/BIOS:

Unlike the majority of examples the RingIO module isn't defined within IPC
but from within SysLink. Therefore, SysLink_setup must be called right after
Ipc_start. SysLink_destroy should be called before Ipc_stop.

The header #include <ti/syslink/SysLink.h> is required to be included in the 
file that calls SysLink_setup and SysLink_destroy.

The inclusion of SysLink_setup and SysLink_destroy requires the heap size to 
be increased for this example. 

The heap size defined in the slave's cfg is increased set to
heapMemParams.size = 0x20000; to accommodate the heap requirement due to the
addition of SysLink_setup and SysLink_destroy. 

