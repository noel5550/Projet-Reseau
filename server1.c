/*
 * server.c
 * gcc server.c
 * ./a.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "5000"   
typedef struct sockaddr_storage sockaddr_storage;
typedef struct addrinfo addrinfo;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
     int socket_descriptor;          /* descripteur de socket */
    int nouv_socket_descriptor;     /* [nouveau] descripteur de socket */

    // File descriptor indicateur utilisé pour accédé au ressource input/output (socket)
    fd_set fd_principal;    //fd_principal file descriptor list
    fd_set fd_temp;  //temp file descriptor list for select()
    int fd_max;        //max. amount of file descriptors

    sockaddr_storage remoteaddr; //client address, adresse du client , defini une socket qu'on pourra manipulé (bind , connect) 
    socklen_t addrlen;

    char buffer[256];              //buffer for client data , buffer pour les entrées des clients
    int longueur_message;          // longueur du message

    char remoteIP[INET6_ADDRSTRLEN]; // taille de la chaine de caractère pour l'adress  IP

    int yes=1;        //for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    // structure prédéfini dans les biblioteque importé
    // contient les informations sur un hote internet ou un service
    addrinfo hints, *ai, *p;  

    FD_ZERO(&fd_principal);    //FD_ZERO works like memset 0;  vide la liste fd_principal 
    FD_ZERO(&fd_temp);  //clear the fd_principal and temp sets   vide la list de lecture

    // fetch a socket, bind it
    memset(&hints, 0, sizeof hints); // on vide la structure contenant les info sur l'adress
    hints.ai_family = AF_UNSPEC;  // AF_UNSPEC indique que l'adresse sera IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM;  // type de socket attendu , Support de dialogue garantissant l'intégrité, fournissant un flux de données binaires
    hints.ai_flags = AI_PASSIVE; // permet de lié la socket , et la socket va accepté la connection

    // getaddrinfo() renvoie une ou plusieurs structures addrinfo,
    // chacune d'entre elles contenant une adresse Internet qui puisse être indiquée dans un appel à bind ou connect
    // hint donne les critères de selection des structures d'adresse de socket
    // ai pointe sur la liste des critères
    // on teste si on peut accédé au PORT désiré
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) { 
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    // on parcourt la liste des structures ( liste chainer , p->ai_next , pointe sur la structure suivante )
    // on crée une socket pour chaque adress
    for(p = ai; p != NULL; p = p->ai_next)
    {   
        // crée un socket , et renvoi un descripteur
        // le descripteur est une référence a la socket créé
        socket_descriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol); 
        if (socket_descriptor < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        // défini les options de la socket
        // SOL_SOCKET 
        // SO_REUSEDDR autorise la réutilisation de l'adresse locale
        setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        // on lie la socket avec l'adresse
        if (bind(socket_descriptor, p->ai_addr, p->ai_addrlen) < 0) {
            close(socket_descriptor);
            continue;
        }

        break;
    }

    // si la liaison échoue
    if (p == NULL) {
        fprintf(stderr, "erreur, liaison de la socket avec l'adresse échoué\n");
       // fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); //all done; free memory

    //puts("binding successful!");
    puts("liaison de la socket avec l'adresse réussi");

    // listen marque la référence de la socket qui est utilisé pour accepter les demandes de connexions entrantes en utilisant accept()
    if (listen(socket_descriptor, 10) == -1)
    {
        perror("listen");
        exit(3);
    }

   // puts("hearing aid is on, server listening...");

    // add the socket_descriptor to the fd_principal set
    // on ajoute les descripteurs ( référence de socket ) dans la liste principale
    FD_SET(socket_descriptor, &fd_principal);

    // keep track of the biggest file descriptor
    fd_max = socket_descriptor; //(it's this one so far)

    // main loop
    for(;;)
    {
        fd_temp = fd_principal; // copy it

        // on verifie que les file descriptor sont prêt a être utilisé ( entré - sortie )
        if (select(fd_max+1, &fd_temp, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        //run through the existing connections looking for data to read
        // parcours les références de socket a la recherche de donné a lire
        for(i = 0; i <= fd_max; i++) //EDIT HERE
        {
            // on verifie que "i" est bien compris dans la liste des file descriptor
            // (doc) Returns a non-zero value if the bit for the file descriptor fd is set in the file descriptor set pointed to by fdset, and 0 otherwise.

            if (FD_ISSET(i, &fd_temp))
            {   // we got one
                if (i == socket_descriptor)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;

                    // crée un socket avec les meme caractéristique que sockadrr
                    nouv_socket_descriptor = accept(socket_descriptor,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (nouv_socket_descriptor == -1)
                    {
                        perror("création de socket a échoué");
                    }

                    else
                    {
                        // on passe le bit de new fd dans la liste des files descriptor
                        FD_SET(nouv_socket_descriptor, &fd_principal); // pass to fd_principal set

                        if (nouv_socket_descriptor > fd_max)
                        {   // keep track of the max
                            fd_max = nouv_socket_descriptor;
                        }

                        /* printf("selectserver: new connection from %s on "
                               "socket %d\n", inet_ntop(remoteaddr.ss_family,
                               get_in_addr((struct sockaddr*)&remoteaddr),
                               remoteIP, INET6_ADDRSTRLEN), nouv_socket_descriptor); */

                        printf("nouvelle connexion %s sur la socket %d\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN), nouv_socket_descriptor);
                    }
                }
                
                else
                {
                    // gérer les messages envoyés par les clients
                    // i représente la socket
                    // buffer => tableau de 256 char
                    // recv utilisé pour recevoir les messages depuis une socket
                    // renvoie le nombre d'octets reçus
                    if ((longueur_message = recv(i, buffer, sizeof buffer, 0)) <= 0)
                    {   
                        // erreur ou connexion fermer par le client
                        if (longueur_message == 0)
                        {   // connection closed
                            // printf("selectserver: socket %d hung up\n", i);
                            printf("connexion fermer , socket : %d\n", i);
                        }
                        else
                        {
                            //perror("recv");
                            perror("erreur lors de la reception des messages");
                        }
                        close(i); // bye!
                        FD_CLR(i, &fd_principal); // remove from fd_principal set
                    }
                    else
                    {
                        // nous avons reçu des données d'un client

                        // fd_max , le nombre de file descriptor ( reférence des sockets )
                        for(j = 0; j <= fd_max; j++)
                        {
                            // broadcast to everyone
                            // verifie que le bit qui réprente la socket, est present dans la liste des file descriptor (socket)
                            if (FD_ISSET(j, &fd_principal))
                            {
                                // except the socket_descriptor and ourselves
                                if (j != socket_descriptor && j != i)
                                {
                                    if (send(j, buffer, longueur_message, 0) == -1)
                                    {
                                        perror("erreur lors de l'envoie du message");
                                    }
                                }
                            }
                        }
                    }
                }//handle data from client
            } //new incoming connection
        } //looping through file descriptors
    } //for(;;)

    return 0;
}
