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

#define LG_MESSAGE 1024
#define MAX_PIXEL 5

typedef struct CASE{
	char couleur[10]; //couleur format RRRGGGBBB

}CASE;

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
char* interpretationMsg(CASE *matrice, int l, int c,char messageRecu[LG_MESSAGE], CLIENT *liste_client, int i);
void mainServeur( CASE *matrice, int l, int c, int socketEcoute, CLIENT *liste_client, int maxClients);
CLIENT* creerClient();
CLIENT* ajouterClient(CLIENT* liste, CLIENT* pers);
CLIENT* supprimeClient(CLIENT *liste, int idSup);
CLIENT *actualiseClient(CLIENT *liste_client, struct pollfd *fds, int maxClients);
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
    int port=0, c=0, l=0, maxClients=0;

	// Vérifie que la commande a la forme attendue
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <-p PORT> <-s LxH> <-l MAX_CLIENTS>\n", argv[0]);
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
                fprintf(stderr, "Usage: %s <-p PORT> <-s LxH> <-l MAX_CLIENTS>\n", argv[0]);
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
                    client->timer=time(0);
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
                    printf("Message reçu de %s:%d : '%s'\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), messageRecu);                    
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
                    printf("%s\n(%d octets)\n\n", messageEnvoi, ecrits);
                }
            }

        }
    }

	// Fermeture du socket
    close(socketEcoute);

}



//-------------FONCTION DE JEU-------------//
char* base64_encode(const char* rgb) {
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t input_length = strlen(rgb);
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(output_length + 1);
    if (encoded_data == NULL) return NULL;

    size_t i, j;
    uint32_t octet_a, octet_b, octet_c, triple;

    char*nombre = malloc(3*sizeof(char));
    int tableauRGB[input_length/3];
    int cpt=0;

    for (i = 0; i < input_length;) {
        nombre[0] = rgb[i++];
        nombre[1] = rgb[i++];
        nombre[2] = rgb[i++];

        tableauRGB[cpt]= atoi(nombre);
        cpt++;
    }

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

    // Ajoute les caractères '=' uniquement si nécessaire
    for (i = 0; i < (3 - (j % 3)) % 3; i++) {
        encoded_data[j++] = '=';
    }

    encoded_data[j] = '\0';
    return encoded_data;
}

char* base64_decode(const char* messageRecu) {
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t input_length = strlen(messageRecu);

    printf("%s\n", messageRecu);
    size_t output_length = input_length / 4 * 3;

    unsigned char* messageDecode = malloc(output_length + 1);
    size_t i, j;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;

    int* tableauRGB = malloc(output_length * sizeof(int));
    int cpt = 0;

    for (i = 0, j = 0; i < input_length;) {
        sextet_a = (messageRecu[i] == '=') ? 0 : strchr(base64_chars, messageRecu[i]) - base64_chars;
        sextet_b = (messageRecu[i+1] == '=') ? 0 : strchr(base64_chars, messageRecu[i+1]) - base64_chars;
        sextet_c = (messageRecu[i+2] == '=') ? 0 : strchr(base64_chars, messageRecu[i+2]) - base64_chars;
        sextet_d = (messageRecu[i+3] == '=') ? 0 : strchr(base64_chars, messageRecu[i+3]) - base64_chars;

        triple = (sextet_a << 3 * 6)
               + (sextet_b << 2 * 6)
               + (sextet_c << 1 * 6)
               + (sextet_d << 0 * 6);

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


void initMatrice(CASE *matrice, int taille){
    for(int i = 0; i < taille; i++) {
        strcpy(matrice[i].couleur,"255255255");
    }
    printf("matrice initialisée avec succes.\n");
}

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
    sprintf(resultat, "1");
    return resultat;
}

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
                            curr->pixel=MAX_PIXEL;
                        }
                        afficher_id_clients(liste_client);
                        return "00 OK\0";
                    } else{
                        return "11 Out Of Bound\0";
                    }
                    
                }else
                {
                    return "20 Out Of Quota\0"; //Out of quota
                }              
                
            }else
            {
                return "12 Bad Color\0";// Bad color
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
    pers->pixel=MAX_PIXEL;
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
    if (curr->pixel>0)
    {
        curr->pixel--;
    }else
    {
        /* code */
    }
    
    //printf("l'ID %d a un nombre de pixel de : %d\n", i, curr->pixel);
    
    return liste;
}
//---------------------------------------//


