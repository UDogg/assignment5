#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

#define PACKET_SIZE 264

/*
 * REQUEST FORMAT::
 * uint16_t length;
 * uint32_t op_code;
 * uint16_t return_code;
 * uint32_t block[JBOD_BLOCK_SIZE];
*/

struct jbod_packet_t
{
	uint16_t length;
	uint32_t op;
	uint16_t ret;
	uint8_t block[JBOD_BLOCK_SIZE];
};

typedef struct jbod_packet_t jbod_packet_t;

jbod_packet_t packet;

//function to allocate memory to req pointer which contains contents of packet.
uint8_t *create_req(uint32_t op, uint8_t *block)
{
	packet.op = htonl(op);
	packet.ret = htons(0);

	if (block != NULL)
		memcpy(packet.block, block, JBOD_BLOCK_SIZE*sizeof(uint8_t));

	packet.length = sizeof(packet);
	packet.length = htons(packet.length);

	uint8_t *req = (uint8_t *)malloc(packet.length);
	memcpy(req, &packet, packet.length);

	return req;
}


int cli_sd = -1;

//read function
static bool nread(int fd, int len, uint8_t *buf)
{
	if (read(fd, buf, len) < 0)
	{
		/*reads len bytes from file descriptor to buf
  		read returns -1 when it fails*/
		perror("RESPONSE ERROR...");
		return false;
	}
 	return true;
}

//write function
static bool nwrite(int fd, int len, uint8_t *buf)
{
	if (write(fd, buf, len) < 0)
	{
		/*writes len bytes from buffer to file descriptor*/
		perror("ERROR WHILE REQUESTING SERVER...");
		return false;
	}
	return true;
}



//receive packet function
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block)
{
	uint8_t buffer[packet.length];

	if (!nread(fd, packet.length, buffer))
	{
		perror("RESPONSE ERROR...");
		return false;
	}
	/*implementing nread inside this 
  	so that receive acts like read*/
	int ptr = sizeof(uint16_t);
	memcpy(op, &buffer[ptr], sizeof(uint32_t));
	*op = ntohl(*op);

	ptr += sizeof(uint32_t);
	memcpy(ret, &buffer[ptr], sizeof(uint16_t));
	*ret = ntohs(*ret);

	ptr += sizeof(uint16_t);
	if (block != NULL)
		memcpy(block, &buffer[ptr], JBOD_BLOCK_SIZE*sizeof(uint8_t));
	//sets values of op, ret, and block according to incoming packets
	return true;
}


//send packet function
static bool send_packet(int sd, uint32_t op, uint8_t *block)
{
	uint8_t *req = create_req(op, block);
	//creates req pointer so that it can read packet length bytes from req pointer to sd
  	//send also acts as write
	if (!nwrite(sd, packet.length, req))
	{
		perror("ERROR WHILE REQUESTING SERVER...");
		return false;
	}
	return true;
}


//as shown in the assignment video
bool jbod_connect(const char *ip, uint16_t port)
{
	//make a socket for connection
	struct sockaddr_in server_addr;

	cli_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (cli_sd < 0)
	{
		perror("ERROR CREATING A SOCKET");
		close(cli_sd);
		cli_sd = -1;
		return false;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(JBOD_PORT);
	server_addr.sin_addr.s_addr = inet_addr(JBOD_SERVER);

	if (connect(cli_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("ERROR CONNECTING TO SERVER");
		close(cli_sd);
		cli_sd = -1;
		return false;
	}

	return true;
}

//disconnect function
void jbod_disconnect(void)
{	
	//closes the socket and resets socket descriptor
	close(cli_sd);
	cli_sd = -1;
}



//returns values similar to jbod_operation()
int jbod_client_operation(uint32_t op, uint8_t *block)
{
	if (!send_packet(cli_sd, op, block))
	{
		perror("FAILED TO REQUEST SERVER");
		return -1;
	}

	uint16_t ret;
	if (!recv_packet(cli_sd, &op, &ret, block))
	{
		perror("RESPONSE FAILED");
		return -1;
	}

	return 0;
}
