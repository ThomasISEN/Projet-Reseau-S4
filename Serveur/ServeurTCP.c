/**
 * \file ServeurTCP.c
 * \brief Gestion du serveur
 * \author Denis Arrahmani Massard
 * \version 1
 * \date 31 mars 2023
 *
 */

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
#include <time.h>

/**
 * @defgroup fonct_jeu Fonctions du jeu
 * Fonctions qui permettent d'échanger des informations sur le jeu 
 * 
 * @defgroup fonct_reseau Fonctions du protocole
 * Fonctions qui permettent de mettre en place et faire marcher la communication client serveur
 * 
 * @defgroup fonct_client Fonctions pour les clients
 * Fonctions qui permettent de modifier les clients
 */

#define LG_MESSAGE 1024
#define MAX_PIXEL 1
#define MAX_CLIENT 10

/**
 * @struct CASE
 * @details structure des cases de la matrice
 */
typedef struct CASE{
	char couleur[10]; //couleur format RRRGGGBBB

}CASE;

/**
 * @struct CLIENT
 * @details structure des clients
 */
typedef struct CLIENT CLIENT;
struct CLIENT{
	int id;
    clock_t timer;
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
char *getWaitTime(CLIENT *liste, int i);
void stripFunc(char phrase[LG_MESSAGE*sizeof(char)]);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot);
void afficheMatrice(CASE *matrice);
int createSocket();
void bindSocket(int socketEcoute, int port);
void listenSocket(int socketEcoute);
char* interpretationMsg(CASE *matrice, int l, int c,char messageRecu[LG_MESSAGE], CLIENT *liste_client, int i, int maxPixel);
void mainServeur( CASE *matrice, int l, int c, int socketEcoute, CLIENT *liste_client, int maxPixel);
CLIENT* creerClient(int maxPixel);
CLIENT* ajouterClient(CLIENT* liste, CLIENT* pers);
CLIENT* supprimeClient(CLIENT *liste, int idSup);
CLIENT *actualiseClient(CLIENT *liste_client, struct pollfd *fds);
CLIENT *deduirePixel(CLIENT *liste, int i);
char* base64_encode(const char* rgb);
char* base64_decode(const char* messageRecu);



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
    int port=0, c=0, l=0, maxPixel=0;

	// Vérifie que la commande a la forme attendue
    if (argc != 7 && argc != 5 && argc != 3 && argc != 1) {
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
                maxPixel = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l MAX_CLIENTS]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (port==0 || port<1024)
    {
        port=5000;
    }
    if (c==0 || l==0)
    {
        c=80;
        l=40;
    }
    if (maxPixel==0)
    {
        maxPixel=10;
    }
    
    
    

    // Création des sockets
    CASE matrice[l*c];
	CLIENT *liste_client=NULL;
    int socketEcoute, client_fds[MAX_CLIENT];
    struct sockaddr_in server_addr;
	struct pollfd serveur_fds;

    initMatrice(matrice, l*c);

    // Création du socket serveur
    socketEcoute = createSocket();

    // Configuration du socket serveur avec l'adresse et le port
    bindSocket(socketEcoute, port);

    // Mise en écoute du socket serveur
    listenSocket(socketEcoute);
    printf("%d/%dx%d/%d\n",port, l,c,maxPixel);
	//lancement du serveur
	mainServeur(matrice, l, c, socketEcoute, liste_client, maxPixel);

    return 0;
}

/**
 * Fonction principale du serveur qui gère les connexions avec les clients.
 * @param matrice la matrice représentant l'image à afficher
 * @param l le nombre de lignes de la matrice
 * @param c le nombre de colonnes de la matrice
 * @param socketEcoute le socket d'écoute sur lequel le serveur accepte les connexions entrantes
 * @param liste_client la liste chaînée des clients connectés au serveur
 * @param maxPixel le nombre maximal de pixels modifiables par un client
 */
void mainServeur( CASE *matrice, int l, int c, int socketEcoute, CLIENT *liste_client, int maxPixel){

    // Déclaration des variables locales
	struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
	struct pollfd fds[MAX_CLIENT + 1]; //ajouter le fds du serveur
	fds[0].fd = socketEcoute;
    fds[0].events = POLLIN;

	// Boucle principale
    while (1) {

		//actualisation de la liste du poll()
		for (int j = 1; j < (MAX_CLIENT+1); j++) {
			fds[j].fd = -1;
			fds[j].events = POLLIN;
    	}
		
        // Ajout des clients à la liste des descripteurs surveillés par le poll()
		if (liste_client!=NULL) {
			int i=1;
			CLIENT *tmp = liste_client;
			while(tmp!=NULL && i<(MAX_CLIENT+1)){
				fds[i]=tmp->fds;
				tmp->id=i;
				tmp=tmp->suiv;
				i++;
			}
		}

        // Attente d'événements sur les sockets
        int rc = poll(fds, MAX_CLIENT + 1, -1);
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
				// Ajout des clients à la liste des descripteurs surveillés par le poll()
				int i=1;
				if (liste_client != NULL)
				{
                    i=2;
					CLIENT *tmp = liste_client;
					while(tmp->suiv!=NULL){
						tmp=tmp->suiv;
						i++;
					}
				}
				printf("i: %d\n",i);
				if (i >= MAX_CLIENT) {
                    printf("Trop de connexions simultanées, fermeture de la connexion\n");
                    close(new_fd);
                } else {	
					// Création d'un nouveau client
					CLIENT *client=NULL;
					client= creerClient(maxPixel);
					client->client_fds = new_fd;
					client->fds.fd = new_fd;
                    client->id=i;
                    client->timer=time(0);
					liste_client=ajouterClient(liste_client, client);
					printf("Nouvelle connexion : %s:%d id:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client->id);
				}
            }
        }
        // Vérification des événements sur les sockets clients
        for (int i = 1; i < MAX_CLIENT; i++) {
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
                    printf("Message reçu de %s:%d : '%s'\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), messageRecu);                    
                    strcpy(messageEnvoi,interpretationMsg(matrice, l, c, messageRecu, liste_client, i, maxPixel));
                    // Mise à jour de l'état du client
                    liste_client = actualiseClient(liste_client, fds);
                }

            }else  if (fds[i].fd!=-1 && fds[i].revents & POLLOUT) {
                // Mise à jour de l'état du client
                liste_client = actualiseClient(liste_client, fds);
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
                    printf("%s\n(%d octets)\n\n", messageEnvoi, ecrits);
                }
            }

        }
    }

	// Fermeture du socket
    close(socketEcoute);

}



//-------------FONCTION DE JEU-------------//

/**
 * Encodes un tableau de caractères en base64.
 * @param rgb Le tableau de caractères à encoder.
 * @return Un pointeur vers le tableau de caractères encodé en base64, ou NULL en cas d'erreur.
 * @ingroup fonct_jeu
 */
char* base64_encode(const char* rgb) {
    // Tableau des caractères de la table base64
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Longueur du tableau d'entrée
    size_t input_length = strlen(rgb);

    // Longueur du tableau encodé en base64
    size_t output_length = 4 * ((input_length + 2) / 3);

    // Alloue la mémoire nécessaire pour le tableau encodé en base64
    char* encoded_data = malloc(output_length + 1);
    if (encoded_data == NULL) return NULL;

    // Variables pour stocker les octets et les triples
    size_t i, j;
    uint32_t octet_a, octet_b, octet_c, triple;

    // Tableau pour stocker les nombres RGB
    char* nombre = malloc(3*sizeof(char));
    int tableauRGB[input_length/3];
    int cpt=0;

    // Convertit les caractères en nombres RGB
    for (i = 0; i < input_length;) {
        nombre[0] = rgb[i++];
        nombre[1] = rgb[i++];
        nombre[2] = rgb[i++];

        tableauRGB[cpt]= atoi(nombre);
        cpt++;
    }

    // Encode chaque triple d'octets en base64
    for (i = 0, j = 0; i < input_length/3;) {
        octet_a = (uint8_t)tableauRGB[i++];
        octet_b = (uint8_t)tableauRGB[i++];
        octet_c = (uint8_t)tableauRGB[i++];
        triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = base64_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 0 * 6) & 0x3F];
    }

    // Ajoute les caractères '=' si nécessaire pour que la longueur du tableau soit multiple de 4
    for (i = 0; i < (3 - (j % 3)) % 3; i++) {
        encoded_data[j++] = '=';
    }

    // Termine le tableau encodé en base64 avec un caractère nul
    encoded_data[j] = '\0';

    // Retourne le pointeur vers le tableau encodé en base64
    return encoded_data;
}

/**
 * Convertit une chaîne de caractères encodée en base64 en chaîne de caractères décodée en RGB.
 *
 * @param messageRecu La chaîne de caractères encodée en base64 à décoder.
 * @return La chaîne de caractères décodée en RGB, au format "RRRGGGBBBRRRGGGBBBRRRGGGBBB...".
 * @ingroup fonct_jeu
 */
char* base64_decode(const char* messageRecu) {
    // Définition des caractères de base64
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Récupération de la longueur de la chaîne d'entrée
    size_t input_length = strlen(messageRecu);

    // Calcul de la longueur de la chaîne de sortie décodée
    size_t output_length = input_length / 4 * 3;

    // Allocation d'un tableau pour stocker les valeurs RGB décodées
    unsigned char* messageDecode = malloc(output_length + 1);
    int* tableauRGB = malloc(output_length * sizeof(int));
    int cpt = 0;

    // Boucle de décodage de la chaîne encodée en base64
    size_t i, j;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;
    for (i = 0, j = 0; i < input_length;) {
        // Récupération des sextets à partir de la chaîne encodée
        sextet_a = (messageRecu[i] == '=') ? 0 : strchr(base64_chars, messageRecu[i]) - base64_chars;
        sextet_b = (messageRecu[i+1] == '=') ? 0 : strchr(base64_chars, messageRecu[i+1]) - base64_chars;
        sextet_c = (messageRecu[i+2] == '=') ? 0 : strchr(base64_chars, messageRecu[i+2]) - base64_chars;
        sextet_d = (messageRecu[i+3] == '=') ? 0 : strchr(base64_chars, messageRecu[i+3]) - base64_chars;

        // Calcul de la valeur décimale correspondant aux sextets décodés
        triple = (sextet_a << 3 * 6)
               + (sextet_b << 2 * 6)
               + (sextet_c << 1 * 6)
               + (sextet_d << 0 * 6);

        // Stockage des valeurs RGB dans le tableau
        tableauRGB[cpt++] = (triple >> 2 * 8) & 0xFF;
        tableauRGB[cpt++] = (triple >> 1 * 8) & 0xFF;
        tableauRGB[cpt++] = (triple >> 0 * 8) & 0xFF;

        i += 4;
    }

    // Conversion des valeurs RGB en chaîne de caractères
    int taille=3;
    char* result = malloc(taille*sizeof(char));
    printf("otput lenght: %ld\n", output_length);
    for (int i = 0; i < output_length; i++) {
        char nombre[4];
        if (tableauRGB[i]<10)
        {
            sprintf(nombre, "00%d", tableauRGB[i]);
            taille+=strlen(nombre)+2;
            
        }else if (tableauRGB[i]<100)
        {
            sprintf(nombre, "0%d", tableauRGB[i]);
            taille+=strlen(nombre)+1;
            
            }else
            {
            sprintf(nombre, "%d", tableauRGB[i]);
            taille+=strlen(nombre);
            
        }
        
        result= (char*)realloc(result, taille*sizeof(char));
        strcat(result, nombre);
        printf("%d/%s/%d\n", tableauRGB[i], result, i);
    }
    result[taille] = '\0';

    // Libération de la mémoire allouée
    free(messageDecode);
    free(tableauRGB);

    return result;
}


/**
 * Initialise la matrice en donnant à chaque case la couleur blanche (255255255).
 *
 * @param matrice Un pointeur vers le tableau de cases à initialiser.
 * @param taille La taille du tableau de cases.
 * @return Aucune valeur de retour.
 * @ingroup fonct_jeu
 */
void initMatrice(CASE *matrice, int taille){
    for(int i = 0; i < taille; i++) {
        strcpy(matrice[i].couleur,"255255255");
    }
    printf("matrice initialisée avec succes.\n");
}

/**
 * @brief Modifie la couleur d'un pixel dans une matrice.
 * 
 * @param matrice Pointeur vers la matrice de cases.
 * @param posL Position en ligne du pixel à modifier.
 * @param posC Position en colonne du pixel à modifier.
 * @param l Nombre de lignes de la matrice.
 * @param c Nombre de colonnes de la matrice.
 * @param val Chaîne de caractères représentant la nouvelle couleur du pixel, sous la forme "RRRVVVBBB".
 * @return Retourne une chaîne de caractères indiquant le statut de la modification. "00 OK"
 *  si la modification s'est déroulée avec succès, "11 Out Of Bound" si la position du pixel 
 * est hors des limites de la matrice, ou "12 Bad Color" si la chaîne de caractères ne représente pas une couleur valide.
 * @ingroup fonct_jeu
 */
char *setPixel(CASE *matrice, int posL, int posC,int l, int c, char *val){
	if (posL >= 0 && posL < l && posC >= 0 && posC < c) {
        for (int i = 0; i < 9;)
        {
            char nombre[3]={val[i],val[i+1],val[i+2]};
            if (atoi(nombre)>255 || atoi(nombre)<0)
            {
                return "12 Bad Color\0";//Bad Color
            }
            i+=3;
        }
        strcpy(matrice[(posL*c)+posC].couleur, val); //(posL*c)+posC valeur sur un tableau 1D d'une position en tableau 2D
        printf("la case : '%s'\n",matrice[(posL*c)+posC].couleur);
        return "00 OK\0"; //OK
    }else
    {
        printf("Out of Bounds\n");
        return "11 Out Of Bound\0";//Out of Bounds
    }
    
}

/**
 * @brief Fonction qui retourne une chaîne de caractères contenant les couleurs de chaque case d'une matrice.
 *
 * @param matrice un pointeur vers le tableau de cases à parcourir
 * @param ligne le nombre de lignes de la matrice
 * @param colonne le nombre de colonnes de la matrice
 *
 * @return un pointeur vers la chaîne de caractères contenant les couleurs des cases de la matrice
 * @return NULL si l'allocation de mémoire pour la chaîne a échoué
 * @ingroup fonct_jeu
 */
char* getMatrice(CASE *matrice, int ligne, int colonne) {
    int i, j, tailleMax = 10; // Taille maximale d'une couleur
    char* chaine = malloc((ligne * colonne * tailleMax + 1) * sizeof(char));
    // La taille de la chaîne résultante doit être suffisamment grande pour stocker tous les caractères

    if (chaine == NULL) {
        printf("Erreur : impossible d'allouer suffisamment d'espace mémoire pour la chaîne\n");
        return NULL;
    }
    
    chaine[0] = '\0'; // Initialise la chaîne à la chaîne vide
    // Parcours de la matrice
    for (i = 0; i < ligne; i++) {
        for (j = 0; j < colonne; j++) {
            // Ajout des informations de la case courante à la chaîne
            int tailleCouleur = strlen(matrice[i * colonne + j].couleur);
            char* temp = malloc((tailleCouleur + 1) * sizeof(char)); // Alloue suffisamment d'espace pour stocker la couleur
            if (temp == NULL) {
                printf("Erreur : impossible d'allouer suffisamment d'espace mémoire pour temp\n");
                free(chaine); // Libère l'espace mémoire alloué pour la chaîne
                return NULL;
            }
            strcpy(temp, matrice[i * colonne + j].couleur); // Copie la couleur dans temp
            strcat(chaine, temp);
            free(temp); // Libère l'espace mémoire alloué pour temp
        }
    }
    return chaine;
}

/**
 * Renvoie une chaîne de caractères représentant la taille de la grille.
 *
 * @param l     le nombre de lignes de la grille
 * @param c     le nombre de colonnes de la grille
 *
 * @return      une chaîne de caractères de la forme "cxl", où c est le nombre de colonnes et l est le nombre de lignes.
 *              La chaîne renvoyée est allouée dynamiquement et doit être libérée par l'appelant après utilisation.
 * @ingroup fonct_jeu
 */
char *getSize(int l, int c){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%dx%d", c, l);
    return resultat;
}

/**
 * @brief Récupère la limite minimale de pixel pour une image de dimensions spécifiées.
 * 
 * @param l Le nombre de lignes de l'image.
 * @param c Le nombre de colonnes de l'image.
 * @param pix_Min Le pixel minimum de l'image.
 * 
 * @return Un pointeur vers une chaîne de caractères représentant la limite minimale de pixel.
 * @ingroup fonct_jeu
 */
char *getLimits(int l, int c,int pix_Min){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "%d", pix_Min);
    return resultat;
}

/**
 * @brief Renvoie la version actuelle du programme.
 *
 * @return un pointeur vers une chaîne de caractères représentant la version du programme.
 * @ingroup fonct_jeu
 */
char *getVersion(){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    sprintf(resultat, "1");
    return resultat;
}

/**
 * @brief Calcule le temps restant avant que le client ne puisse envoyer une autre requête.
 *
 * @param liste un pointeur vers la liste des clients connectés.
 * @param i l'identifiant du client.
 * 
 * @return un pointeur vers une chaîne de caractères représentant le temps d'attente restant en secondes.
 * @ingroup fonct_jeu
 */
char *getWaitTime(CLIENT *liste, int i){
	char *resultat = (char *) malloc(LG_MESSAGE * sizeof(char));
    
    CLIENT *curr = liste;
    while (curr != NULL && curr->id != i) {
        curr = curr->suiv;
    }
    if (time(0)-curr->timer>=60 || curr->pixel>0)
    {
        sprintf(resultat, "%d", 0);
    }else
    {
        sprintf(resultat, "%ld", 60-(time(0)-curr->timer));
    }
    return resultat;
}

/**
 * @brief Sélectionne un mot spécifique dans une phrase donnée en fonction de sa position.
 *
 * @param phrase une chaîne de caractères représentant la phrase à analyser.
 * @param nombre la position du mot à sélectionner dans la phrase.
 * @param separateur une chaîne de caractères représentant le séparateur entre les mots dans la phrase.
 * @param mot un pointeur vers une chaîne de caractères où le mot sélectionné sera stocké.
 * @ingroup fonct_jeu
 */
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
}
//---------------------------------------//


//-------------FONCTION RESEAU-------------//
/**
 * @brief Crée une nouvelle socket TCP/IP.
 *
 * @return Retourne l'identifiant de la socket créée.
 * @ingroup fonct_reseau
 */
int createSocket() {
    int socketEcoute = socket(PF_INET, SOCK_STREAM, 0);
    if (socketEcoute == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket créée avec succès ! (%d)\n", socketEcoute);
    return socketEcoute;
}

/**
 * @brief Associe la socket à un port spécifié.
 *
 * @param socketEcoute Identifiant de la socket à associer au port.
 * @param port Port à associer à la socket.
 * @ingroup fonct_reseau
 */
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

/**
 * @brief Place la socket en mode d'écoute passive.
 *
 * @param socketEcoute Identifiant de la socket à placer en écoute passive.
 * @ingroup fonct_reseau
 */
void listenSocket(int socketEcoute) {
    if (listen(socketEcoute, 5) == -1) {
        perror("Erreur lors de la mise en écoute du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket placée en écoute passive ...\n");
}

/**
 * interpretationMsg : Fonction qui analyse un message reçu et effectue une action en fonction de son contenu.
 *
 * @param matrice : Pointeur vers une matrice de type CASE.
 * @param l : Taille de la matrice en longueur.
 * @param c : Taille de la matrice en largeur.
 * @param messageRecu : Tableau de caractères contenant le message reçu.
 * @param liste_client : Pointeur vers la liste des clients connectés.
 * @param i : Identifiant du client qui a envoyé le message.
 * @param maxPixel : Nombre maximum de pixels qu'un client peut allouer à la matrice.
 *
 * @return : Un pointeur vers un tableau de caractères contenant la réponse de la commande.
 * @ingroup fonct_reseau
 */
char* interpretationMsg(CASE *matrice, int l, int c,char messageRecu[LG_MESSAGE], CLIENT *liste_client, int i,int maxPixel){

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
            if(strlen(couleur)>5 || strcmp(couleur,"12\0")==0){
                return "12 Bad Color\0";// Bad color
            }
            strcpy(couleur, base64_decode(couleur));
            if (strlen(couleur)==9)
            {
                CLIENT *curr = liste_client;
                while (curr != NULL && curr->id != i) {
                    curr = curr->suiv;
                }

                if (curr->pixel>0 || time(0)-curr->timer>=60)
                {
                    if (strcmp(setPixel(matrice, posL, posC, l, c, couleur),"00 OK\0")==0)
                    {
                        if (curr->pixel>0)
                        {
                            liste_client=deduirePixel(liste_client, i);
                            curr->timer=time(0);
                        }else{
                            curr->timer=time(0);
                            curr->pixel=maxPixel;
                        }
                        afficher_id_clients(liste_client);
                        return "00 OK\0";
                    } else{
                        return "11 Out Of Bound\0";
                    }
                    
                }else
                {
                    return "20 Out Of Quota\0";
                }              
                
            }else
            {
                return "12 Bad Color\0";
            }

        } else if(strcmp(prMot,"/getSize\0")==0){
            return getSize(l,c);

        } else if(strcmp(prMot,"/getMatrix\0")==0){
            
            //printf("matrice commande selected\n");
            char *data= getMatrice(matrice, l, c);
            printf("matrice ASCII: %s \n", data);
            //printf("matrice commande executed\n");
            return base64_encode(data);

        } else if(strcmp(prMot,"/getLimits\0")==0){
            return getLimits(l, c, 5);

        } else if(strcmp(prMot,"/getVersion\0")==0){
            return getVersion();

        } else if(strcmp(prMot,"/getWaitTime\0")==0){
            return getWaitTime(liste_client, i);
            
        } else{
            return "99 Unknown Command\0";
        }
}
//---------------------------------------//


//-------------FONCTION CLIENT-------------//

/**
 * supprimeClient : Fonction qui supprime un client de la liste des clients connectés en fonction de son identifiant.
 *
 * @param liste : Pointeur vers la liste des clients connectés.
 * @param idSup : Identifiant du client à supprimer.
 *
 * @return : Un pointeur vers la nouvelle liste des clients connectés.
 * @ingroup fonct_client
 */
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

/**
 * creerClient : Fonction qui crée un nouveau client.
 *
 * @param maxPixel : Le nombre maximal de pixels que le client peut envoyer.
 *
 * @return : Un pointeur vers le nouveau client créé.
 * @ingroup fonct_client
 */
CLIENT* creerClient(int maxPixel){
	CLIENT *pers = malloc(sizeof(CLIENT));
	pers->suiv=NULL;
	pers->id=0;
	pers->client_fds = -1;
	pers->fds.fd = -1;
	pers->fds.events = POLLIN;
    pers->timer=0;
    pers->pixel=maxPixel;
	return pers;
}

/**
 * ajouterClient : Fonction qui ajoute un client à la liste des clients connectés.
 *
 * @param liste : Pointeur vers la liste des clients connectés.
 * @param pers : Pointeur vers la structure CLIENT contenant les informations du client à ajouter.
 *
 * @return : Un pointeur vers la nouvelle liste des clients connectés.
 * @ingroup fonct_client
 */
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

/**
 * actualiseClient : Fonction qui actualise l'état des clients en fonction des événements du poll.
 *
 * @param liste_client : Pointeur vers la liste des clients connectés.
 * @param fds : Tableau de pollfd contenant les événements à surveiller pour chaque client.
 *
 * @return : Un pointeur vers la liste des clients connectés mise à jour.
 * @ingroup fonct_client
 */
CLIENT *actualiseClient(CLIENT *liste_client, struct pollfd *fds) {
    struct CLIENT *ptr = liste_client;

    // Parcourir la liste chainée
    while (ptr != NULL) {
        int client_fd = ptr->fds.fd;

        // Trouver le pollfd correspondant
        int i;
        for (i = 1; i < MAX_CLIENT; i++) {
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

/**
 * deduirePixel : Fonction qui déduit un pixel du compte d'un client en fonction de son identifiant.
 *
 * @param liste : Pointeur vers la liste des clients connectés.
 * @param i : Identifiant du client dont on veut déduire un pixel.
 *
 * @return : Un pointeur vers la nouvelle liste des clients connectés.
 * @ingroup fonct_client
 */
CLIENT *deduirePixel(CLIENT *liste, int i){
    CLIENT *curr = liste;
    //printf("l'ID à déduire: %d\n", i);
    
    while (curr != NULL && curr->id != i) {
        curr = curr->suiv;
    }
    if (curr->pixel>0)
    {
        curr->pixel--;
    }
    
    //printf("l'ID %d a un nombre de pixel de : %d\n", i, curr->pixel);
    
    return liste;
}
//---------------------------------------//


