#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#define PORT 8080
#define MAXIMUM_PEEPS	100

static unsigned int counter_for_clients = 0;
static int indentification_for_user = 10;

typedef struct {
	struct sockaddr_in addr;	/* Client remote addressesess */
	int file_descriptor_connection;			/* Connection file descriptor */
	int indentification_for_user;			/* Client unique identifier */
	char name[32];			/* Client name */
} client_t;

client_t *clients[MAXIMUM_PEEPS];

void add_to_queue(client_t *cl){
	int i;
	for(i=0;i<MAXIMUM_PEEPS;i++){
		if(!clients[i]){
			clients[i] = cl;
			return;
		}
	}
}

void send_message(char *s, int indentification_for_user){
	int i;
	for(i=0;i<MAXIMUM_PEEPS;i++){
		if(clients[i]){
			if(clients[i]->indentification_for_user != indentification_for_user){
				write(clients[i]->file_descriptor_connection, s, strlen(s));
			}
		}
	}
}

void send_message_to_client(char *s, int indentification_for_user){
	int i;
	for(i=0;i<MAXIMUM_PEEPS;i++){
		if(clients[i]){
			if(clients[i]->indentification_for_user == indentification_for_user){
				write(clients[i]->file_descriptor_connection, s, strlen(s));
			}
		}
	}
}

/* Strip CRLF */
void carriage_return_manager(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}
void output_client_addresses(struct cli_addr addresses){
	printf("%d.%d.%d.%d",
		addresses.sin_addresses.s_addresses & 0xFF,
		(addresses.sin_addresses.s_addresses & 0xFF00)>>8,
		(addresses.sin_addresses.s_addresses & 0xFF0000)>>16,
		(addresses.sin_addresses.s_addresses & 0xFF000000)>>24);
}
void message_clients_currently_online(int file_descriptor_connection){
	int i;
	char s[64];
	for(i=0;i<MAXIMUM_PEEPS;i++){
		if(clients[i]){
			sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->indentification_for_user, clients[i]->name);
			message_me(s, file_descriptor_connection);
		}
	}
}
void message_me(const char *s, int file_descriptor_connection){
	write(file_descriptor_connection, s, strlen(s));
}
void send_message_all(char *s){
	int i;
	for(i=0;i<MAXIMUM_PEEPS;i++){
		if(clients[i]){
			write(clients[i]->file_descriptor_connection, s, strlen(s));
		}
	}
}

void *client_handler(void *argv){
	char buff_out[1024];
	char buff_in[1024];
	int rlen;

	counter_for_clients++;
	client_t *cli = (client_t *)argv;

	printf("<<ACCEPT ");
	output_client_addresses(cli->addresses);
	printf(" REFERENCED BY %d\n", cli->indentification_for_user);

	sprintf(buff_out, "<<JOIN, HELLO %s\r\n", cli->name);
	send_message_all(buff_out);

	/* Receive input from client */
	while((rlen = read(cli->file_descriptor_connection, buff_in, sizeof(buff_in)-1)) > 0){
	        buff_in[rlen] = '\0';
	        buff_out[0] = '\0';
		carriage_return_manager(buff_in);

		/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}
	
		/* Special options */
		if(buff_in[0] == '\\'){
			char *command, *param;
			command = strtok(buff_in," ");
			if(!strcmp(command, "\\Q")){
				break;
			}else if(!strcmp(command, "\\NAME")){
				param = strtok(NULL, " ");
				if(param){
					char *old_name = strdup(cli->name);
					strcpy(cli->name, param);
					sprintf(buff_out, "<<RENAME, %s TO %s\r\n", old_name, cli->name);
					free(old_name);
					send_message_all(buff_out);
				}else{
					message_me("<<NAME CANNOT BE NULL\r\n", cli->file_descriptor_connection);
				}
			}else if(!strcmp(command, "\\Private")){
				param = strtok(NULL, " ");
				if(param){
					int indentification_for_user = atoi(param);
					param = strtok(NULL, " ");
					if(param){
						sprintf(buff_out, "[PM][%s]", cli->name);
						while(param != NULL){
							strcat(buff_out, " ");
							strcat(buff_out, param);
							param = strtok(NULL, " ");
						}
						strcat(buff_out, "\r\n");
						send_message_to_client(buff_out, indentification_for_user);
					}else{
						message_me("<<MESSAGE CANNOT BE NULL\r\n", cli->file_descriptor_connection);
					}
			}else if(!strcmp(command, "\\ActiveClients")){
				sprintf(buff_out, "<<CLIENTS %d\r\n", counter_for_clients);
				message_me(buff_out, cli->file_descriptor_connection);
				message_clients_currently_online(cli->file_descriptor_connection);
			}else if(!strcmp(command, "\\Help")){
				strcat(buff_out, "\\Q    \\Q chatroom\r\n");
				strcat(buff_out, "\\P     Server test\r\n");
				strcat(buff_out, "\\Name   <name> Change servername\r\n");
				strcat(buff_out, "\\Private  <reference> <message> Send private message\r\n");
				strcat(buff_out, "\\ActiveClients   Show ActiveClients clients\r\n");
				strcat(buff_out, "\\Help     Show Help\r\n");
				message_me(buff_out, cli->file_descriptor_connection);
			}else{
				message_me("<<UNKOWN COMMAND\r\n", cli->file_descriptor_connection);
			}
		}else{
			/* Send message */
			sprintf(buff_out, "[%s] %s\r\n", cli->name, buff_in);
			send_message(buff_out, cli->indentification_for_user);
		}
	}
}
}


  
int main(int argvc, char const *argv[])
{
    char string[256];
    int listenfd = 0, file_descriptor_connection = 0;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
    char message = scanf("%s", string);
    char *hello = &message;
    char buffer[1024] = {0};
    while(1){
		socklen_t clilen = sizeof(cli_addr);
		file_descriptor_connection = accept(listenfd, (struct sockaddresses*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((counter_for_clients+1) == MAXIMUM_PEEPS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			output_client_addressesess(cli_addr);
			printf("\n");
			close(file_descriptor_connection);
			continue;
		}

    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    


    // Convert IPv4 and IPv6 addressesesses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addresses)<=0) 
    {
        printf("\nInvalid addressesess/ addressesess not supported \n");
        return -1;
    }
  
    if (connect(sock, (struct sockaddresses *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    send(sock , hello , strlen(hello) , 0 );
    valread = read( sock , buffer, 1024);
    printf("%s\n",buffer );
}
   
   return 0;
}



