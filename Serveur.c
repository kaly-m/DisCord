#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define NB_THREADS 6

#define BUFFER_SIZE 1024

int stop = 0;

// pseudo des clients
char *clients[NB_THREADS];

// file descriptors des clients connectés
int fds[NB_THREADS];

#define PORT 12345
int sock;
socklen_t lg;
struct sockaddr_in local;   // structure pour configurer une adresse de socket (ici local)
struct sockaddr_in distant; // structure pour configurer une adresse de socket (ici distant)

/* ------------------------ Socket Instanciation --------------------------- */
void creer_socket()
{
    // instanciation de la structure pour initialiser notre socket
    bzero(&local, sizeof(local));       // mise a zero de la zone adresse
    local.sin_family = AF_INET;         // famille d adresse internet
    local.sin_port = htons(PORT);       // numero de port
    local.sin_addr.s_addr = INADDR_ANY; // types d’adresses prises en charge
    bzero(&(local.sin_zero), 8);        // fin de remplissage

    lg = sizeof(struct sockaddr_in);
    //  création de la socket (IPV4, flux de donnée bidirectionnel (SOCK_STREAM)) + vérification des erreurs à la création de la socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }
    //  liaison entre la socket (sock) précédemment créée et la structure sockaddr_in instanciée auparavant
    if (bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        exit(1);
    }
    // initialise le nombre de connexion simultanées à 5
    if (listen(sock, NB_THREADS) == -1)
    {
        perror("listen");
        exit(1);
    }
}

/* ------------------------ Signaux ------------------------ */
void sigInt_handler(int code)
{
    for (int i = 0; i < NB_THREADS; i++)
    {
        if (fds[i] != -1)
        {
            //envoi du message d'erreur aux clients
            write(fds[i], "ERROR\n", 7);
        }
    }
    //set le flag a true pour que les threads exit
    stop = 1;
    exit(0);
}

/* ------------------------ Threads --------------------------- */

struct ThreadsArgs
{
    int index;
};

void *run(void *args)
{
    // cast
    struct ThreadsArgs *temp = args;

    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);

    int socketClient;

    while (!stop)
    {
        // attente connexion
        printf("Thread n°%i en attente d'une connexion..\n", temp->index);
        if ((socketClient = accept(sock, (struct sockaddr *)&distant, &lg)) == -1)
        {
            perror("accept");
            pthread_exit(NULL);
        }

        // ecriture du socket dans le tableau file descriptor de client
        fds[temp->index] = socketClient;

        // demande le pseudo du client
        write(socketClient, "Client ! Quel est votre pseudo ?!? \n", 37);

        char *pseudo = malloc(sizeof(char) * 80);
        // read le pseudo envoyé par le client et l'écrit dans le tableau de pseudos

        int nbread = read(socketClient, pseudo, 80);
        if (nbread <= 0)
        { // gestion erreurs
            printf("Il a même pas voulu dire son nom c'est trop injuste.. \n");
            perror("read");
            close(socketClient);
            pthread_exit(NULL); // Fin propre du thread en cas d'erreur
        }
        pseudo[nbread - 1] = ' ';
        clients[temp->index] = pseudo;

        // client connecté
        printf("%ss'est connecté !! \n", pseudo);

        // annonce de la connexion du client
        strcpy(buffer, pseudo);
        strcat(buffer, "s'est connecté ! \n");

        // parcourir les clients connectés pour leur transférer le message
        for (int i = 0; i < NB_THREADS; i++)
        {
            if (i != temp->index && clients[i] != NULL)
            {

                write(fds[i], buffer, strlen(buffer));
            }
        }

        // vide le buffer mess
        strcpy(buffer, "");

        // lis le contenu de socketClient, tant que le serveur ne reçoit pas le mot fin
        while (strcmp(buffer, "fin\n") != 0 && !stop) //|| clients[temp->index] != NULL
        {
            nbread = read(socketClient, buffer, BUFFER_SIZE - 1);
            if (nbread <= 0)
            {
                strcpy(buffer, "fin\n");
                buffer[5] = '\0';
            }
            else
            {
                buffer[nbread] = '\0'; // Ajoute un caractère de fin de chaîne
            }

            // concaténer pseudo + message
            char message[BUFFER_SIZE];
            strcpy(message, pseudo);

            if (strcmp(buffer, "fin\n") != 0)
            {
                strcat(message, ": ");
                strcat(message, buffer);
            } else {
                strcat(message,"s'est déconnecté..\n");
            }

            // parcourir les clients connectés pour leur transférer le message
            for (int i = 0; i < NB_THREADS; i++)
            {
                if (i != temp->index && clients[i] != NULL)
                {
                    write(fds[i], message, strlen(message));
                }
            }
        }

        // ferme son socket (une seule connexion)
        close(socketClient);

        // remettre leur emplacement vide
        clients[temp->index] = NULL;
        fds[temp->index] = -1;

        // au revoir le client
        printf("au revoir %s \n", pseudo);
        free(pseudo);
    }
    free(buffer);
    exit(0);
    pthread_exit(NULL);
}

/* ------------------------ Serveur --------------------------- */
int main()
{
    creer_socket();
    //gestion des signaux
    signal(SIGINT,&sigInt_handler);

    // initialisation du tableau clients
    for (int i = 0; i < NB_THREADS; i++)
    {
        clients[i] = NULL;
    }

    //initialisation du tableau file descriptor
    for (int i = 0; i < NB_THREADS; i++)
    {
        fds[i] = -1;
    }

    // initialisation des threads
    pthread_t threads[NB_THREADS];

    struct ThreadsArgs arguments[NB_THREADS];

    for (int i = 0; i < NB_THREADS; i++)
    {
        arguments[i].index = i;
        if (pthread_create(&threads[i], NULL, run, &arguments[i]) != 0)
        {
            perror("pthread_create");
            exit(1);
        }
    }

    /*------- pthread join est fait pour que le main attende la fin des threads ----- */

    // exécution des threads (fonction bloquante)
    for (int i = 0; i < NB_THREADS; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("pthread_join");
            exit(1);
        }
    }
    stop = 1;
    exit(0);
}
