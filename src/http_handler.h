#ifndef HTTP_HANDLER_H  
#define HTTP_HANDLER_H  

#include <netinet/in.h>  

void handle_client(int client_socket, struct sockaddr_in *client_addr);  
const char* get_mime_type(const char *filepath);  

#endif