#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/udp.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<string.h> /* memset */
#include<unistd.h> /* close */
#include<time.h>

typedef int bool;
#define true 1
#define false 0
#define BUFSIZE 101
#define PACKET_SIZE 100
#define DEFAULT_PORT 3000
#define PORT 67
#define SOCKET_ERROR -1
#define SOCKET_READ_TIMEOUT_SEC 2
#define LIMITE 10

unsigned long packets_to_send;


void kill(char *msg){
    perror(msg);
    exit(1);
}

unsigned long fsize(FILE **f){
    fseek(*f, 0, SEEK_END);
    unsigned long len = (unsigned long)ftell(*f);
    rewind(*f);
    return len;
}


int copy_size(unsigned long file_size, int send_qtd){
    if(file_size > PACKET_SIZE * send_qtd)
        return PACKET_SIZE;
    else
        return PACKET_SIZE + file_size - (PACKET_SIZE*send_qtd);
}


int hasContent(){
    if(packets_to_send == 0)
        return 0;
    packets_to_send--;
    return 1;
}


unsigned long packets_counter(unsigned long file_size){
    return (file_size/PACKET_SIZE) + 1;
}
char ack(char atual){
    if (atual == '1')
        return '0';
    return '1';
}
int main(int argc, char *argv[]){

    if(argc != 4){
        kill("Missing arguments! <filename><ip><port>");
    }

    int sock, rv, packs_to_expire = 0, on = 1;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    unsigned long file_size, start = 0, size, packets_send = 0;
    char atual_ack = '0';
    struct sockaddr_in my_address, other_address;
    FILE *fd;
    char buffer[BUFSIZE];
    bool enable_send = true;
    ssize_t received;
    fd_set set;
    struct timeval timeout;

   
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1)
        kill("Socket error!");

    
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

   
    memset((char *)&my_address, 0, addr_len);
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(DEFAULT_PORT);
    my_address.sin_addr.s_addr = htonl(INADDR_ANY);

    
    other_address.sin_family = AF_INET;
    other_address.sin_port = htons(atoi(argv[3]));
    other_address.sin_addr.s_addr = inet_addr(argv[2]);

    
    if(bind(sock, (struct sockaddr*)&my_address, addr_len) == -1){
        kill("Bind error!");
    }

   
    fd = fopen(argv[1],"rb");
    if(fd == NULL)
        kill("File not found!");

    file_size = fsize(&fd);
    packets_to_send = packets_counter(file_size);

    
    char *file = malloc((sizeof(char) * file_size));

    if(file == NULL)
        kill("Memory error!");
    if(fread(file, 1, file_size, fd) != file_size)
        kill("Copy error!");

    

    FD_ZERO(&set); 
    FD_SET(sock, &set); 

  
    
    timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
    timeout.tv_usec = 0;


   
    while(hasContent()){

       
        size = copy_size(file_size, packets_send + 1);

      
        buffer[0] = atual_ack;

       
        if(size > 0)
            memcpy(buffer + 1, file + start, size);

        
        start += size;


      
        while(enable_send && packs_to_expire != LIMITE){

        
            if(sendto(sock, buffer, (unsigned long) (size + 1), 0, (struct sockaddr*)&other_address, addr_len) == -1)
                kill("Send failed!");

            rv = select(sock + 1, &set, NULL, NULL, &timeout);


            if (rv == SOCKET_ERROR)     
                kill("Socket error!");
            else if (rv == 0){          

               
                packs_to_expire++;

               
                timeout.tv_usec = 0;
                timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
                FD_ZERO(&set);
                FD_SET(sock, &set);

                
            }
            else{  
                if (recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr*)&my_address, &addr_len) == SOCKET_ERROR)
                    kill("Error receiving!");
                else{

                    
                    if(buffer[0] == (atual_ack + 2)){
                        atual_ack = ack(atual_ack);
                        enable_send = false;
                        
                    }

                    else{
                        packs_to_expire++;
                        
                    }
                }
            }
        }

      
        enable_send = true;

       
        if(packs_to_expire == LIMITE){
            kill("No response! Stopping!");
        }

        
        memset(buffer, 0, BUFSIZE);

        packs_to_expire = 0;
        packets_send++;
        timeout.tv_usec = 0;
        timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
    }
     printf("File sent\n");
    if(fclose(fd) != 0)
        kill("Error during file closing!");

    if(close(sock) != 0)
        kill("Error during socket closing!");

    free(file);
    return 0;
}