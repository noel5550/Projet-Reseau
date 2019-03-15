/*
 * client.c
 * gcc client.c -lpthread -std=gnu99 (for linux systems)
 * ./a.out localhost
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT "5000" // le port auquel le client sera connecter
#define MAXDATASIZE 250 // max number of bytes we can get at once
#define MAXNAMESIZE 11

typedef struct addrinfo addrinfo;

//fonction du thread
void *receive_handler(void *);

// get sockaddr, IPv4 ou IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]){
    char message[MAXDATASIZE]; // tableau du message
    char nickName[MAXNAMESIZE]; // tableau du pseudo


    int sockfd;
    char buffer_envoie[MAXDATASIZE];

    // structure prédéfini dans les biblioteque importé
    // contient les informations sur un hote internet ou un service
    addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];  // taille de la chaine de caractère pour l'adress  IP
 
    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints); // on vide la structure contenant les info sur l'adress
    hints.ai_family = AF_UNSPEC; // AF_UNSPEC indique que l'adresse sera IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM; // type de socket attendu , Support de dialogue garantissant l'intégrité, fournissant un flux de données binaires

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // boucler tous les adresses et se connecte à la première disponible
    // on crée la socket ( sockfd va contenir le socket descriptor , reference de la socket crée)
    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    // on n'arrive pas à connecter le client avec la socket
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);

    puts("Nom:"); // affichage 
    memset(&nickName, sizeof(nickName), 0); //vide le buffer (remplace l'écriture binaire par des 0 pour  vider la chaine de caratere)
    memset(&message, sizeof(message), 0); ////vide le buffer (remplace l'écriture binaire par des 0 pour  vider la chaine de caratere)
    fgets(nickName, MAXNAMESIZE, stdin);  //recupère le nom de l'utilisateur

    //création d'un nouveau thread pour reçevoir des messages :
    pthread_t recv_thread;

    // le thread principal va gérer l'envoie des messages
    // le nouveau thread quant a lui va s'occuper de la reception des messages
    // crée un thread qui va executer la fonction "receive_handler"
    if( pthread_create(&recv_thread, NULL, receive_handler, (void*)(intptr_t) sockfd) < 0){ 
        perror("could not create thread");
        return 1;
    }

    // le thread a été crée avec succès
    puts("Synchronous receive handler assigned");

    //Envoie un message au serveur
    puts("Connecter\n");
    puts("[Inserez '/quit' pour quitter]");

    //while(strcmp(buffer_envoie,"/quit") != 0)
    while(1){ // boucle infini pour que le client ne s'arrete pas   

		char temp[6];
		memset(&temp, sizeof(temp), 0); //vide le temp (remplace l'écriture binaire par des 0 pour  vider la chaine de caratere)
        memset(&buffer_envoie, sizeof(buffer_envoie), 0); //clean sendBuffer
        fgets(buffer_envoie, 250, stdin); //lit 250 caractere dans l'entré et les stocks dans buffer_envoie

        // le client peut quitter la conversation
		if(buffer_envoie[0] == '/' &&
		   buffer_envoie[1] == 'q' &&
		   buffer_envoie[2] == 'u' &&
		   buffer_envoie[3] == 'i' &&
		   buffer_envoie[4] == 't'){
			return 1; 
		}

		int count = 0;
        while(count < strlen(nickName)){
            message[count] = nickName[count]; // on met le nickname dans le message pour qu'il s'affiche a l'envoie
            count++;
        }

        count--;
        message[count] = ':';
        count++;
		//prepend
        for(int i = 0; i < strlen(buffer_envoie); i++){  // buffer_envoie représente ce qui arrive en entrée 
            message[count] = buffer_envoie[i];
            count++;
        }
        message[count] = '\0';

        // on envoie le message dans la socket
        if(send(sockfd, message, strlen(message), 0) < 0){
            puts("Send failed");
            return 1;
        }

        // une fois le message envoyé , on vide le buffer
        memset(&buffer_envoie, sizeof(buffer_envoie), 0);
    }

    // une fois que l'utilisateur entre "quit"
    // attend que le thread qui recoit les messages achèves sont execution
    // ferme le descripteur
    // on quitte le programme
    pthread_join(recv_thread , NULL);
    close(sockfd);
    return 0;
}

//thread function
void *receive_handler(void *sock_fd){
	int sFd = (intptr_t) sock_fd;
    char buffer[MAXDATASIZE];
    int longueur_message;

    while(1){
        if ((longueur_message = recv(sFd, buffer, MAXDATASIZE-1, 0)) == -1){  // récéption du message depuis une socket, affiche un message d'erreur, et quitte le programme.
            perror("aucun message recu");
            exit(1);
        }else{
        	buffer[longueur_message] = '\0'; // la derniere case du buffer recoit EOF
        }  
        printf("%s", buffer);   // on affiche le contenu du buffer
    }
}
