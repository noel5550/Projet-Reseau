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

#define PORT "5000"   // port we're listening on

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
    // File descriptor indicateur utilisé pour accédé au ressource input/output (socket)
    fd_set master;    //master file descriptor list
    fd_set read_fds;  //temp file descriptor list for select()
    int fdmax;        //max. amount of file descriptors

    int listener;     //listening socket descriptor
    int newfd;        //newly accept()ed socket descriptor

    struct sockaddr_storage remoteaddr; //client address , defini une socket qu'on pourra manipulé (bind , connect) 
    socklen_t addrlen;

    char buf[256];    //buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN]; // taille de la chaine de caractère pour l'adress  IP

    int yes=1;        //for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    // structure prédéfini dans les biblioteque importé
    // contient les informations sur un hote internet ou un service
    struct addrinfo hints, *ai, *p;  

    FD_ZERO(&master);    //FD_ZERO works like memset 0;  vide la liste master 
    FD_ZERO(&read_fds);  //clear the master and temp sets   vide la list de lecture

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
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol); 
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        // défini les options de la socket
        // SOL_SOCKET 
        // SO_REUSEDDR autorise la réutilisation de l'adresse locale
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        // on lie la socket avec l'adresse
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // si la liaison échoue
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); //all done; free memory

    puts("binding successful!");

    // listen marque la référence de la socket qui est utilisé pour accepter les demandes de connexions entrantes en utilisant accept()
    if (listen(listener, 10) == -1)
    {
        perror("listen");
        exit(3);
    }

    puts("hearing aid is on, server listening...");

    // add the listener to the master set
    // on ajoute les descripteurs ( référence de socket ) dans la liste principale
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; //(it's this one so far)

    // main loop
    for(;;)
    {
        read_fds = master; // copy it

        // on verifie que les file descriptor sont prêt a être utilisé ( entré - sortie )
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        //run through the existing connections looking for data to read
        // parcours les références de socket a la recherche de donné a lire
        for(i = 0; i <= fdmax; i++) //EDIT HERE
        {
            // on verifie que "i" est bien compris dans la liste des file descriptor
            // (doc) Returns a non-zero value if the bit for the file descriptor fd is set in the file descriptor set pointed to by fdset, and 0 otherwise.

            if (FD_ISSET(i, &read_fds))
            {   // we got one
                if (i == listener)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;

                    // crée un socket avec les meme caractéristique que sockadrr
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }

                    else
                    {
                        // on passe le bit de new fd dans la liste des files descriptor
                        FD_SET(newfd, &master); // pass to master set

                        if (newfd > fdmax)
                        {   // keep track of the max
                            fdmax = newfd;
                        }

                        printf("selectserver: new connection from %s on "
                               "socket %d\n", inet_ntop(remoteaddr.ss_family,
                               get_in_addr((struct sockaddr*)&remoteaddr),
                               remoteIP, INET6_ADDRSTRLEN), newfd);
                    }
                }
                
                else
                {
                    // gérer les messages envoyés par les clients
                    // i représente la socket
                    // buf => tableau de 256 char
                    // recv utilisé pour recevoir les messages depuis une socket
                    // renvoie le nombre d'octets reçus
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
                    {   
                        // erreur ou connexion fermer par le client
                        if (nbytes == 0)
                        {   // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }
                    else
                    {
                        // nous avons reçu des données d'un client

                        // fdmax , le nombre de file descriptor ( reférence des sockets )
                        for(j = 0; j <= fdmax; j++)
                        {
                            // broadcast to everyone
                            // verifie que le bit qui réprente la socket, est present dans la liste des file descriptor (socket)
                            if (FD_ISSET(j, &master))
                            {
                                // except the listener and ourselves
                                if (j != listener && j != i)
                                {
                                    if (send(j, buf, nbytes, 0) == -1)
                                    {
                                        perror("send");
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
