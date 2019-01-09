/*▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽△△
⎧===============================================================================⎫
⎪ Francesco Martina                                                             ⎪
⎪ mat. 484116                                                                   ⎪
⎪ Progetto A.A 2015/2016                                                        ⎪
⎪—————————————————————————————————————————————————————————————————————————————--⎪
⎪                                                                               ⎪
⎪ Client Side                                                                   ⎪
⎪                                                                               ⎪
⎪—————————————————————————————————————————————————————————————————————————————--⎪
⎪ © All Right Reserved                                                          ⎪
⎩===============================================================================⎭
△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△*/


//Inclusions
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

///////// Macro definitions
//#define tcp_port		5000											//TCP port
//#define IP 				"127.0.0.1"

#define RX_BUFFER_SIZE	40												//size of TCP rx buffer
#define TX_BUFFER_SIZE	40												//size of TCP tx buffer

//////////////////////// Var & Types //////////////////////

///////// Global variables
int port;																//TCP port
char host[40];															//host IP or name
int sock;																//connection socket
char rx_buffer[RX_BUFFER_SIZE];											//define rx_buffer
char tx_buffer[TX_BUFFER_SIZE];											//define tx_buffer
int ret;																//General Purpose return variable
enum {MENU,GAME,DISCONNECTED} status;									//0 in menu, 1 if game is running, 2 if server has disconnected

//////////////////////// Functions ////////////////////////
void print_command_list(){
	printf("Sono disponibili i seguenti  comandi:\n\t* !help -->  mostra  l'elenco  dei  comandi  disponibili\n\t* !who --> mostra l'elenco dei client connessi al server\n\t* !create --> crea una nuova partita e attendi un avversario\n\t* !join --> unisciti ad una partita e inizia a giocare\n\t* !disconnect --> disconnetti il client dall'attuale partita\n\t* !quit --> disconnetti il client dal server\n\t* !show_map --> mostra la mappa di gioco\n\t* !hit num_cell --> marca la casella num_cell\n\n");
}

//print ">" simbol
void prompt(){
	printf(">");
}

//print "#" simbol
void hashtag(){
	printf("#");
}

//read line from terminal and delete '\n' at the end
void reader(){
	char* p;
	fgets(tx_buffer, TX_BUFFER_SIZE, stdin);							//read command
	p=strchr(tx_buffer,'\n');
	*p='\0';															//subtitute '\n' with '\0'
}

//print reply message from server
void receiver(){
	do{
		ret = (int)recv(sock,rx_buffer, RX_BUFFER_SIZE, MSG_WAITALL);
		//command by server (all mutual exclusive)
		if(!strcmp(rx_buffer,"#")) status = GAME; else					//set client status to game
		if(!strcmp(rx_buffer,">")) status = MENU; else					//set client status to normal menu
		if(!strcmp(rx_buffer,"@@")) {printf("\n"); return;} else		//message has ended
		if(!strcmp(rx_buffer,"@ack")) send(sock, rx_buffer, RX_BUFFER_SIZE, 0);	//resend @ack

		else
		if(ret>0)printf("%s", rx_buffer);								//append message

	}while(ret>0);

	status = DISCONNECTED;
	printf("\n\n");
	return;																//server has disconnected
}


///////
char command_list[7+9][20];												//7 command + 9 !hit # command
void init_command(){
    //command initialization
    strcpy(command_list[0],"!help");
    strcpy(command_list[10],"!who");
    strcpy(command_list[11],"!create");
    strcpy(command_list[12],"!join");
    strcpy(command_list[13],"!disconnect");
    strcpy(command_list[14],"!quit");
    strcpy(command_list[15],"!show_map");

    //!hit command needs a number
    //(one digit number does not justify heavy parsing)
    strcpy(command_list[1],"!hit 1");
    strcpy(command_list[2],"!hit 2");
    strcpy(command_list[3],"!hit 3");
    strcpy(command_list[4],"!hit 4");
    strcpy(command_list[5],"!hit 5");
    strcpy(command_list[6],"!hit 6");
    strcpy(command_list[7],"!hit 7");
    strcpy(command_list[8],"!hit 8");
    strcpy(command_list[9],"!hit 9");
}

char interpreter(char* command){                               			//command interpreter
    if(!strcmp(command,command_list[0])) return 0;
    if(!strcmp(command,command_list[1])) return 1;
    if(!strcmp(command,command_list[2])) return 2;
    if(!strcmp(command,command_list[3])) return 3;
    if(!strcmp(command,command_list[4])) return 4;
    if(!strcmp(command,command_list[5])) return 5;
    if(!strcmp(command,command_list[6])) return 6;
    if(!strcmp(command,command_list[7])) return 7;
    if(!strcmp(command,command_list[8])) return 8;
    if(!strcmp(command,command_list[9])) return 9;
    if(!strcmp(command,command_list[10])) return 10;
    if(!strcmp(command,command_list[11])) return 11;
    if(!strcmp(command,command_list[12])) return 12;
    if(!strcmp(command,command_list[13])) return 13;
    if(!strcmp(command,command_list[14])) return 14;
    if(!strcmp(command,command_list[15])) return 15;
    return -1;                                                  		//return -1 if command is unknown
}
///////

int main(int argc, char const *argv[])
{
	//reading arguments
	if(argc!=3){
		printf("Invalid Arguments\n\n");
		return 1;
	}

	strcpy(host,argv[1]);												//read host name
	port = atoi(argv[2]);												//parse port number

	init_command();
	printf("\n***************************************************\n\n");
	printf("Connecting to server: %s\t port: %i\n",host,port);

//init tcp server
	struct sockaddr_in server_addr; 

	printf("Opening TCP socket\n");
	sock = socket(PF_INET, SOCK_STREAM, 0);								//create TCP socket
	if(sock == -1){														//checks for errors
		printf("ERROR <-- creating socket\n");
		exit(-1);
	}	

	memset(&server_addr, 0, sizeof(struct sockaddr_in));				//reset server_addr structure

	//structure configurations
	server_addr.sin_family = AF_INET;									//use IPv4 address	
	server_addr.sin_port = htons(port);									//main listen port
	ret = inet_pton(AF_INET, host, &server_addr.sin_addr);

	ret = connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr));	//connect server
	if(ret){
		printf("ERROR <-- connection. Unable to reach Server\n");
		exit(-1);
	}
	printf("Connected!\n\n");


	printf("Insert your username: \n");									//asking credentials
	reader();															//read username

	send(sock, tx_buffer, TX_BUFFER_SIZE, 0);							//send username
	recv(sock, rx_buffer , RX_BUFFER_SIZE , MSG_WAITALL);
	if(!strcmp(rx_buffer,"OK")){
		printf("Connected with server and registered!\n");
	}else if(!strcmp(rx_buffer,"ERROR")){
		printf("Username alredy used!\n"); return 1;
	}else if(!strcmp(rx_buffer,"FULL")){
		printf("Server is full, waiting for thread...\n");
		recv(sock, rx_buffer , RX_BUFFER_SIZE , MSG_WAITALL);
		if(strcmp(rx_buffer,"OK")) return 1;
		printf("Connected with server and registered!\n");
	}

	print_command_list();
	while(1){
		if(status == MENU){
			prompt();													//write command insert '>'
			reader();													//read command
			switch (interpreter(tx_buffer)){
				case 0:	print_command_list(); break;					//!help stamps command list

				case 10:												//!who
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;

				case 11:												//!create
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;

				case 12:												//!join
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				reader();												//read command
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;

				case 14:												//!quit
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;

				//hit # commands
				case 1: printf("This command works only while you are playing\n");	break;	
				case 2: printf("This command works only while you are playing\n");	break;
				case 3: printf("This command works only while you are playing\n");	break;
				case 4: printf("This command works only while you are playing\n");	break;
				case 5: printf("This command works only while you are playing\n");	break;
				case 6: printf("This command works only while you are playing\n");	break;
				case 7: printf("This command works only while you are playing\n");	break;
				case 8: printf("This command works only while you are playing\n");	break;
				case 9: printf("This command works only while you are playing\n");	break;
				//
				case 13: printf("This command works only while you are playing\n");	break;	//!disconnect
				case 15: printf("This command works only while you are playing\n");	break;	//!show_map
				case 16: printf("This command works only while you are playing\n");	break;	//!hit
				default: printf("Unknown command, retry\n"); break;
			}

		}else if(status == GAME){
			hashtag();													//write command insert '>'
			reader();													//read command
			switch (interpreter(tx_buffer)){
				case 0:	print_command_list(); break;					//!help stamps command list
				
				case 1:case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:		//hit cells
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;
				
				case 13:												//!disconnect
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;

				case 15:												//!show_map
				send(sock, tx_buffer, TX_BUFFER_SIZE, 0);				//send command
				receiver();												//print text sent by server and check connection
				break;
				

				case 10: printf("This command works only when you are not playing\n"); break;	//!who
				case 11: printf("This command works only when you are not playing\n"); break;	//!create
				case 12: printf("This command works only when you are not playing\n"); break;	//!join
				case 14: printf("This command works only when you are not playing\n"); break;	//!quit
				default: printf("Unknown command, retry\n"); break;
			}


		}else{															//aka. status = DISCONNECTED
			break;														//quitting
		}

		}
	
	close(sock);

	return 0;
}