#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE 1024

#define SERV "127.0.0.1" // ??
#define PORT 12345       // ??
int port, sock;
char mess[80];

/* -------------- création sockets -------------------------- */

struct sockaddr_in serv_addr; // ??
struct hostent *server;       // ??

void creer_socket()
{
  port = PORT;
  server = gethostbyname(SERV); // ??
  if (!server)
  {
    fprintf(stderr, "Problème serveur \"%s\"\n", SERV);
    exit(1);
  }
  // creation socket locale en mode TCP
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket");
    exit(1);
  }

  bzero(&serv_addr, sizeof(serv_addr)); // ??
  serv_addr.sin_family = AF_INET;       // ??
  bcopy(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(port); // ??
}

/* ------------------------------ Threads ---------------------------- */

/**
 * Lit en boucle les messages qui arrivent dans le serveur.
 * Les affiche dans la sortie standard.
 */
void *tache_lecture()
{
  char buffer[BUFFER_SIZE];

  while (1)
  {
    int nbread = read(sock, buffer, BUFFER_SIZE - 1);

    //ERROR renvoyé par le serveur signifie qu'il a quitté de manière innattendue, on ferme donc le client 
    if (strcmp(buffer,"ERROR\n") == 0)
    {
      printf("Le serveur à quitté de manière innattendue..\n");
      close(sock);
      exit(1);
      pthread_exit(NULL); // le serveur a été fermé de l'autre côté
    } else if (nbread <= 0)
    {
      printf("Déconnexion tout en douceur..\n");
      close(sock);
      exit(1);
      pthread_exit(NULL); // le serveur a été fermé de l'autre côté
    }

    // écriture dans la sortie standard
    write(1, buffer, nbread);
  }
}

/**
 * Lit en boucle les messages de l'utilisateur sur l'entrée standard.
 * Les envoie dans le serveur.
 */
void *tache_ecriture()
{
  char buffer[BUFFER_SIZE];

  // vide le buffer
  strcpy(buffer, "");

  // lis ligne par ligne l'entrée standard tant que l'utilisateur n'a pas écrit le mot 'fin'
  while (strcmp(buffer, "fin") != 0)
  {
    // Lecture entrée standard
    int nbread = read(0, buffer, BUFFER_SIZE - 1);

    // Écriture dans le serveur
    write(sock, buffer, nbread);
  }
  close(sock);
  exit(1);
  pthread_exit(NULL);
}

/* ------------------------ Signaux ------------------------ */
void sigInt_handler(int code)
{
  close(sock);
  exit(1);
}

/* --------------------- Fonction Main -----------------------------*/
int main()
{ // lancement fonction
  creer_socket();

  // gestion signaux
  signal(SIGINT, &sigInt_handler);

  // connection au serveur
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Connexion impossible:");
    exit(1);
  }
  printf("connexion avec serveur ok\n");

  // instanciation thread lecture
  pthread_t th_Lecture;
  pthread_create(&th_Lecture, NULL, tache_lecture, NULL);

  // instanciation thread ecriture
  pthread_t th_Ecriture;
  pthread_create(&th_Ecriture, NULL, tache_ecriture, NULL);

  // fonction bloquante
  pthread_join(th_Lecture, NULL);
  pthread_join(th_Ecriture, NULL);
}

