#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <stdbool.h> // for bool function
#include <signal.h>

//#define IP "10.90.0.1"
#define BUFLEN 512  // Tamanho do buffer


int fd, client, server_pid;
struct user{
  char username[100];
  char ip[100];
  int port;
  char password[100];
  bool pp, ps, mc; // pear pear, pear server, multicast
  int state;
  struct sockaddr_in si;
  socklen_t slen;
};

struct message{
    struct user recv;
    char message[BUFLEN];
    bool valid;
};

int UDP_PORT;
int TCP_PORT;
char CONFIG_FILE [10];

struct user * users_online;
char MULTICAST[200];
int number_of_users;
int s;
int fd, client;
int server_pid;

void erro(char *msg);
int myatoi(char* str);
bool verify(char* str);
void sigint(int signum);
bool seek_password(char * buffer, char * linha);
void seek_name(char *buffer, char* linha);
int seek_user_state(struct sockaddr_in si_minha,  socklen_t slen );
int seek_user_idx_trought_ip(char *ip);
int seek_user_idx_trought_name(char* name);
int numUsers(char *nome);
void getuser(char *buffer, int i);
void initialize(char *file_name);
char* menu(struct user newuser,char* m,char * some_message);
char* udp_client(int state, char info[], char ip[]);
void tcp_admin(int admin_fd);
char* connection_type(int op, int u_idx);
int user_is_online(struct user this);
struct message get_sm_trought_ui(char * info);
void send_message_ps(struct message msg,int u_idx);
void send_pp_info(struct user pp_req,struct user other_u);
void admin_menu(char* m, char *some_message);
void admin_add(char* info, char* filename);
void admin_del(char* name, char* filename);
void admin_list(char* info, char*filename);
int countWords(char* str);
void udp();
void tcp();


int main(int argc, char *argv[]) {

  if(argc != 4) {
    printf("Erro: ./server <Port Client> <Port Config> <Configuration File> \n");
    exit(1);
  }

  signal(SIGINT,sigint);

  UDP_PORT = myatoi(argv[1]);
  TCP_PORT = myatoi(argv[2]);
  strcpy(CONFIG_FILE,argv[3]);

  server_pid = getpid();
  number_of_users = numUsers(CONFIG_FILE);
  users_online = malloc(sizeof(struct user)*number_of_users);

  initialize(CONFIG_FILE);

  if(fork() == 0){
    tcp();
    exit(0);
  }
  udp();

  wait(NULL);
  
}

// ---- udp ---- //

void udp(){
  
  struct sockaddr_in si_minha, si_outra;
  int recv_len,send_len;
  socklen_t slen = sizeof(si_outra);
  char buf[BUFLEN];
  char buf_envio[BUFLEN];

  // Cria um socket para recepção de pacotes UDP
  if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    erro("Erro na criação do socket");
  }

  // Preenchimento da socket address structure
  si_minha.sin_family = AF_INET;
  si_minha.sin_port = htons(UDP_PORT);
  si_minha.sin_addr.s_addr = inet_addr("10.90.0.1");//INADDR_ANY

  // Associa o socket à informação de endereço
  if(bind(s,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
    erro("Erro no bind");
  }

  
  printf("SERVER RECV UDP CONNECTIOS ON:  %s %d \n",inet_ntoa(si_minha.sin_addr),UDP_PORT );

  while(1){
  // Espera recepção de mensagem
    if((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1) {
      erro("Erro no recvfrom");
    }
    buf[recv_len]='\0';
    printf("%s\n",buf);
    printf("## message from %s %d\n", inet_ntoa(si_outra.sin_addr), ntohs(si_outra.sin_port));

    int state = seek_user_state(si_outra,slen);
    printf("%d--\n",state);
    char* option; 
    option = udp_client(state,buf,inet_ntoa(si_outra.sin_addr));

    strcpy(buf_envio,option);
    if((send_len = sendto(s, buf_envio, strlen(buf_envio) ,0, (struct sockaddr *) &si_outra,slen)) == -1) {
          erro("Erro no sendto");
    }
  }
  // Fecha socket e termina programa
  close(s);
}

char* udp_client(int state, char info[], char ip[]){

  int u_idx = seek_user_idx_trought_ip(ip);; // user index in array
  int u_idx_send;
  int connect_op; // connection number 
  char m [500]; // menu

  if(state == 1){ //request username
    users_online[u_idx].state += 1;
    return "##SERVER - USERMANE:\n";
  }
    
  else if (state == 2){ // recv username // req pass
    if(strcmp(users_online[u_idx].username,info)==0){
      users_online[u_idx].state += 1;
      return "##SERVER - PASSWORD:\n";
    }
    else{
      return "##SERVER - INVALID USERNAME:\n##SERVER - USERMANE:\n";
    }
  }

  else if (state == 3){ // recv pass // send menu
    if(strcmp(users_online[u_idx].password,info)==0){
      users_online[u_idx].state += 1;
      char m [1000];
      return menu(users_online[u_idx],m," ");
    }
    else{
      return "##SERVER - INVALID PASS:\n##SERVER - PASSWORD:\n";
    }
  }

  else if(state == 4){ // recv type of connection // send info of connection
    if(verify(info)){
      connect_op = myatoi(info);
      return connection_type(connect_op, u_idx);
    }
    else{
      return menu(users_online[u_idx],m,"##SERVER - WRONG COMMAND TRY AGAIN");
    }
  }

  else if(state == 5){ // type is PS , receive name of destination send message
    if(countWords(info) >= 3){
      struct message msg = get_sm_trought_ui(info);
      if(msg.valid){
        if(user_is_online(msg.recv)==1){
          send_message_ps(msg, u_idx);
          users_online[u_idx].state = 4;
          return menu(users_online[u_idx],m,"##SERVER - MESSAGE HAS BEEN SENT");
        }
        else{
          users_online[u_idx].state = 4;
          return menu(users_online[u_idx],m,"##SERVER - THIS USER IS NOT ONLINE");
        }
      }
    }
    else{
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - WRONG COMMAND TRY AGAIN");
    }
  }

  else if(state == 6){ // type is PP, receiveing name and making stuff
    u_idx_send = seek_user_idx_trought_name(info);
    if(u_idx_send != -1){
      if(user_is_online(users_online[u_idx_send])==1){
        if(users_online[u_idx_send].pp){
          users_online[u_idx].state = 7;
          printf("mudei o esatdo no rejoin mando menu\n");
          send_pp_info(users_online[u_idx],users_online[u_idx_send]);
          return "$$WRITE YOUR MESSAGE:\n";
        }
        else{
          users_online[u_idx].state = 4;
          return menu(users_online[u_idx],m,"##SERVER - USER DOES NOT HAVE PERMISSION TO PERFORM PP CONNECTION");
        }
      }
      else{
        users_online[u_idx].state = 4;
        return menu(users_online[u_idx],m,"##SERVER - USER IS NOT ONLINE");
      }
    }
    else{
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - USER DOEST NOT EXIST");
    }
  }

  else if(state == 7){ // pp message has been sent, send // wait for rejoin
    if(strcmp("rejoin",info)==0){
      printf("vou mandar o menu\n");
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - WELCOME BACK");
    }
  }
 
  return "##YOU DONT BELONG HERE\n";
}

void send_message_ps(struct message msg, int u_idx){
  char mens[1000];
  strcat(msg.message,"\n");
  sprintf(mens,"## SERVER - MESSAGE FROM %s : \n## %s",users_online[u_idx].username,msg.message);
  printf("## sending  message from some client to %s %d\n", msg.recv.ip,msg.recv.port);
  if(sendto(s, mens , BUFLEN, 0, (struct sockaddr *) &msg.recv.si, msg.recv.slen) == -1) {
      erro("Erro no sendto");
  }
}

void send_pp_info(struct user pp_req,struct user other_u){
  char mens[1000];
  sprintf(mens,"##SERVER LEAVING - YOU %s ARE NOW CONNECTED PP TO %s FROM %s %d \n",pp_req.username ,other_u.username,other_u.ip,other_u.port);
  if(sendto(s, mens , BUFLEN, 0, (struct sockaddr *) &pp_req.si, pp_req.slen) == -1) {
      erro("Erro no sendto");
  }
}

char* connection_type(int op, int u_idx){
  struct user this_user = users_online[u_idx];
  char m [500]; // menu

  if(op == 1){
    if(this_user.ps){
      users_online[u_idx].state = 5;
      return "##SERVER - YOU HAVE CHOSEN CLIENT-SERVER\n##         TO SEND A MESSAGE DO AS FOLLOWS:\n##         USERNAME OF DESTINATION -> YOUR MESSAGE\n\n";
    }
    else{
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - YOU DONT HAVE ACESS");
    }
  }

  if(op == 2){
    if(this_user.pp){
      users_online[u_idx].state = 6;
      return "##SERVER - YOU HAVE CHOSEN CLIENT-CLIENT\n##         WHAT IS THE NAME OF THE USER YOU WISH TO SEND A MESSAGE:\n";
    }
    else{
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - YOU DONT HAVE ACESS");
    }

  }

  if(op == 3){
    users_online[u_idx].state = 7;
    if(this_user.mc){
      char MC_IP [20] = "239.0.0.1";
      int MC_PORT = 4003;
      sprintf(MULTICAST,"##SERVER - YOU %s HAVE CHOSEN MULTICAST\n##         MULTICAST IP AND PORT: %s %d\n",this_user.username,MC_IP,MC_PORT);
      return MULTICAST;
    }
    else{
      users_online[u_idx].state = 4;
      return menu(users_online[u_idx],m,"##SERVER - YOU DONT HAVE ACESS");
    }

  }

  if(op == 4){
    users_online[u_idx].state = 0;
    printf("user %s disconnected\n",users_online[u_idx].username);
    return "##SERVER - GOODBYE\n";
  }

  else{
    users_online[u_idx].state = 4;
    return menu(users_online[u_idx],m,"##SERVER - YOU DONT HAVE ACESS");
  }


  return "##SERVER - SOMETHING WENT WRONG\n";
}

char* menu(struct user newuser, char* m , char * some_message){
  char option1 [100],option2 [100],option3 [100];
  if(newuser.ps){
    sprintf(option1,"1 | CLIENT SERVER CONNECTION\n");
  }
  else{
     sprintf(option1,"1 | CLIENT SERVER CONNECTION ( YOU DO NOT HAVE ACCESS TO THIS CONNECTION)\n");
  }
  if(newuser.pp){
    sprintf(option2,"2 | CLIENT TO CLIENT CONNECTION\n");
  }
  else{
     sprintf(option2,"2 | CLIENT TO CLIENT CONNECTION ( YOU DO NOT HAVE ACCESS TO THIS CONNECTION)\n");
  }
  if(newuser.mc){
    sprintf(option3,"3 | CLIENT TO GROUP CONNECTION\n");
  }
  else{
     sprintf(option3,"3 | CLIENT TO GROUP CONNECTION ( YOU DO NOT HAVE ACCESS TO THIS CONNECTION)\n");
  }

  
  sprintf(m,"%s\n\n        --- | CHOOSE THE NUMBER OF THE CONNECTION | ---\n\n%s%s%s\n4 | LEAVE SERVER\n\n",some_message,option1,option2,option3);
  

  return m; 
}

// --- tcp --- //

void tcp(){

  struct sockaddr_in addr, client_addr;
  int client_addr_size;

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("10.90.0.1");
  addr.sin_port = htons(TCP_PORT);

  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  erro("na funcao socket");

  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  // CTRL C can cause server to enter a time wait state, to unsure no loss of data
  //SO_REUSEADDR allows server to bind to an address which is in a TIME_WAIT state.

  
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
  erro("na funcao bind");
  if( listen(fd, 5) < 0)
  erro("na funcao listen");
  client_addr_size = sizeof(client_addr);

 
  printf("SERVER RECV TCP CONNECTIOS ON:  %s %d \n",inet_ntoa(addr.sin_addr),TCP_PORT );

  while (1) {
    //clean finished child processes, avoiding zombies
    //must use WNOHANG or would block whenever a child process was working
    while(waitpid(-1,NULL,WNOHANG)>0);
    //wait for new connection
    client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    if (client > 0) {
      if (fork() == 0) {
        close(fd);
        tcp_admin(client);
        exit(0);
      }
    close(client);
    }
  }
}

void tcp_admin(int admin_fd){

  int nread = 0;
  char buffer[BUFLEN];
  int option = 0;
  admin_menu(buffer, " ");
  write(admin_fd, buffer, 1+strlen(buffer));

  nread = read(admin_fd, buffer, BUFLEN-1); // get option
  buffer[nread] = '\0';

  while(option!=4){

    if(verify(buffer)){
      option = myatoi(buffer);
      if(option == 1){
        char users[600];
        char users_list[500];
        
        admin_list(users_list,CONFIG_FILE);
        sprintf(users,"## SERVER - THIS ARE THE USERS:\n\n          %s",users_list);
        admin_menu(buffer, users);
        write(admin_fd, buffer, 1+strlen(buffer));      

        strcpy(users_list,"");
      }
      else if(option == 2){
        sprintf(buffer,"\n## SERVER - TO ADD A USER DO AS FOLLOWS:\n##          USER_ID:IP:PASSWORD:CS:PP:MC\n");
        write(admin_fd, buffer, 1+strlen(buffer));
        
        nread = read(admin_fd, buffer, BUFLEN-1); // get option
        buffer[nread] = '\0';

        if(countWords(buffer)==6){
          admin_add(buffer,CONFIG_FILE);
  
          admin_menu(buffer, "##SERVER - USER ADDED");
          write(admin_fd, buffer, 1+strlen(buffer));
        }
        else{
          admin_menu(buffer, "##SERVER - WRONG COMMAND");
          write(admin_fd, buffer, 1+strlen(buffer));
        }
        

      }
      else if(option == 3){
        strcpy(buffer,"\n## SERVER - USERNAME OF THE USER YOU WISH DO REMOVE:\n");
        write(admin_fd, buffer, 1+strlen(buffer));

        nread = read(admin_fd, buffer, BUFLEN-1); // get option
        buffer[nread] = '\0';
        admin_del(buffer,CONFIG_FILE);

        admin_menu(buffer, "##SERVER - USER REMOVED");
        write(admin_fd, buffer, 1+strlen(buffer));

      }

      else if(option == 4){
        strcpy(buffer,"##SERVER - LOGING OUT\n");
        write(admin_fd, buffer, 1+strlen(buffer));
        break;

      }
    }

    else{

      admin_menu(buffer, "##SERVER - WRONG COMMAND");
      write(admin_fd, buffer, 1+strlen(buffer));

    }

    nread = read(admin_fd, buffer, BUFLEN-1); // get option
    buffer[nread] = '\0';
  }

  close(admin_fd);
}

void admin_menu(char* m, char *some_message){
   sprintf(m,"\n%s\n\n        --- | CHOOSE THE NUMBER OF THE CONNECTION | ---\n1 | LIST USERS\n2 | ADD USER\n3 | DELETE USER\n4 | QUIT\n\n", some_message);
}

void admin_add(char* info, char* filename){
    FILE *fo = fopen(filename, "a");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    fputs("\n", fo);
    fputs(info, fo);
    fclose(fo);
}

void admin_del(char* name, char* filename){
    int count = 0;
    FILE *fo = fopen(filename, "r");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    FILE *fp = fopen("temp.txt", "w");
    if(fp==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    char line[256], line2[256];
    char *token;
    while(!feof(fo)){
        strcpy(line, "\0");
        strcpy(line2, "\0");
        fgets(line, 256, fo);
        strcpy(line2, line);
        token = strtok(line2, ":");
        if(strcmp(name, token)!=0){
            fputs(line, fp);
            count++;
        }
    }
    fclose(fo);
    fclose(fp);
    FILE *fd = fopen("temp.txt", "r");
    if(fd==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    FILE *fc = fopen(filename, "w");
    if(fc==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    char *token2;
    while(count!=1){
        strcpy(line, "\0");
        fgets(line, 256, fd);
        fputs(line, fc);
        count--;
    }
    strcpy(line, "\0");
    fgets(line, 256, fd);
    token2 = strtok(line, "\n");
    fputs(token2, fc);
    fclose(fc);
    fclose(fd);
}

void admin_list(char* info, char*filename){

    FILE *fo = fopen(filename, "r");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
        strcpy(info,"\0");
    char line[50];
    while(!feof(fo)){
        strcpy(line, "\0");
        fgets(line, 50, fo);
        strcat(info, line);
    }
    fclose(fo);
}

// --- seeks --- //

int seek_user_state(struct sockaddr_in si_minha,  socklen_t slen ){
  char ip [20];
  strcpy(ip,inet_ntoa(si_minha.sin_addr));
  int port = ntohs(si_minha.sin_port);
  for(int i=0; i <number_of_users;i++ ){
    if(strcmp(users_online[i].ip,ip)==0){
      if(users_online[i].state == 0){
        users_online[i].state = 1;
        users_online[i].port = port;
        users_online[i].si = si_minha;
        users_online[i].slen = slen;
      } 
      return users_online[i].state;
    }
  }
  return -1; // user is not in users database
}

int seek_user_idx_trought_ip(char *ip){
    for(int i = 0; i < number_of_users; i++){
        if(strcmp(users_online[i].ip, ip)==0)
            return i;
    }
    return -1;
}

int seek_user_idx_trought_name(char* name){
  for(int i = 0; i < number_of_users; i++){
    if(strcmp(name,users_online[i].username)==0){  
        return i;
    }
  }
  return -1;
}

void seek_name(char *buffer, char* linha){
    FILE *fo = fopen(CONFIG_FILE, "r");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    char *name, linha2[100];
    while(fgets(linha, 100, fo)!=NULL){
        strcpy(linha2, linha);
        name = strtok(linha2, ":");
        if(strcmp(name, buffer)==0){
            fclose(fo);
            return;
        }
    }
    fclose(fo);
    strcpy(linha, "invalid_user");
}

bool seek_password(char * buffer, char * linha){
    char *password;
    password = strtok(linha, ":");
    password = strtok(NULL, ":");
    password = strtok(NULL, ":");
    if(strcmp(buffer, password)==0)
        return true;
    return false;
}


// --- gets --- //

struct message get_sm_trought_ui(char * info){ // get struct message with info from users input
    struct message dest;
    char *token;
    token = strtok(info, " ");
    int id = seek_user_idx_trought_name(token);
    if(id != -1){
      dest.recv = users_online[id];
      token = strtok(NULL, " ");
      token = strtok(NULL, "");
      strcpy(dest.message, token);
      dest.valid = true;
      return dest;
    }
    else{
      dest.valid = false;
      return dest;
    }
}

void getuser(char *buffer, int i){
    char *token;
    token = strtok(buffer, ":");
    strcpy(users_online[i].username, token);
    token = strtok(NULL, ":");
    strcpy(users_online[i].ip, token);
    token = strtok(NULL, ":");
    strcpy(users_online[i].password, token);
    token = strtok(NULL, ":");
    if(strcmp(token, "yes")==0)
        users_online[i].ps = true;
    else
        users_online[i].ps = false;
    token = strtok(NULL, ":");
    if(strcmp(token, "yes")==0)
        users_online[i].pp = true;
    else
        users_online[i].pp = false;
    token = strtok(NULL, ":");
    if(strcmp(token, "yes\n")==0) {
        users_online[i].mc = true;
        
    }
    else
        users_online[i].mc = false;
    users_online[i].state = 0;
    users_online[i].port = 0;
}

// --- other --- //


void initialize(char *file_name){
    FILE *fo = fopen(file_name, "r");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return;
    }
    char linha[100];
    int i = 0;
    while(fgets(linha, 100, fo)!=NULL){
        getuser(linha, i);
        i++;
    }
    fclose(fo);
}

int numUsers(char *nome){
    int count = 0;
    FILE *fo = fopen(nome, "r");
    if(fo==NULL){
        fprintf(stderr, "Erro ao abrir o ficheiro de origem\n");
        return 0;
    }
    char linha[100];
    while(fgets(linha, 100, fo)!=NULL)
        count++;
    fclose(fo);
    return count;
}

int user_is_online(struct user this){
  for(int i = 0; i < number_of_users; i++){
    if(strcmp(this.username,users_online[i].username)==0){
      if(users_online[i].state != 0){
        return 1; // user is online
      }
      else{
        return 2; // user is not online
      }
    }
  }
  return 0; // poop has happened
}

void sigint(int signum){
  if(getpid() == server_pid){
    close(s);
    printf("\nSERVER : CLOSED\n");
    wait(NULL);
    exit(0);
  }
  close(fd);
  close(client);
  exit(0);
}

bool verify(char* str){
  int i = 0; // index of str

  while(str[i] != '\0'){ // condition to see end of string
    if(str[i] <= 57 && str[i] >= 48){ // condition to see if value is within numeric range
      i++;
    }
    else
      return false; // Error (char does not belong within range of integer values)
  }
  return true;
}

int myatoi(char* str){ // Self-made atoi (Convert string into int)
    int i = 0; // index of str
    int res = 0; // converted number
    while(str[i] != '\0'){ // condition to see end of string
        res = res * 10 + str[i] - '0'; // previous 'res' is multiplied by 10, and the difference between
        i++;                           // the char of str and the char '0' gives the corresponding int number
      }
    return res;
}

int countWords(char* str){
    int state = 0;
    int wc = 0;
    while (*str){
        if (*str == ' ' || *str == '\n' || *str == '\t'|| *str == ':')
            state = 0;
        else if (state == 0){
            state = 1;
            ++wc;
        }
        ++str;
    }
    return wc;
}

void erro(char *msg){
  printf("Erro: %s\n", msg);
  exit(-1);
}