HeapBufMP Example

Program Logic:
The GPP creates a local HeapBufMP and also allocates a block of memory from the
HeapBufMP to be used as a string buffer. The slave opens the remote HeapBufMP
and also allocates a block of memory from the HeapBufMP to be used as a string
buffer. The GPP writes a lowercase string to the string buffer and sends the
buffer's translated shared region address to the slave. The slave copies the
string buffer into its own string buffer and converts the lowercase string to
uppercase. The slave then sends its buffer's shared region address GPP. The
GPP then prints the uppercase string.
