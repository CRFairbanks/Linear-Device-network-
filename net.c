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

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  for (int i = 0; i < len; i++) {
    int n = read(fd, &buf[i], (len-i));
    if (n < 0) { return false; }    // negative read failure
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
for (int i = 0; i < len; i++) {
    int n = write(fd, &buf[i], (len-i));
    if (n < 0) { return false; }    // negative write failure
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint16_t len;
  uint8_t header[HEADER_LEN];
  if (!nread(fd, HEADER_LEN, header)){ return false; } // ENSURES READ SUCCESS

  int offset = 0;
  memcpy(&len, header, sizeof(len));
  offset += sizeof(len);
  memcpy(op, (header + offset), sizeof(*op));
  offset +=  sizeof(*op);
  memcpy(ret, (header + offset), sizeof(*ret));

  len = ntohs(len);                 // NET BYTE TO HOST BYTE
  *op = ntohl(*op);
  *ret = htons(*ret);

  if (len != 8) { 
    if (!nread(fd, JBOD_BLOCK_SIZE, block)){ return false; } 
  }

  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint16_t len = HEADER_LEN;
  uint8_t buffer[HEADER_LEN + JBOD_BLOCK_SIZE];

  if ((op >> 26)== JBOD_WRITE_BLOCK) { len += 256; } // WILL ENSURE THE CMD IS WRITE

  
  uint16_t len2 = ntohs(len);
  uint32_t op2  = htonl(op);
  int op2size = sizeof(op2);
  int len2size = sizeof(len2);
  int offset = 0;

  memcpy(buffer, &len2, len2size);          // NN...
  offset =+ len2size;
  memcpy(buffer + offset, &op2, op2size); // NNOOOO...
  offset += op2size;
  offset += 2; // NNOOOO__... (essentially header_len but for consistency..)

  if (op >> 26 == JBOD_WRITE_BLOCK) { memcpy(buffer + offset, block, JBOD_BLOCK_SIZE); }

  return (nwrite(sd, len, buffer));
}


/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);

  if(inet_aton(ip, &saddr.sin_addr) == 0) { return false; }
  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if(cli_sd == -1) { return false; }

  if(connect(cli_sd, (const struct sockaddr *)&saddr, sizeof(saddr)) == -1){
    return false;
  }
  else { return true; }
}


/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  printf("");
  if(!(send_packet(cli_sd, op, block))) { return -1; } 
  uint16_t ret = 0;
  if(!(recv_packet(cli_sd, &op, &ret, block))) { return -1; } 
  return ret;

}