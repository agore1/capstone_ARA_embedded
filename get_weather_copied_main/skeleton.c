//Skeleton code for modularized sending method
typedef enum{
	GETCMD,
	MARKCOMPLETE,
	SEND
} tx_type;

void sendMessage(tx_type type, char* data){
	//switch on which type: GETCMD, MARKCOMPLETE or SEND
	//what's required to send a message? Must be connected 

