Notify Example

Program Logic:
The GPP sends either an App_CMD_INC_PAYLOAD or App_CMD_DEC_PAYLOAD command which
includes a 16 bit payload. Depending on the command received the slave increases
or decreases the attached payload and replies with an App_CMD_OP_COMPLETE 
command that includes the updated payload. This continues until the GPP sends an
App_CMD_SHUTDOWN command which results in both cores executing their cleanup 
procedure. 

