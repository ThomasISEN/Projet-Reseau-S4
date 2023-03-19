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

#define LG_MESSAGE 1024

typedef struct CASE{
	char couleur[10]; //couleur format RRRGGGBBB

}CASE;

typedef struct CLIENT CLIENT;

struct CLIENT{
	int id;
    int timer;
    int pixel;
	int client_fds;
	struct pollfd fds;
	CLIENT *suiv;

};

//-------------HEADER-------------//
void initMatrice(CASE *matrice, int taille);
char *setPixel(CASE *matrice, int posL, int posC,int l, int c, char *val);
char *getMatrice(CASE *matrice, int ligne,int colone);
char *getSize(int l, int c);
char *getLimits(int l, int c,int maxTempsAttente);
char *getVersion();
char *getWaitTime(int timer);
void stripFunc(char phrase[LG_MESSAGE*sizeof(char)]);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot);
void afficheMatrice(CASE *matrice);
int createSocket();
void bindSocket(int socketEcoute, int port);
void listenSocket(int socketEcoute);
char* interpretationMsg(CASE *matrice, int l, int c,char messageRecu[LG_MESSAGE], CLIENT *liste_client, int i);
void mainServeur( CASE *matrice, int l, int c, int socketEcoute, CLIENT *liste_client, int maxClients);
CLIENT* creerClient();
CLIENT* ajouterClient(CLIENT* liste, CLIENT* pers);
CLIENT* supprimeClient(CLIENT *liste, int idSup);
CLIENT *actualiseClient(CLIENT *liste_client, struct pollfd *fds, int maxClients);
CLIENT *deduirePixel(CLIENT *liste, int i);
char* base64_encode(const unsigned char* input, size_t input_len);



void afficher_id_clients(struct CLIENT *liste_client) {
    struct CLIENT *p = liste_client;
    while (p != NULL) {
        printf("ID client : %d restant: %d\n", p->id, p->pixel);
        p = p->suiv;
    }
}

//---------------------------------------//
int main(int argc, char *argv[]) {

	//Interpretation de la commande du lancement serveur
    int opt;
    int port=0, c=0, l=0, maxClients=0;

	// Vérifie que la commande a la forme attendue
    if (argc != 7) {
        fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l MAX_CLIENTS]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "p:s:l:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                sscanf(optarg, "%dx%d", &c, &l);
                break;
            case 'l':
                maxClients = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l MAX_CLIENTS]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Création des sockets
    CASE matrice[l*c];
	CLIENT *liste_client=NULL;
    int socketEcoute, client_fds[maxClients];
    struct sockaddr_in server_addr;
	struct pollfd serveur_fds;

    initMatrice(matrice, l*c);

    // Création du socket serveur
    socketEcoute = createSocket();

    // Configuration du socket serveur avec l'adresse et le port
    bindSocket(socketEcoute, port);

    // Mise en écoute du socket serveur
    listenSocket(socketEcoute);

	//lancement du serveur
	mainServeur(matrice, l, c, socketEcoute, liste_client, maxClients);

    return 0;
}

void mainServeur( CASE *matrice, int l, int c, int socketEcoute, CLIENT *liste_client, int maxClients){

	struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
	struct pollfd fds[maxClients + 1]; //ajouter le fds du serveur
	fds[0].fd = socketEcoute;
    fds[0].events = POLLIN;

	// Boucle principale
    while (1) {

		//actualisation de la liste du poll()
		for (int j = 1; j < (maxClients+1); j++) {
			fds[j].fd = -1;
			fds[j].events = POLLIN;
    	}
		
		if (liste_client!=NULL)//liste vide
		{
			int i=1;
			CLIENT *tmp = liste_client;
			while(tmp!=NULL && i<(maxClients+1)){
				fds[i]=tmp->fds;
				tmp->id=i;
				tmp=tmp->suiv;
				//printf("client %d: %d",i,fds[i].fd);
				i++;
				//printf("%p\n", tmp->suiv);
			}
		}

        // Attente d'événements sur les sockets
        int rc = poll(fds, maxClients + 1, -1);
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
				//on regarde si il n'a a pas trop de clients
				int i=1;
				if (liste_client!=NULL)
				{
                    i=2;
					CLIENT *tmp = liste_client;
					while(tmp->suiv!=NULL){
						tmp=tmp->suiv;
						i++;
					}
				}
				printf("i: %d\n",i);
				if (i>= maxClients) {
                    printf("Trop de connexions simultanées, fermeture de la connexion\n");
                    close(new_fd);
                }else
				{	
					//nouvelle connexion
					CLIENT *client=NULL;
					client= creerClient();
					client->client_fds = new_fd;
					client->fds.fd = new_fd;
                    client->id=i;
					liste_client=ajouterClient(liste_client, client);
					printf("Nouvelle connexion : %s:%d id:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client->id);
				}
            }
        }
        // Vérification des événements sur les sockets clients
        for (int i = 1; i < maxClients; i++) {
            char messageEnvoi[LG_MESSAGE];
            char messageRecu[LG_MESSAGE];

            if (fds[i].fd!=-1 && fds[i].revents & POLLIN) {
                // Lecture des données provenant du client
                int lus = read(fds[i].fd, messageRecu, LG_MESSAGE * sizeof(char));
                if (lus == -1) {
                    perror("Erreur lors de la lecture des données");
                } else if (lus == 0) {
                    // Déconnexion du client
                    printf("Déconnexion du client : %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(fds[i].fd);
					liste_client=supprimeClient(liste_client, i);
                } else {
                    // Affichage du message reçu
                    messageRecu[lus]='\0';
                    printf("Message reçu de %s:%d : %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), messageRecu);                    
                    strcpy(messageEnvoi,interpretationMsg(matrice, l, c, messageRecu, liste_client, i));
                    // Mise à jour de l'état du client
                    liste_client = actualiseClient(liste_client, fds, maxClients);
                }

            }else  if (fds[i].fd!=-1 && fds[i].revents & POLLOUT) {
                // Mise à jour de l'état du client
                liste_client = actualiseClient(liste_client, fds, maxClients);

                int ecrits = write(fds[i].fd, messageEnvoi, strlen(messageEnvoi));
                switch (ecrits) {
                    case -1:
                        perror("write");
                        close(fds[i].fd);
                        liste_client=supprimeClient(liste_client, i);
                        exit(-6);
                    case 0:
                        fprintf(stderr, "La socket a été fermée par le client !\n\n");
                        close(fds[i].fd);
                        liste_client=supprimeClient(liste_client, i);
                        exit(0);
                    default:
                    printf(" ");
                        //printf("%s\n(%d octets)\n\n", messageEnvoi, ecrits);
                }
            }

        }
    }

	// Fermeture du socket
    close(socketEcoute);

}



//-------------FONCTION DE JEU-------------//
char* base64_encode(const unsigned char* data, size_t input_length) {
    //printf("base 64 encoding... !\n");
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length + 1);
    if (encoded_data == NULL) return NULL;

    size_t i, j;
    uint32_t octet_a, octet_b, octet_c, triple;

    for (i = 0, j = 0; i < input_length;) {
        octet_a = data[i++];
        octet_b = (i < input_length) ? data[i++] : 0;
        octet_c = (i < input_length) ? data[i++] : 0;
        triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = base64_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 0 * 6) & 0x3F];
    }

    // Ajoute les caractères '=' uniquement si nécessaire
    for (i = 0; i < (3 - (j % 3)) % 3; i++) {
        encoded_data[j++] = '=';
    }

    encoded_data[j] = '\0';
    printf("base 64 encoded !\n");

    return encoded_data;
}

void initMatrice(CASE *matrice, int taille){
    for(int i = 0; i < taille; i++) {
        strcpy(matrice[i].couleur,"255255255");
    }
    printf("matrice initialisée avec succes.\n");
}

char *setPixel(CASE *matrice, int posL, int posC,int l, int c, char *val){
	if (posL >= 0 && posL < l && posC >= 0 && posC < c) {
        strcpy(matrice[(posL*c)+posC].couleur, val); //(posL*c)+posC valeur sur un tableau 1D d'une position en tableau 2D
        return "00\0"; //OK
    }else
    {
        printf("Out of Bounds\n");
        return "11\0";//Out of Bounds
    }
    
}

char* getMatrice(CASE *matrice, int ligne, int colonne) {
    int i, j;
    int tailleChaine = ligne * colonne * 10 + 1; // 13 pour les informations d'une case (10 pour la couleur + 3 pour l'espace et le \n)
    char* chaine = malloc(tailleChaine * sizeof(char));
    char temp[10]; // 13 pour les informations d'une case (10 pour la couleur + 3 pour l'espace et le \n)

    // Parcours de la matrice
    for (i = 0; i < ligne; i++) {
        for (j = 0; j < colonne; j++) {
            // Ajout des informations de la case courante à la chaîne
            sprintf(temp, "%s", matrice[i * colonne + j].couleur);
            strcat(chaine, temp);
        }
        //strcat(chaine, "\n"); // Retour à la ligne après chaque ligne de la matrice
    }
    //printf("chaine char:%s\n",chaine);
    return chaine;
}


char *getSize(int l, int c){//C et L paramettre de serveur
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%dx%d", c, l);
    return resultat;
}

char *getLimits(int l, int c,int pix_Min){//maxTempsAttente paramettre de serveur
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%d", pix_Min);
    return resultat;
}

char *getVersion(){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "2.0");
    return resultat;
}

char *getWaitTime(int timer){//A identifier par rapport à l'IP client
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%d", timer);
    return resultat;
}

void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot){
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
	if (cpt>3)//trop de mot pour la commande SetPixel
	{
		mot[0] = '\0'; // chaîne vide
	}else if (*separateur=='x' && cpt!=2)//pas assez ou trop d'argument à la commande
	{
		mot[0] = '\0'; // chaîne vide
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

void bindSocket(int socketEcoute, int port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
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

char* interpretationMsg(CASE *matrice, int l, int c,char messageRecu[LG_MESSAGE], CLIENT *liste_client, int i){
        //printf("interpretation message...\n");
        //printf("pixel restants pour chaque clients:\n");
        //afficher_id_clients(liste_client);

        char prMot[LG_MESSAGE]; //le premier mot de la commande
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
            int posC = atoi(x);
            int posL = atoi(y);

            selectMot(messageRecu, 3, " ", couleur);
            if (strlen(couleur)==9)
            {
                liste_client=deduirePixel(liste_client, i);                
                return setPixel(matrice, posL, posC, l, c, couleur);
            }else
            {
                return "12\0";//Bad Command
            }

        } else if(strcmp(prMot,"/getSize\0")==0){
            return getSize(l,c);

        } else if(strcmp(prMot,"/getMatrice\0")==0){
            
            //printf("matrice commande selected\n");
            char *data= getMatrice(matrice, l, c);
            //printf("matrice commande executed\n");
            return base64_encode(data ,strlen(data));

        } else if(strcmp(prMot,"/getLimits\0")==0){
            return getLimits(l, c, 5);

        } else if(strcmp(prMot,"/getVersion\0")==0){
            return getVersion();

        } else if(strcmp(prMot,"/getWaitTime\0")==0){
            return getWaitTime(60);
            
        } else{
            return "10\0";
        }
}
//---------------------------------------//


//-------------FONCTION CLIENT-------------//
CLIENT* supprimeClient(CLIENT *liste, int idSup){
    CLIENT *prev = NULL;
    CLIENT *curr = liste;
    
    while (curr != NULL && curr->id != idSup) {
        prev = curr;
        curr = curr->suiv;
    }
    
    if (curr == NULL) {
        printf("ID not found\n");
        return liste;
    }
    
    if (prev == NULL) {
        liste = curr->suiv;
    } else {
        prev->suiv = curr->suiv;
    }
    
    free(curr);
    printf("Client with ID %d deleted\n", idSup);
    
    return liste;
}

CLIENT* creerClient(){
	CLIENT *pers = malloc(sizeof(CLIENT));
	pers->suiv=NULL;
	pers->id=0;
	pers->client_fds = -1;
	pers->fds.fd = -1;
	pers->fds.events = POLLIN;
    pers->timer=0;
    pers->pixel=2;
	return pers;
}

CLIENT* ajouterClient(CLIENT* liste, CLIENT* pers){
	if (liste==NULL)//liste vide
	{
		return pers;
	}
	CLIENT *tmp = liste;
	do{
		if(tmp->suiv==NULL){
			tmp->suiv=pers;
			return tmp;
		}else{
			tmp=tmp->suiv;
		}
	}while(tmp->suiv!=NULL);
	tmp->suiv=pers;
	return liste;

}

CLIENT *actualiseClient(CLIENT *liste_client, struct pollfd *fds, int maxClients) {
    struct CLIENT *ptr = liste_client;

    // Parcourir la liste chainée
    while (ptr != NULL) {
        int client_fd = ptr->fds.fd;

        // Trouver le pollfd correspondant
        int i;
        for (i = 1; i < maxClients; i++) {
            if (fds[i].fd == client_fd) {
                break;
            }
        }

        // Actualiser l'état du client
        if (fds[i].revents & POLLIN) {
            ptr->fds.events = POLLOUT;
        } else if (fds[i].revents & POLLOUT) {
            ptr->fds.events = POLLIN;
        }

        ptr = ptr->suiv;
    }

    return liste_client;
}

CLIENT *deduirePixel(CLIENT *liste, int i){
    CLIENT *curr = liste;
    //printf("l'ID à déduire: %d\n", i);
    
    while (curr != NULL && curr->id != i) {
        curr = curr->suiv;
    }
    
    if (curr == NULL) {
        printf("ID not found\n");
        return liste;
    }
    curr->pixel--;
    //printf("l'ID %d a un nombre de pixel de : %d\n", i, curr->pixel);
    
    return liste;
}
//---------------------------------------//

