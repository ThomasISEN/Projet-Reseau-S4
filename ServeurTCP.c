#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <poll.h>

#define MAX_CLIENTS 10
#define LG_MESSAGE 1024
#define L 4
#define C 8
#define VERSION 1.5
#define PORT 12345

typedef struct CASE{
	char couleur[10]; //couleur format RRRGGGBBB

}CASE;

//-------------HEADER-------------//
void initMartice(CASE matrice[L][C]);
void setPixel(CASE matrice[L][C], int posL, int posC, char *val);
char *getMatrice(CASE matrice[L][C]);
char *getSize();
char *getLimits(int pixMin);
char *getVersion();
char *getWaitTime(int timer);
void stripFunc(char phrase[LG_MESSAGE*sizeof(char)]);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot);
void afficheMatrice(CASE matrice[L][C]);
int createSocket();
void bindSocket(int socketEcoute);
void listenSocket(int socketEcoute);
void interpretationMsg(CASE matrice[L][C], char messageEnvoi[LG_MESSAGE],char messageRecu[LG_MESSAGE], int lus);
//---------------------------------------//

//-------------FONCTION DE JEU-------------//
void initMartice(CASE matrice[L][C]){
	for (int i = 0; i < L; ++i)//parcours des lignes
	{
		for (int j = 0; j < C; ++j)//parcours des colonnes 
		{
			strcpy(matrice[i][j].couleur,"255255255");
		}
	}
}

void setPixel(CASE matrice[L][C], int posL, int posC, char *val){
	if (posL >= 0 && posL < L && posC >= 0 && posC < C) {
        strcpy(matrice[posL][posC].couleur, val);
    }
}

char* getMatrice(CASE matrice[L][C]) {
    char* matstr = NULL;
    int matstr_size = 0;
    for (int i = 0; i < L; ++i) {
        for (int j = 0; j < C; ++j) {
            char* couleur = matrice[i][j].couleur;
            int couleur_size = strlen(couleur);
			matstr_size += couleur_size+2;
            matstr = (char*)realloc(matstr, matstr_size +1);
			strcat(matstr, "|"); 
            strcat(matstr, couleur);
			strcat(matstr, "|");
        }
		matstr_size += 1;
		matstr = (char*)realloc(matstr, matstr_size + 1);// +1 pour le \0
		strcat(matstr, "\n"); 
    }
    return matstr;
}

char *getSize(){//C et L paramettre de serveur
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%dx%d", L, C);
    return resultat;
}

char *getLimits(int maxTempsAttente){//maxTempsAttente paramettre de serveur
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "Limits: (%d, %d), (%d s)", L, C, maxTempsAttente);
    return resultat;
}

char *getVersion(){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "Version: 1.0");
    return resultat;
}

char *getWaitTime(int timer){//A identifier par rapport à l'IP client 
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "temps d'attente: %d s", timer);
    return resultat;
}

void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot){
	// faire gestion erreur nombre trop grand ou trop petit
	int i=0, j=0, cpt=1;
	
	while (phrase[i]!='\n'  && phrase[i]!='\0')
	{
		if (cpt==nombre)//on regarde si on est au mot que l'on veut
		{
			mot[j]=phrase[i];
			j++;
		}			
		i++;
		if (phrase[i]==*separateur || phrase[i]==' ')// on passe au mot suivant // séparateur : pour le x de la position
		{
			i++;
			cpt++;
		}
	}
	mot[j]='\0';

	//ajout pour gérer les erreurs setPixel
	if (cpt>3)//trop de mot 
	{
		mot[0] = '\0'; // chaîne vide
	}else if (*separateur=='x' && cpt!=2)//pas assez ou trop d'argument à la commande
	{
		mot[0] = '\0'; // chaîne vide
	}
}

void afficheMatrice(CASE matrice[L][C]){
	for (int i = 0; i < L; ++i)
	{
		for (int j = 0; j < C; ++j)
		{
			printf("|%s|",matrice[i][j].couleur);
		}
		printf("\n");
	}
}
//---------------------------------------//


//-------------FONCTION RESEAU-------------//
int createSocket() {
    int socketEcoute = socket(PF_INET, SOCK_STREAM, 0);
    if (socketEcoute == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket créée avec succès ! (%d)\n", socketEcoute);
    return socketEcoute;
}

void bindSocket(int socketEcoute) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(socketEcoute, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erreur lors de l'association du socket au port");
        exit(EXIT_FAILURE);
    }
    printf("Socket attachée avec succès !\n");
}

void listenSocket(int socketEcoute) {
    if (listen(socketEcoute, 5) == -1) {
        perror("Erreur lors de la mise en écoute du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket placée en écoute passive ...\n");
}

void interpretationMsg(CASE matrice[L][C], char messageEnvoi[LG_MESSAGE],char messageRecu[LG_MESSAGE], int lus){
        
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        char prMot[LG_MESSAGE]; //le premier mot de la commande
        printf("message recu: %s\n", messageRecu);
        selectMot(messageRecu, 1, " ", prMot);

        if(strcmp(prMot,"/setPixel\0")==0){
            //Définition:
            char place[LG_MESSAGE];
            char x[LG_MESSAGE];
            char y[LG_MESSAGE];
            char couleur[LG_MESSAGE];

            selectMot(messageRecu, 2, " ", place);
            selectMot(place, 1, "x",x);
            selectMot(place, 2, "x",y);
            int xInt = atoi(x);
            int yInt = atoi(y);
            
            selectMot(messageRecu, 3, " ", couleur);
            if (strlen(couleur)==9)
            {
                setPixel(matrice, yInt, xInt, couleur);
                strcpy(messageEnvoi,getMatrice(matrice));
            }else
            {
                strcpy(messageEnvoi,"Bad Command");
            }
            // afficheMatrice(matrice);

        } else if(strcmp(prMot,"/getSize\0")==0){
			strcpy(messageEnvoi,getMatrice(matrice));
			strcat(messageEnvoi,"\n");
            strcat(messageEnvoi,getSize());
			strcat(messageEnvoi,"\n");
        } else if(strcmp(prMot,"/getMatrice\0")==0){
            strcpy(messageEnvoi,getMatrice(matrice));
			strcat(messageEnvoi,"\n");
        } else if(strcmp(prMot,"/getLimits\0")==0){
			strcpy(messageEnvoi,getMatrice(matrice));
			strcat(messageEnvoi,"\n");
            strcat(messageEnvoi,getLimits(10));
			strcat(messageEnvoi,"\n");
        } else if(strcmp(prMot,"/getVersion\0")==0){
			strcpy(messageEnvoi,getMatrice(matrice));
			strcat(messageEnvoi,"\n");
            strcat(messageEnvoi,getVersion());
			strcat(messageEnvoi,"\n");
        } else if(strcmp(prMot,"/getWaitTime\0")==0){
			strcpy(messageEnvoi,getMatrice(matrice));
			strcat(messageEnvoi,"\n");
            strcat(messageEnvoi,getWaitTime(60));
			strcat(messageEnvoi,"\n");
        } else{
            printf("Message reçu : %s (%d octets)\n\n", messageRecu, lus);
            strcpy(messageEnvoi,"Bad Command\n");
        }
}
//---------------------------------------//


void mainServeur( CASE matrice[L][C], int socketEcoute, int client_fds[MAX_CLIENTS], struct pollfd fds[MAX_CLIENTS + 1]){
	
	struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

	// Boucle principale
    while (1) {
        // Attente d'événements sur les sockets
        int rc = poll(fds, MAX_CLIENTS + 1, -1);
        if (rc == -1) {
            perror("Erreur lors de l'appel à poll()");
            exit(EXIT_FAILURE);
        }

        // Vérification des événements sur le socket serveur
        if (fds[0].revents & POLLIN) {
            // Acceptation d'une nouvelle connexion
            int new_fd = accept(socketEcoute, (struct sockaddr*)&client_addr, &addrlen);
            if (new_fd == -1) {
                perror("Erreur lors de l'acceptation de la connexion");
            } else {
                // Recherche d'une place libre pour la nouvelle connexion
                int i;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (client_fds[i] == -1) {
                        client_fds[i] = new_fd;
                        fds[i + 1].fd = new_fd;
                        printf("Nouvelle connexion : %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        break;
                    }
                }
                if (i == MAX_CLIENTS) {
                    printf("Trop de connexions simultanées, fermeture de la connexion\n");
                    close(new_fd);
                }
            }
        }

        // Vérification des événements sur les sockets clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            char messageEnvoi[LG_MESSAGE];
            char messageRecu[LG_MESSAGE];

            if (client_fds[i] != -1 && fds[i + 1].revents & POLLIN) {
                // Lecture des données provenant du client
                int lus = read(client_fds[i], messageRecu, LG_MESSAGE * sizeof(char));
                if (lus == -1) {
                    perror("Erreur lors de la lecture des données");
                } else if (lus == 0) {
                    // Déconnexion du client
                    printf("Déconnexion du client : %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(client_fds[i]);
                    client_fds[i] = -1; //liberation de la place dans le tableau de socket
                    fds[i + 1].fd = -1;
                } else {
                    // Affichage du message reçu
                    messageRecu[lus]='\0';
                    printf("Message reçu de %s:%d : %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), messageRecu);

                    interpretationMsg(matrice, messageEnvoi, messageRecu, lus);
                    fds[i + 1].events = POLLOUT;
                    printf("message envoie: %s\n", messageEnvoi);
                }

            }else  if (client_fds[i] != -1 && fds[i + 1].revents & POLLOUT) {
                printf("renvoyer les reponses\n");
                fds[i + 1].events = POLLIN;

                int ecrits = write(client_fds[i], messageEnvoi, strlen(messageEnvoi));
                switch (ecrits) {
                    case -1:
                        perror("write");
                        close(client_fds[i]);
                        exit(-6);
                    case 0:
                        fprintf(stderr, "La socket a été fermée par le client !\n\n");
                        close(client_fds[i]);
                        exit(0);
                    default:
                        printf("%s\n(%d octets)\n\n", messageEnvoi, ecrits);
                }
            }

        }
    }

	// Fermeture des sockets
    close(socketEcoute);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != -1) {
            close(client_fds[i]);
        }
    }
}


/*
connecter les paramettres rentrés dans la commande ./serveurTCP [-p PORT] [-s LxH] [-l RATE_LIMIT] dans le programme



*/





int main(int argc, char *argv[]) {

	//Interpretation de la commande du lancement serveur
    int opt;
    int port=0, width=0, height=0, rate_limit=0;

	// Vérifie que la commande a la forme attendue
    if (argc != 7) {
        fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l RATE_LIMIT]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "p:s:l:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                sscanf(optarg, "%dx%d", &width, &height);
                break;
            case 'l':
                rate_limit = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l RATE_LIMIT]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    printf("Port: %d\n", port);
    printf("Width: %d\n", width);
    printf("Height: %d\n", height);
    printf("Rate limit: %d\n", rate_limit);

    // Création des sockets
    CASE matrice[L][C];
    int socketEcoute, client_fds[MAX_CLIENTS];
    struct sockaddr_in server_addr;
	struct pollfd fds[MAX_CLIENTS + 1];

    initMartice(matrice);

    // Création du socket serveur
    socketEcoute = createSocket();

    // Configuration du socket serveur
    // Association du socket serveur à l'adresse et au port
    bindSocket(socketEcoute);

    // Mise en écoute du socket serveur
    listenSocket(socketEcoute);

    // Initialisation des tableaux de sockets et de pollfd
	
    memset(fds, 0, sizeof(fds));
    fds[0].fd = socketEcoute;
    fds[0].events = POLLIN;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        printf("fd: %d",fds[i].fd);
        client_fds[i] = -1;
        fds[i + 1].fd = -1;
        fds[i + 1].events = POLLIN;
    }

	mainServeur(matrice,socketEcoute, client_fds, fds);

    return 0;
}
