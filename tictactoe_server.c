/*▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽▽△△
⎧===============================================================================⎫
⎪ Francesco Martina                                                             ⎪
⎪ mat. 484116                                                                   ⎪
⎪ Progetto A.A 2015/2016                                                        ⎪
⎪—————————————————————————————————————————————————————————————————————————————--⎪
⎪                                                                               ⎪
⎪ Server Side                                                                   ⎪
⎪                                                                               ⎪
⎪—————————————————————————————————————————————————————————————————————————————--⎪
⎪ © All Right Reserved                                                          ⎪
⎩===============================================================================⎭
△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△△*/


//Inclusions
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define __OSX__
#ifdef __OSX__
#include <dispatch/dispatch.h>                                              //semaphore.h is deprecated in OSX
#else
#include <semaphore.h>
#endif

///////// Macro definitions
#define NUM_THREADS		4													//pool threads number (even)
#define NUM_GAME_TABLES	NUM_THREADS/2										//create Client/2 game tables					
//#define tcp_port		5000												//main listening server TCP port
#define RX_BUFFER_SIZE	40													//size of TCP rx buffer
#define TX_BUFFER_SIZE	40													//size of TCP tx buffer
#define USERNAME_SIZE	RX_BUFFER_SIZE + 1									//20 character + EOS


//////////////////////// Var & Types //////////////////////
///////// Service types
typedef enum {DISCONNECTED, CONNECTED, WAITING, BUSY} Client_status_t;		//type for client status
typedef enum {E,X,O} Cell_t;												//type of TIC_TAC_TOE game cells, "E" for empty

///////// Structures
struct Client_struct{
    char username[40];														//username string (max 20 char)
    int socket;																//socket ID
    Client_status_t client_status;											//connection status of client
    int game_table;															//store associated game table if is busy
    Cell_t role;															//store if player is X or O
};

///////// Global variables
int port;																	//TCP port
char host[40];																//host IP or name

struct Client_struct client[NUM_THREADS];									//many threads as many clients
Cell_t game_table[NUM_GAME_TABLES][10];										//create game tables (NUM_THREADS/2 tables of nine cells + player turn)
char command_list[7+9][20];                                                 //List of all available command. 7 command + 9 !hit # command

pthread_t _main_thread;														//define main server thread
pthread_t _thread[NUM_THREADS];												//define threads pool
int actual_socket;                                                          //the last socket id

#ifdef __OSX__
dispatch_semaphore_t thread_sema;                                           //semaphore which controls thread invocation OSX
#else
sem_t thread_sema;                                                          //semaphore which controls thread invocation LINUX
#endif

pthread_cond_t conn = PTHREAD_COND_INITIALIZER;								//conditional variable for new client-thread binding
pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;						//mutex associated to conn variable
pthread_mutex_t user_data_mutex = PTHREAD_MUTEX_INITIALIZER;				//mutex for client stucture array
pthread_mutex_t game_table_mutex[NUM_GAME_TABLES];							//mutex for client stucture array

//////////////////////// Functions ////////////////////////
char server_full(){															//checks if there are free threads
    int i;
    for(i=0;i<NUM_THREADS;i++){
        pthread_mutex_lock(&user_data_mutex);          						//lock array
        if(client[i].client_status == DISCONNECTED) {                       //check free threads
            pthread_mutex_unlock(&user_data_mutex);     					//unlock array
            return 0;                                           			//server is not full
        }else{
            pthread_mutex_unlock(&user_data_mutex);     					//unlock array
        }
    }
    return 1;                                                   			//server is full
}

char username_valid(char* username){										//check if username is alredy used
    int i;
    for(i=0;i<NUM_THREADS;i++){
        pthread_mutex_lock(&user_data_mutex);
        if(!strcmp(username, client[i].username)){
            pthread_mutex_unlock(&user_data_mutex);
            return 0;
        }else{
            pthread_mutex_unlock(&user_data_mutex);
        }
    }
    return 1;
}

void init(){
    int i,j;
    
    //system var 
    actual_socket = 0;														//reset "recent" socket

    #ifdef __OSX__
    thread_sema = dispatch_semaphore_create(NUM_THREADS);                   //initialize semaphore thread (all threads free)
    #else
    sem_init(&thread_sema, 0, NUM_THREADS);                                 //      ""
    #endif

    for(i=0;i<NUM_GAME_TABLES;i++)								        	//initialize game tables mutex and conditional var for wakeup WAITING player
    	pthread_mutex_init(&game_table_mutex[i], NULL);

    //client initialization
    for(i=0;i<NUM_THREADS;i++){
        memset(&client[i].username,0,USERNAME_SIZE);;						//reset usernames
        client[i].client_status = DISCONNECTED;								//all disconnected
        client[i].socket = 0;												//means that all thread are free
        client[i].game_table = NUM_GAME_TABLES;								//no game_table
        client[i].role = E;													//no role
    }

    //game_tables init
    for(i=0;i<NUM_GAME_TABLES;i++)
    	for(j=0;j<9;j++) game_table[i][j] = E;								//empty all cells

    for(i=0;i<NUM_GAME_TABLES;i++)
    	game_table[i][9] = X;												//First turn to 'X' player

}

///////
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

char interpreter(char* command){                                            //command interpreter
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
    return -1;                                                              //return -1 if command is unknown
}
///////
///////////////////////////////////////////////////////////
/////////////////// Thread processing /////////////////////
///////////////////////////////////////////////////////////

//Main server listening thread
void* main_thread(void* id_arg){
    int ID = *((int*)id_arg);												//acquire main thread ID
    printf("Thread main is ready!\n");
    
    
    int ret;																//service var
    
    ////init tcp server
    struct sockaddr_in server_addr, client_addr;
    int main_sock, client_sock;
    
    printf("\n\n*******************************************************\n");
    printf("TIC_TAC_TOE Game, Developed by\nFrancesco Martina\nAll Rights Reserved 2016\n");
    printf("*******************************************************\n\n\n");
    printf("Main Thread Started: ID = %i\n",ID);
    printf("Opening TCP socket\n");
    main_sock = socket(PF_INET, SOCK_STREAM, 0);							//create TCP socket
    if(main_sock == -1){printf("ERROR <-- creating socket\n");exit(-1);}	//checks for errors
    
    memset(&server_addr, 0, sizeof(struct sockaddr_in));					//reset server_addr structure
    
    //structure configurations
    server_addr.sin_family = AF_INET;										//use IPv4 address
    //server_addr.sin_addr.s_addr = htonl(INADDR_ANY);						//listen on any network interface
    inet_pton(AF_INET, host, &server_addr.sin_addr);						//listen on specific host
    server_addr.sin_port = htons(port);									    //main listen port
    
    ret = bind(main_sock, (struct sockaddr*) &server_addr, sizeof(server_addr));		//bind local address to socket
    if(ret == -1){printf("ERROR <-- binding socket\n");exit(-1);}
    
    printf("Listening for clients\n");
    ret = listen(main_sock, NUM_THREADS);									//start listening
    if(ret){printf("ERROR <-- start listening\n");exit(-1);}
    
    unsigned int len;														//store size of client address structure
    len = sizeof(client_addr);
    
    while(1){
        client_sock = accept(main_sock, (struct sockaddr*) &client_addr, &len);		//waiting for connections
        if(client_sock != -1){
            
            if(server_full()){
                char tx_buffer[TX_BUFFER_SIZE];                             //define tx_buffer
                printf("Server is full, waiting for thread...\n");
                strcpy(tx_buffer,"FULL");
                send(client_sock, tx_buffer, TX_BUFFER_SIZE, 0);            //send message
            }
            
            #ifdef __OSX__
            dispatch_semaphore_wait(thread_sema,DISPATCH_TIME_FOREVER);		//waitig for free threads
            #else
            sem_wait(&thread_sema);                                         //      ""
            #endif

            printf("Connecting with client on socket %d\n", client_sock);
            pthread_mutex_lock(&conn_mutex);								//wait for others which are saving the last socket id
            actual_socket = client_sock;									//save current client
            pthread_mutex_unlock(&conn_mutex);								//unlock mutex
            pthread_cond_signal(&conn);										//wake up free thread
            
        }else{
            //TODO error
        }
    }

    close(main_sock);														//close main socket
    pthread_exit(NULL);
}



///////////////////////////////////////////////////////////
//Server Command

void _who(int *id){															//send list client and their status
	char tx_buffer[TX_BUFFER_SIZE];											//define tx_buffer
	char aux[TX_BUFFER_SIZE];												//auxiliar buffer

	printf("Client %i has requested the clients status\n",*id);				//diagnostic issue

	int i;
	pthread_mutex_lock(&user_data_mutex);
	for(i=0;i<NUM_THREADS;i++){
		if(client[i].client_status != DISCONNECTED){
			strcpy(tx_buffer,"Client: ");									//append to buffer
			send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);	        //send data
			strcpy(tx_buffer,client[i].username);							
			send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);			
			strcpy(tx_buffer,"  ");											
			switch (client[i].client_status){								//append client status
				case CONNECTED:
					strcpy(aux,"(CONNECTED)  ");
					strcat(tx_buffer,aux);
				break;
				case WAITING:
					strcpy(aux,"(WAITING)  ");
					strcat(tx_buffer,aux);		
				break;
				case BUSY:
					strcpy(aux,"(BUSY)  ");
					strcat(tx_buffer,aux);	
				break;
				case DISCONNECTED:											//never reached
				break;
			}
			send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);	        //send data
		}
	}

	strcpy(tx_buffer,"@@");													//"@@" represent the end of line	
	send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);			        //send data
	pthread_mutex_unlock(&user_data_mutex);
}

void _quit(int *id){														//send goodBye to user before disconnecting
	char tx_buffer[RX_BUFFER_SIZE];											//define tx_buffer
	strcpy(tx_buffer,"Disconnecting...Bye\n");								//append to buffer
	send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);			        //send data

	//here server doesn't send ENL because client has to quit
	//strcpy(tx_buffer,"@@");												//append to buffer EOL
	//send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);		        //send EOL
}

char cell_char(int table, int cell){
	switch(game_table[table][cell]){
		case E: return ' ';
		case O: return 'O';
		case X: return 'X';
	}
    return -1;
}



//	X|O|X
//	-----
// 	 |O|
//	-----
// 	 | |O

void _show_map(int *id){													//show game map
	char tx_buffer[TX_BUFFER_SIZE];											//define tx_buffer

	printf("Client %i has requested the game map\n",*id);					//diagnostic issue

	pthread_mutex_lock(&user_data_mutex);
	if(client[*id].client_status == BUSY){                           		//checks if player has got a map
		pthread_mutex_lock(&game_table_mutex[client[*id].game_table]);
		//send map's row
		snprintf(tx_buffer, TX_BUFFER_SIZE,"\n %c|%c|%c\n",cell_char(client[*id].game_table,6),cell_char(client[*id].game_table,7),cell_char(client[*id].game_table,8));
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send row
		snprintf(tx_buffer, TX_BUFFER_SIZE," -----\n");
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send line
		snprintf(tx_buffer, TX_BUFFER_SIZE," %c|%c|%c\n",cell_char(client[*id].game_table,3),cell_char(client[*id].game_table,4),cell_char(client[*id].game_table,5));
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send row
		snprintf(tx_buffer, TX_BUFFER_SIZE," -----\n");
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send line
		snprintf(tx_buffer, TX_BUFFER_SIZE," %c|%c|%c\n",cell_char(client[*id].game_table,0),cell_char(client[*id].game_table,1),cell_char(client[*id].game_table,2));
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send row
		snprintf(tx_buffer, TX_BUFFER_SIZE,"\n\nturn of: %c",cell_char(client[*id].game_table,9));
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send turn message
		pthread_mutex_unlock(&game_table_mutex[client[*id].game_table]);

		strcpy(tx_buffer,"@@");
		send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send end of message
	}else{
        // Never reached, controlled in client side. 
		// strcpy(tx_buffer,"You aren't playing!");
		// send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send end of message
		// strcpy(tx_buffer,"@@");
		// send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);				//send end of message
	}
	pthread_mutex_unlock(&user_data_mutex);
}


int search_free_game_table(){												//search unused gametable in user structs. It should be called in locked user_data_mutex
	int i,ret;
	char aux[NUM_GAME_TABLES];												//flags for used gametables
	memset(aux, 0, NUM_GAME_TABLES);										//reset aux flags

	for(i=0;i<NUM_THREADS;i++){												//scroll all clients
        ret=client[i].game_table;							
		if(ret!=NUM_GAME_TABLES)						                    //check for used tables
			aux[ret] = 1;													//set flag
    }

	for(i=0;i<NUM_GAME_TABLES;i++)
		if(!aux[i]) return i;												//return the first free table.

    return -1;
}

int request_opponentID(int game_table, int searcher){                       //return the opponentID
    int i;
    for(i=0;i<NUM_THREADS;i++){
        if(i==searcher)continue;                                            //exclude searcher
        if(client[i].game_table==game_table) return i;                      //return opponent id
    }
    return -1;                                                              //not found
}

void _create(int *id){														//create playground
	int i,ret;																	
	char tx_buffer[TX_BUFFER_SIZE];											//define tx_buffer
	printf("Client %i is opening a new game\n",*id);						//diagnostic issue
	pthread_mutex_lock(&user_data_mutex);
    ret = search_free_game_table();                                         //search for free game_tables
    if(ret!=-1){
    	client[*id].game_table = ret;                                           //associate new gametable
    	for(i=0;i<9;i++) game_table[client[*id].game_table][i] = E;				//clear table
    	game_table[client[*id].game_table][9] = X;								//first turn to 'X' player (who create the table. Aka ID user)
        client[*id].client_status = WAITING;                                    //player is waiting for 
    	printf("(gametable n. %i)\n", client[*id].game_table);					//diagnostic issue
    	strcpy(tx_buffer,"Playground created\n");
    	send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);					//send message
        strcpy(tx_buffer,"Wait for opponent\n");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
    	pthread_mutex_unlock(&user_data_mutex);

    	char rx_buffer[RX_BUFFER_SIZE];											     //define rx_buffer
        ret = (int)recv(client[*id].socket,rx_buffer, RX_BUFFER_SIZE, MSG_WAITALL);  //wait for wakeup @ack
        if(strcmp(rx_buffer,"@ack") || ret<=0) return;							     //unblock if client has "gently" close connection or @ack message is wrong

        pthread_mutex_lock(&user_data_mutex);
        client[request_opponentID(client[*id].game_table,*id)].client_status = BUSY;        //response to opponent request
        client[*id].role = X;													//creator starts with X role
        pthread_mutex_unlock(&user_data_mutex);

        printf("Game started: (%s) VS (%s)\n",client[*id].username,client[request_opponentID(client[*id].game_table,*id)].username);	//diagnostic issue
        strcpy(tx_buffer,"Game Started\n");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
        strcpy(tx_buffer,"It's your turn!");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
        strcpy(tx_buffer,"#");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send Game status
        strcpy(tx_buffer,"@@");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send EOM
    }else{
        pthread_mutex_unlock(&user_data_mutex);
        printf("All Gametables are currently busy!\n");
        strcpy(tx_buffer,"All Gametables are currently busy!\n");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
        strcpy(tx_buffer,"Please join another game table");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
        strcpy(tx_buffer,"@@");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send EOM
    }
}

int client_search_name(char* username, int searcher){                       //return id client with a specific username, -1 if he does not exists, exclude searcher
    int i;
    for(i=0;i<NUM_THREADS;i++){
        if(i==searcher)continue;                                            //exclude searcher
        if(!strcmp(client[i].username,username)) return i;
    }
    return -1;                                                              //not found
}

void _join(int *id){                                                        //join a match
    int opponentID;                                                         //return integer for error catching
    char rx_buffer[RX_BUFFER_SIZE];                                         //define rx_buffer
    char tx_buffer[TX_BUFFER_SIZE];                                         //define tx_buffer
    
    printf("Client %i requested a joint\n",*id);                            //debug issue

    pthread_mutex_lock(&user_data_mutex);
    if(client[*id].client_status == CONNECTED){                             //player can command join only if he is not playing (redundant, it's controlled at clinet side)
        strcpy(tx_buffer,"Insert opponent username: ");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);             //send message
        strcpy(tx_buffer,"@@");
        send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);             //send EOM
        pthread_mutex_unlock(&user_data_mutex);
        recv(client[*id].socket, rx_buffer, RX_BUFFER_SIZE, MSG_WAITALL);   //read opponent username
        pthread_mutex_lock(&user_data_mutex);
        if((opponentID=client_search_name(rx_buffer,*id))!=-1){
            if(client[opponentID].client_status==WAITING){
                strcpy(tx_buffer,"OK! Starting Game\n");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send message
                client[*id].game_table = client[opponentID].game_table;		//join table. Necessary to find this player
                client[*id].role = O;										//joiner starts with O
                client[opponentID].client_status = BUSY;                    //we are playing
                pthread_mutex_unlock(&user_data_mutex);
                strcpy(tx_buffer,"@ack");
                send(client[opponentID].socket, tx_buffer, TX_BUFFER_SIZE, 0); //wake opponent with @ack message from his socket
                strcpy(tx_buffer,"#");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send Game status
                strcpy(tx_buffer,"Waiting for opponent turn...\n");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send EOM
                return;
            }else if(client[opponentID].client_status==CONNECTED){
                strcpy(tx_buffer,"Create playground");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send message
                strcpy(tx_buffer,"to play with him!");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send message
                strcpy(tx_buffer,"@@");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send EOM
            }else{
                strcpy(tx_buffer,"Player is busy");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send message
                strcpy(tx_buffer,"@@");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);     //send EOM
            }
        }else{
            strcpy(tx_buffer,"Client not found");
            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
            strcpy(tx_buffer,"@@");
            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
        }                            

    }else{
        //never reached, controlled in client side
    }                                
    pthread_mutex_unlock(&user_data_mutex);
}

char win_check(int game_table_i){											//check if game table is won  (need mutex). Return 1 if won X, -1 if O, 2 even drawn, 0 nothing
	char win_flag[8][9];													//win configuration flag
	int i,j;

	//reset all
	for(i=0;i<8;i++)
		for(j=0;j<9;j++)
			win_flag[i][j]=0;

	//write win configurations
	win_flag[0][0]=1;win_flag[0][1]=1;win_flag[0][2]=1;
	win_flag[1][3]=1;win_flag[1][4]=1;win_flag[1][5]=1;
	win_flag[2][6]=1;win_flag[2][7]=1;win_flag[2][8]=1;
	win_flag[3][0]=1;win_flag[3][3]=1;win_flag[3][6]=1;
	win_flag[4][1]=1;win_flag[4][4]=1;win_flag[4][7]=1;
	win_flag[5][2]=1;win_flag[5][5]=1;win_flag[5][8]=1;
	win_flag[6][0]=1;win_flag[6][4]=1;win_flag[6][8]=1;
	win_flag[7][2]=1;win_flag[7][4]=1;win_flag[7][6]=1;

	char auxX,auxO,full_flag;												//full flag check if game_table is full

	full_flag = 1;
	for(i=0;i<9;i++)														//check if game table is full
		if(game_table[game_table_i][i]==E) full_flag = 0;

	for(i=0;i<8;i++){														//check if won X
		auxX = auxO = 0;
		for(j=0;j<9;j++){
			auxX += (game_table[game_table_i][j]==X) && win_flag[i][j];
			auxO += (game_table[game_table_i][j]==O) && win_flag[i][j];
		}
		if(auxX==3)return 1;
		else if(auxO==3)return -1;
	}

	if(full_flag) return 2;													//even draw
	else return 0;															//nothing
}

void _hit(int *id,int cell){												//mark cell and check wons
	char tx_buffer[TX_BUFFER_SIZE];                                         //define tx_buffer
	pthread_mutex_lock(&user_data_mutex);
	if(client[*id].client_status==BUSY){									//check if client is playing
		pthread_mutex_lock(&game_table_mutex[client[*id].game_table]);
		if(game_table[client[*id].game_table][9]==client[*id].role){		//check if it is my turn
            if(game_table[client[*id].game_table][cell]==E){
    			game_table[client[*id].game_table][cell] = client[*id].role;	//hit cell

    			switch(win_check(client[*id].game_table)){						//check if player has won
    			case 1:

    				if(client[*id].role==X){
                        printf("Game table %i finished. Client %i Won\n", client[*id].game_table, *id);                      //diagnostic issue
    					strcpy(tx_buffer,"You Won!");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,">");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
    		            strcpy(tx_buffer,"You Lost!");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,">");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
            			
                        client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                   //restore opponent client status
                        client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;                //disassociate any table
            	        client[*id].client_status = CONNECTED;							//restore client status
            			client[*id].game_table = NUM_GAME_TABLES;						//disassociate any table		


    				}else{
                        printf("Game table %i finished. Client %i Won\n", client[*id].game_table, request_opponentID(client[*id].game_table,*id));   //diagnostic issue
    					strcpy(tx_buffer,"You Lost!");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,">");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
    		            strcpy(tx_buffer,"You Won!");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,">");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message

                        client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                   //restore opponent client status
                        client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;                //disassociate any table
    		            client[*id].client_status = CONNECTED;							//restore client status
            			client[*id].game_table = NUM_GAME_TABLES;						//disassociate any table		

    				}
    			break;

    			case -1:
                   
    				if(client[*id].role==X){
                        printf("Game table %i finished. Client %i Won\n", client[*id].game_table, request_opponentID(client[*id].game_table,*id));   //diagnostic issue
    					strcpy(tx_buffer,"You Lost!");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,">");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
    		            strcpy(tx_buffer,"You Won!");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,">");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message

                        client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                   //restore opponent client status
                        client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;                //disassociate any table
    		            client[*id].client_status = CONNECTED;							//restore client status
            			client[*id].game_table = NUM_GAME_TABLES;						//disassociate any table		

    				}else{
                        printf("Game table %i finished. Client %i Won\n", client[*id].game_table, *id);                      //diagnostic issue
    					strcpy(tx_buffer,"You Won!");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,">");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
    		            strcpy(tx_buffer,"You Lost!");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,">");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message

                        client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                   //restore opponent client status
                        client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;                //disassociate any table
    		            client[*id].client_status = CONNECTED;							//restore client status
            			client[*id].game_table = NUM_GAME_TABLES;						//disassociate any table		
    				}

    			break;

    			case 2:
                        printf("Game table %i finished even draw\n", client[*id].game_table);   //diagnostic issue
    					strcpy(tx_buffer,"Finished even draw!");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,">");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
    		            strcpy(tx_buffer,"Finished even draw!");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,">");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message

                        client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                   //restore opponent client status
                        client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;                //disassociate any table
    		            client[*id].client_status = CONNECTED;							//restore client status
            			client[*id].game_table = NUM_GAME_TABLES;						//disassociate any table		

    		    break;

    			case 0:
                        printf("Client %i hitted cell %i\n", *id, cell+1);              //diagnostic issue
    					snprintf(tx_buffer, TX_BUFFER_SIZE,"Cell %i hitted!\n",cell+1); //cell+1 because first table cell is 1
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
    		            strcpy(tx_buffer,"Waiting for opponent turn...\n");
    		            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message. No EOM becaus the user have to wait

    		            snprintf(tx_buffer, TX_BUFFER_SIZE,"%s has hitted the cell %i",client[*id].username,cell+1);        //cell+1 because first table cell is 1
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
    		            strcpy(tx_buffer,"@@");
    		            send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message

    		            //change turn
    		            if(game_table[client[*id].game_table][9]==X)
    		            	game_table[client[*id].game_table][9]=O;
    		            else
    		            	game_table[client[*id].game_table][9]=X;

    		    break;
    			}

            }else{
                strcpy(tx_buffer,"Cell is not empty!");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
                strcpy(tx_buffer,"@@");
                send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
            }			

		}else{
			strcpy(tx_buffer,"It's not your turn!");
            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send message
            strcpy(tx_buffer,"@@");
            send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);         //send EOM
		}

	}

	pthread_mutex_unlock(&game_table_mutex[client[*id].game_table]);
	pthread_mutex_unlock(&user_data_mutex);	
	
}

void _disconnect(int* id){                                                  //close game match
    char tx_buffer[TX_BUFFER_SIZE];                                         //define tx_buffer
    printf("Client %i is closing match\n", *id);

    strcpy(tx_buffer,"Opponent left the game\nYou Won!");
    send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);    //send opponent message
    strcpy(tx_buffer,">");
    send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);    //send opponent end of game status
    strcpy(tx_buffer,"@@");
    send(client[request_opponentID(client[*id].game_table,*id)].socket, tx_buffer, TX_BUFFER_SIZE, 0);    //send opponent message

    strcpy(tx_buffer,"Closing match! You Lost!");
    send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send message
    strcpy(tx_buffer,">");
    send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send end of game status
    strcpy(tx_buffer,"@@");
    send(client[*id].socket, tx_buffer, TX_BUFFER_SIZE, 0);                 //send EOM

    pthread_mutex_lock(&user_data_mutex);
    client[request_opponentID(client[*id].game_table,*id)].client_status = CONNECTED;                 //restore opponent client status
    client[request_opponentID(client[*id].game_table,*id)].game_table = NUM_GAME_TABLES;              //disassociate any table
    client[*id].client_status = CONNECTED;                                  //restore client status
    client[*id].game_table = NUM_GAME_TABLES;                               //free all tables
    pthread_mutex_unlock(&user_data_mutex);

}

//////// Client processing thread
void* thread(void* id_arg){
    int ID = *((int*)id_arg);												//acquire thread id
    printf("Thread %d is ready!\n",ID);										//diagnostic issues
    
    int ret;																//return integer for error catching
    char rx_buffer[RX_BUFFER_SIZE];											//define rx_buffer
    char tx_buffer[TX_BUFFER_SIZE];											//define tx_buffer
    
    while(1){
        pthread_mutex_lock(&conn_mutex);
        while(!actual_socket)
            pthread_cond_wait(&conn, &conn_mutex);							//waiting for wakeup
        
        pthread_mutex_lock(&user_data_mutex);
        client[ID].socket = actual_socket;									//save the socket id for this thread
        pthread_mutex_unlock(&user_data_mutex);
        
        actual_socket = 0;													//reset socket, the last it as been taken by this thread
        pthread_mutex_unlock(&conn_mutex);
        
        recv(client[ID].socket, rx_buffer, RX_BUFFER_SIZE, MSG_WAITALL);	//read client username
        
        if(username_valid(rx_buffer)){										//check if username is valid
            
            printf("Client connected on socket %i -- managment thread is %i\n", client[ID].socket, ID);
            
            pthread_mutex_lock(&user_data_mutex);
            client[ID].client_status = CONNECTED;
            pthread_mutex_unlock(&user_data_mutex);
            
            strcpy(client[ID].username,rx_buffer);							//save username
            printf("Client %s registered\n",rx_buffer);
            
            strcpy(tx_buffer,"OK");
            send(client[ID].socket,tx_buffer, TX_BUFFER_SIZE, 0);	//send confirmation ("OK")

            //// here client is connected and server is ready to receive command
            int stat = 1;													// 1 if client connected, 0 if it has disconnected
            while(stat){
            	memset(rx_buffer,0,RX_BUFFER_SIZE);							//this is necessary if client "gently" disconnect
                ret = (int)recv(client[ID].socket,rx_buffer, RX_BUFFER_SIZE, MSG_WAITALL);  //read command
                
                //printf("Command %i\n", interpreter(rx_buffer));
                //printf("%s\n",rx_buffer );

                switch(interpreter(rx_buffer)){								//command interpretation
                	
                	//client size take care of  command list themselves
                	//case 0: break;										//!help


                	case 10: _who(&ID); break;								//!who  	
                	case 11: _create(&ID); break;							//!create
                    case 12: _join(&ID); break;                             //!join
                    case 13: _disconnect(&ID); break;                       //!disconnect   												     	
                	case 14: _quit(&ID); stat = 0; break;					//!quit
                	case 15: _show_map(&ID); break;							//!show_map  

                	//hit_cases
                	case 1: _hit(&ID,0); break;  							//!hit X
                	case 2: _hit(&ID,1); break; 
                	case 3: _hit(&ID,2); break; 
                	case 4: _hit(&ID,3); break; 
                	case 5: _hit(&ID,4); break; 
                	case 6: _hit(&ID,5); break; 
                	case 7: _hit(&ID,6); break; 
                	case 8: _hit(&ID,7); break; 
                	case 9: _hit(&ID,8); break;    	
                	

                	//default:
                	

                }
                
                if (!ret)                                                   //check if client has "gently" disconnected
                {
                    printf("Client %s(%i) closes connection\n", client[ID].username, ID);
                    stat = 0;
                }
            }
            ////

            
        }else{
            printf("Invalid username, alredy used\n");
            strcpy(tx_buffer,"ERROR");
            send(client[ID].socket, tx_buffer, TX_BUFFER_SIZE, 0);			//send error message ("ERROR")
        }

        close(client[ID].socket);											//close socket
        printf("Thread %i disconnected\n", ID);
        pthread_mutex_lock(&user_data_mutex);

        //reset opponent data if player closed connection while playing
        if(client[ID].client_status==BUSY){
			strcpy(tx_buffer,"Opponent left the game\nYou Won!");
            send(client[request_opponentID(client[ID].game_table,ID)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
            strcpy(tx_buffer,">");
            send(client[request_opponentID(client[ID].game_table,ID)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent end of game status
            strcpy(tx_buffer,"@@");
            send(client[request_opponentID(client[ID].game_table,ID)].socket, tx_buffer, TX_BUFFER_SIZE, 0);	//send opponent message
            client[request_opponentID(client[ID].game_table,ID)].client_status = CONNECTED;					//restore opponent client status
        	client[request_opponentID(client[ID].game_table,ID)].game_table = NUM_GAME_TABLES;				//disassociate any table
        }

        //reset all client data
        client[ID].socket = 0;												//reset socket for this client/thread
        client[ID].client_status = DISCONNECTED;
        client[ID].game_table = NUM_GAME_TABLES;							//disassociate any table

        memset(&client[ID].username,0,USERNAME_SIZE);
        pthread_mutex_unlock(&user_data_mutex);
        printf("Client %i Resetted\n", ID);

        #ifdef __OSX__
        dispatch_semaphore_signal(thread_sema);                             //thread is free
        #else
        sem_post(&thread_sema);                                             //      ""
        #endif
        
    }
    pthread_exit(NULL);
}
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////


int main(int argc, char const *argv[])
{
	//reading arguments
	if(argc!=3){
		printf("Invalid Arguments\n\n");
		return 1;
	}

	strcpy(host,argv[1]);													//read host name
	port = atoi(argv[2]);													//parse port number

    init();																	//initialization
    init_command();                                                         //command list initialization
    
    ////create thread pool
    int i,ret;
    int ids[NUM_THREADS];
    for(i=0;i<NUM_THREADS;i++){
        ids[i] = i;															//necessary to pass the correct id number
        ret = pthread_create(&_thread[i], NULL, thread,(void*)&ids[i]);		//create threads and passes id by argument
        if(ret){															//check thread creation
            printf("ERROR <-- creation of thread n. %d -- return code from pthread_create() is %d\n",i,ret);
            exit(-1);
        }
    }
    
    ////create main server thread
    i = NUM_THREADS;														//main thread as ID = NUM_THREADS (last thread + 1)
    ret = pthread_create(&_main_thread, NULL, main_thread,(void*)&i);		//create threads and passes id by argument
    if(ret){                                                    			//check thread creation
        printf("ERROR <-- creation of main thread -- return code from pthread_create() is %d\n",ret);
        exit(-1);
    }
    
    
    pthread_exit(NULL);
}