#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <stdbool.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> 
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define BUFLEN 512	// Tamanho do buffer

void erro(char *s);
void udp( int argc, char *argv[]);
void tcp(int argc, char *argv[]);
void initialize_pp_shm();
bool pp_aproved(char * info);
bool mc_aproved(char* info);
void get_username_ip_port_pp(char* info,  char username_sender[], char ip[], char port[]);
void get_username_ip_port_mc(char* info, char username_sender[],char* ip, char* port);
void reset_ppc();
void udp_send(int s, struct sockaddr_in si_outra, socklen_t slen);
void udp_recv(int s, struct sockaddr_in si_outra, socklen_t slen);


struct pp_connection{
	bool pp_req;
	bool pp_val;
	bool mc_req;
	bool mc_val;
	char ip[20];
	char port[20];
	char username[20];
};

int shmid;
struct pp_connection * ppc;
sem_t *shm_mutex;

//./cliente <IP Servidor> <porto> 
int main(int argc, char *argv[]){

	if(strcmp(argv[2],"80")==0){
		tcp(argc, argv);
	}
	else if (strcmp(argv[2],"9876")==0){
		udp(argc, argv);
	}
	else{
		printf("Wrong Port\n");
	}
	return 0;
}

void udp( int argc, char *argv[]){
	initialize_pp_shm();

	sem_unlink("mutex");
	shm_mutex=sem_open("mutex", O_CREAT|O_EXCL, 0700,1);

	struct sockaddr_in si_minha, si_outra;
	char endServer[100];
	int s;
	struct hostent *hostPtr;
	socklen_t slen = sizeof(si_minha);

	if(argc != 3) {
		printf("Erro: ./cliente <IP Servidor> <porto> \n");
		exit(1);
	}

	// Servidor
	strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
 		erro("Nao consegui obter endereço");

	
	// Cria um socket para recepção de pacotes UDP
	if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		erro("Erro na criação do socket");
	}


	
	if(fork()==0){ 
		
		udp_recv(s,si_outra,slen);
		
		exit(0);
	}
	else{
		if(fork()==0){

			bzero((void *) &si_outra,sizeof(si_outra));
		  	si_outra.sin_family = AF_INET;
			si_outra.sin_port = htons(atoi(argv[2]));
			si_outra.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
			
			udp_send(s,si_outra,slen);
			
			exit(0);
		}

	}

	for(int i = 0; i < 2; i++){
		wait(NULL);
	}

	sem_close(shm_mutex);
	sem_unlink("mutex");
	shmdt(ppc);
	shmctl(shmid, IPC_RMID, NULL);

	// Fecha socket e termina programa
	close(s);
}

void udp_send(int s, struct sockaddr_in si_outra, socklen_t slen){

	char buf[300];	
	char ppbuf[BUFLEN];
	

	strcpy(buf,"hello\n");
	if(sendto(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra,slen) == -1) {
			erro("Erro no sendto");
	}

	while(1){
		
		fgets(buf,BUFLEN,stdin);
		buf[strcspn(buf,"\n")] = 0;
		printf("\n");	
		
		if(strcmp(buf,"2")==0){
			sem_wait(shm_mutex);
			ppc->pp_req = true;
			sem_post(shm_mutex);
		}

		if(strcmp(buf, "3")==0){
			sem_wait(shm_mutex);
			ppc->mc_req = true;
			sem_post(shm_mutex);
		}

		if(ppc->pp_val){
			// Envio de mensagem

			struct sockaddr_in si_pp;
			memset(&si_pp, 0, sizeof(si_pp));
			socklen_t slen_pp = sizeof(si_pp);

			si_pp.sin_family = AF_INET;
			si_pp.sin_port = htons(atoi(ppc->port));
			si_pp.sin_addr.s_addr = inet_addr(ppc->ip);

			sprintf(ppbuf,"$$PRIVATE MESSAGE FROM %s:\n$$%s\n",ppc->username,buf);
			if(sendto(s, ppbuf, BUFLEN, 0, (struct sockaddr *) &si_pp ,slen_pp) == -1) {
			erro("Erro no sendto pp");
			}

			printf("$$PP MESSAGE HAS BEEN SENT\n");

			reset_ppc();

			strcpy(buf,"rejoin");
			if(sendto(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, slen) == -1) {
			erro("Erro no sendto");
			}
		}
		
		else if(ppc->mc_val){

			char buf_mc[500];

			int sock;
			struct sockaddr_in addr;

			memset(&addr,0,sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(atoi(ppc->port));
			addr.sin_addr.s_addr = inet_addr(ppc->ip);
			
			sock = socket(AF_INET, SOCK_DGRAM, 0);

			int multicastTTL = 254;
			if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL,
			    sizeof(multicastTTL)) < 0) {
			    erro("socket opt");
			}


			sprintf(buf_mc,"$$%s -> %s",ppc->username,buf);
		    // Envio de mensagem
			if(sendto(sock, buf_mc, BUFLEN, 0, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
			erro("Erro no sendto");
			}
		
			//printf("$$MC MESSAGE HAS BEEN SENT\n");

			if(strcmp(buf,"$$QUIT")==0){

				reset_ppc();

				strcpy(buf,"rejoin");
				if(sendto(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, slen) == -1) {
				erro("Erro no sendto");
				}
			}

		}
		else{

    		// Envio de mensagem
			if(sendto(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, slen) == -1) {
			erro("Erro no sendto");
			}

			if(strcmp(buf, "4")==0){
				break;
			}
		}
	}
}

void udp_recv(int s,struct sockaddr_in si_outra, socklen_t slen){

	char buf_rec[BUFLEN];
	char buf_rec_copy[BUFLEN];
	int recv_len; 

	while(1){

    	if(strcmp(buf_rec,"##SERVER - GOODBYE\n")==0){
			break;
		}
	
    	if(!ppc->pp_val){
	    	if(ppc->pp_req && pp_aproved(buf_rec_copy)){

	    		sem_wait(shm_mutex);
	    		get_username_ip_port_pp(buf_rec,ppc->username,ppc->ip,ppc->port);
				ppc->pp_val = true;
				sem_post(shm_mutex);
	    	}
	    }
	   	if(!ppc->mc_val){
	   		char bc[BUFLEN];
	   		strcpy(bc,buf_rec_copy);
			if(ppc->mc_req && mc_aproved(bc)){
				sem_wait(shm_mutex);
				get_username_ip_port_mc(buf_rec_copy,ppc->username,ppc->ip,ppc->port);
				ppc->mc_val = true;
				sem_post(shm_mutex);
				printf("$$YOU ARE TALKIN OVER MULTICAST ON THE IP AND PORT: %s %s\n$$ TO LEAVE WRITE $$QUIT\n",ppc->ip,ppc->port);
			}
		}	

		if(ppc->mc_val){

			int fd = socket(AF_INET, SOCK_DGRAM, 0);
		    
		    u_int yes = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){	
	       		erro("Reusing ADDR failed");
    		}

    		struct sockaddr_in addr;
		    memset(&addr, 0, sizeof(addr));
		    addr.sin_family = AF_INET;
		    addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
		    addr.sin_port = htons(atoi(ppc->port));

		    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		        erro("bind");
    		}

    		struct ip_mreq mreq;
		    mreq.imr_multiaddr.s_addr = inet_addr(ppc->ip);
		    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0){
		        erro("setsockopt");
		    }

		    
		    socklen_t addrlen = sizeof(addr);
		    if((recv_len =recvfrom(fd, buf_rec, BUFLEN, 0, (struct sockaddr *) &addr,(socklen_t *) &addrlen)) == -1) {
			  erro("Erro no recvfrom");
			}

			buf_rec[recv_len] = '\0';
	    	printf("%s\n",buf_rec);
	    	strcpy(buf_rec_copy,buf_rec);
	    	


		}
		else{

			if((recv_len =recvfrom(s, buf_rec, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1) {
			  erro("Erro no recvfrom");
			}

			buf_rec[recv_len] = '\0';
	    	printf("%s\n",buf_rec);
	    	strcpy(buf_rec_copy,buf_rec);
		}
						
	}
}

void tcp(int argc, char *argv[]){
	char endServer[100];
	int fd;
	struct sockaddr_in addr;
	struct hostent *hostPtr;
	int nread = 0;
	char buffer[BUFLEN];buffer[nread] = '\0';
	char buf[BUFLEN];


	if (argc != 3) {
		printf("cliente <host> <port>\n");
		exit(-1);
	}
	strcpy(endServer, argv[1]);
	if ((hostPtr = gethostbyname(endServer)) == 0)
		erro("Não consegui obter endereço");

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons((short) atoi(argv[2]));

	if ((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	  erro("socket");
	if (connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
	  erro("Connect");

	nread = read(fd,buffer,BUFLEN-1);
	buffer[nread] = '\0';
	printf("%s", buffer);

	while(strcmp(buffer,"##SERVER - LOGING OUT\n")){

		fgets(buf,BUFLEN,stdin);
		buf[strcspn(buf,"\n")] = 0;

		write(fd,buf,1+strlen(buf));

		nread = read(fd,buffer,BUFLEN-1);
		buffer[nread] = '\0';
		printf("%s", buffer);

	}
	
	close(fd);

	
	exit(0);
}

void erro(char *s) {
	perror(s);
	exit(1);
}

void initialize_pp_shm(){

	int size = sizeof(struct pp_connection);
    if((shmid=shmget(IPC_PRIVATE, size ,IPC_CREAT | 0766))<0){
        perror("Error in shmget with IPC_GREAT\n");
        exit(1);
    }

    if((ppc = (struct pp_connection *) shmat(shmid,NULL,0))==(struct pp_connection*)-1){
        perror("Shmat error!");
        exit(1);
    }

    ppc->pp_req = false;
    ppc->pp_val = false;
    ppc->mc_req = false;
    ppc->mc_val = false;
}

bool pp_aproved(char * info){
	char* token;
    token = strtok(info, "-");
    if(strcmp("##SERVER LEAVING ",token)==0){
    	return true;
    }
   return false;
}

void get_username_ip_port_pp( char* info,  char username_sender[], char ip[], char port[]){
    char* token;
    token = strtok(info, " ");
    for(int i = 0; i< 4; i++){
        token = strtok(NULL, " ");
    }
    strcpy(username_sender, token);
    for(int i = 0; i< 6; i++){
        token = strtok(NULL, " ");
    }
    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    strcpy(ip, token);
    token = strtok(NULL, " ");
    strcpy(port, token);
}

void reset_ppc(){
	sem_wait(shm_mutex);
	ppc->pp_val = false;
	ppc->pp_req = false;
	ppc->mc_req = false;
    ppc->mc_val = false;
	strcpy(ppc->ip,"");
	strcpy(ppc->port,"");
	strcpy(ppc->username,"");
	sem_post(shm_mutex);
}

void get_username_ip_port_mc(char* info,char username[], char* ip, char* port){
    char* token;
    token = strtok(info, " ");
    for(int i = 0; i<11; i++){
    	if(i==3){
    		strcpy(username, token);
    	}
        token = strtok(NULL, " ");
    }
    strcpy(ip, token);
    token = strtok(NULL, " ");
    strcpy(port, token);
}

bool mc_aproved(char* info){
    char* token;
    token = strtok(info, " ");
    for(int i = 0;i<3;i++){
        token = strtok(NULL, " ");
    }
    if(strcmp(token, "DONT")!=0){
        return true;
    }
    return false;
}

