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

/* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& Variables Globales &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */

#define BUFFER_SIZE 1024

#define PORT 12345

/* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& Threads &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */
#define NB_THREADS 10

int stop = 0;

// pseudo des clients
char *clients[NB_THREADS];

// file descriptors des clients connectés
int fds[NB_THREADS];

/* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& Sockets &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& */

int sock;
socklen_t lg;
struct sockaddr_in local;   // structure pour configurer une adresse de socket (ici local)
struct sockaddr_in distant; // structure pour configurer une adresse de socket (ici distant)


/* INITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINITINIT */

/* ############################################################ Socket Instanciation ############################################################ */
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
    // initialise le nombre de connexion simultanées à NB_THREADS
    if (listen(sock, NB_THREADS) == -1)
    {
        perror("listen");
        exit(1);
    }
}

/* ############################################################ Fonction Filtre ############################################################ */

int pseudoEstValide(char *pseudo, int socketClient)
{
    // pseudo contient des espaces ?
    if (strstr(pseudo, " ") != NULL)
    {
        write(socketClient, "Veuillez choisir un pseudo sans espaces :\n", 43);
        return 0;
    }

    // pseudo déjà pris ?
    // enlève le \n pour que le pseudo corresponde aux pseudos dans les tableau
    pseudo[strlen(pseudo) - 1] = ' ';

    for (int i = 0; i < NB_THREADS; i++)
    {
        if (clients[i] != NULL && strcmp(pseudo, clients[i]) == 0)
        {
            write(socketClient, "Pseudo déjà pris..\n", 22);
            return 0;
        }
    }
    return 1;
}

/* ############################################################ Threads ############################################################ */

/**
 * Structure représentant les arguments à passer aux threads lors de leur création
 * @param index le numéro du thread et donc des client que ce thread va traiter
 */
struct ThreadsArgs
{
    int index;
};

/**
 * Recupère le pseudo du client pris en charge par le thread numéro @param numThread puis le met dans @param pseudo.
 * Opère une vérification quand à la validité du pseudo selon les normes du serveur
 * @param socketClient le file descriptor du client avec lequel on communique
 */
void recupererPseudo(int numThread, char *pseudo, int socketClient)
{
    int nbread;
    write(socketClient, "Client ! Quel est votre pseudo ?!? \n", 37);

    do
    {
        free(pseudo);
        pseudo = malloc(sizeof(char) * 80);

        // récupération du pseudo
        nbread = read(socketClient, pseudo, 80);

    } while (nbread > 0 && !pseudoEstValide(pseudo, socketClient));

    // gestion erreurs
    if (nbread <= 0)
    {
        printf("Il a même pas voulu dire son nom c'est trop injuste.. \n");
    }
    else
    {
        // remplis le tableau de pseudos
        clients[numThread] = pseudo;
    }
}

/**
 * Permet au client numéro @param numThread d'envoyer un message sur le serveur
 * @param mess le message à envoyer
 * Renseigner -1 en @param numThread pour que le client soit annoncé depuis le serveur
 */

void annoncerMessage(int numThread, char *mess)
{
    char *message = malloc(strlen(mess) + 20);

    // Annonce serveur ?
    if (numThread == -1)
    {
        strcpy(message, "Annonce serveur : ");
        strcat(message, mess);
    }
    else
    {
        strcpy(message, mess);
    }

    // parcourir les clients connectés pour leur transférer le message
    for (int i = 0; i < NB_THREADS; i++)
    {
        if (i != numThread && clients[i] != NULL)
        {
            write(fds[i], message, strlen(message));
        }
    }
}

/**
 * Fonction phare du thread Serveur.
 * Opère un échange entre le client et le serveur du temps que le mot "fin" n'est pas capturé (ou Ctrl+c).
 * Termine une fois le mot fin capturé ou le FLAG @stop déclenché (terminaison des threads appelé par le main)
 * @param numThread le numéro du client pris en charge par le thread
 * @param buffer une variable globale propre au thread qui permet l'échange d'information entre processus (taille 1024)
 * @param socketClient le file descriptor du client
 * @param pseudo le pseudo du client
 * Gère les erreurs de potentielle déconnexion du client
 */
void echanger(int numThread, char *buffer, int socketClient, char *pseudo)
{
    while (strcmp(buffer, "fin\n") != 0 && !stop) //|| clients[temp->index] != NULL
    {
        int nbread = read(socketClient, buffer, BUFFER_SIZE - 1);
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
        }
        else
        {
            strcat(message, "s'est déconnecté..\n");
        }

        annoncerMessage(numThread, message);
    }
}

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ - RUN - @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ */

/**
 * Fonction run représente l'architecture principale et les rouages du fonctionnement d'un thread.
 * Fonctionnement du serveur à l'échelle d'un thread
 * @param args les paramètres à fournir au thread (i.e. l'index)
 */
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

        char *pseudo = malloc(sizeof(char) * 80);

        // read le pseudo envoyé par le client et l'écrit dans le tableau de pseudos (s'il est valide)
        recupererPseudo(temp->index, pseudo, socketClient);

        //vérification qu'aucune erreur n'a été produite lors de la récupération du pseudo
        if (clients[temp->index] != NULL)
        {
            // client connecté (côté serveur)
            printf("%ss'est connecté !! \n", pseudo);

            // annonce de la connexion du client (aux autres users)
            strcpy(buffer, pseudo);
            strcat(buffer, "s'est connecté ! \n");

            annoncerMessage(temp->index, buffer);

            // vide le buffer mess
            strcpy(buffer, "");

            // lis le contenu de socketClient, tant que le serveur ne reçoit pas le mot fin
            echanger(temp->index, buffer, socketClient, pseudo);
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

/* ############################################################ Signaux ############################################################ */
/**
 * Handle les signaux envoyés au serveur
 * @param code le numéro du signal envoyé
 */
void sig_handler(int code)
{
    for (int i = 0; i < NB_THREADS; i++)
    {
        if (fds[i] != -1)
        {
            // envoi du message d'erreur aux clients
            write(fds[i], "ERROR\n", 7);
        }
    }
    // set le flag a true pour que les threads exit
    stop = 1;
    exit(0);
}

/* ############################################################ Serveur ############################################################ */
int main()
{
    creer_socket();

    // initialisation du tableau clients + tableau des files descriptors + signaux
    for (int i = 0; i < NB_THREADS; i++)
    {
        // gestion des signaux
        signal(i, &sig_handler);

        clients[i] = NULL;

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
