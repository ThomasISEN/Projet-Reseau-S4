#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */


#define PORT IPPORT_USERRESERVED // = 5000
#define LG_MESSAGE 256
#define L 4
#define C 8
#define VERSION 1.5

typedef struct CASE{
	char couleur[10]; //couleur format RRRGGGBBB

}CASE;

void initMartice(CASE matrice[L][C]);
void setPixel(CASE matrice[L][C], int posL, int posC, char val[10]);
char *getMatrice(CASE matrice[L][C]);
char *getSize();
char *getLimits(int pixMin);
char *getVersion();
char *getWaitTime(int timer);
void stripFunc(char phrase[LG_MESSAGE*sizeof(char)]);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1], char *mot);
void afficheMatrice(CASE matrice[L][C]);


int main()
{
	int socketEcoute;
	struct sockaddr_in pointDeRencontreLocal;
	socklen_t longueurAdresse;
	int socketDialogue;
	struct sockaddr_in pointDeRencontreDistant;
	char messageEnvoi[LG_MESSAGE];/* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE];/* le message de la couche Application ! */
	int ecrits, lus;/* nb d’octets ecrits et lus */
	int retour;
	CASE matrice[L][C];
	initMartice(matrice);

	// Crée un socket de communication
	socketEcoute = socket(PF_INET, SOCK_STREAM, 0);
	/* 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP */

	// Teste la valeur renvoyée par l’appel système socket()
	if(socketEcoute < 0)/* échec ? */
	{
		perror("socket");// Affiche le message d’erreur
		exit(-1);// On sort en indiquant un code erreur
	}

	printf("Socket créée avec succès ! (%d)\n", socketEcoute);


	// On prépare l’adresse d’attachement locale
	longueurAdresse =sizeof(struct sockaddr_in);
	memset(&pointDeRencontreLocal, 0x00, longueurAdresse);
	pointDeRencontreLocal.sin_family = PF_INET;
	pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY);// toutes les interfaces locales disponibles
	pointDeRencontreLocal.sin_port = htons(PORT);

	// On demande l’attachement local de la socket
	if((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
		{
			perror("bind");
			exit(-2);
		}
	printf("Socket attachée avec succès !\n");

	// On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
	if(listen(socketEcoute, 5) < 0){
		perror("listen");
		exit(-3);
	}

	printf("Socket placée en écoute passive ...\n");

	// boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
	while(1)
	{
		memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
		memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));
		printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");
		// c’est un appel bloquant
		socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
		if(socketDialogue < 0)
		{
			perror("accept");
			close(socketDialogue); 
			close(socketEcoute);
			exit(-4);
		}

		printf("Connexion établie avec %s:", inet_ntoa(pointDeRencontreDistant.sin_addr) );
		printf("%d\n",ntohs(pointDeRencontreDistant.sin_port));
		
		// On réceptionne les données du client (cf. protocole)
		lus = read(socketDialogue, messageRecu, LG_MESSAGE*sizeof(char));// ici appel bloquant
		switch(lus)
		{
			case-1 :/* une erreur ! */
				perror("read");
				close(socketDialogue);
				exit(-5);
			case 0 :/* la socket est fermée */
				fprintf(stderr, "La socket a été fermée par le client !\n\n");
				close(socketDialogue);
				return 0;
			default:/* réception de n octets */

				
				char prMot[LG_MESSAGE]; //le premier mot de la commande
				printf("message recu: %s\n", messageRecu);
				selectMot(messageRecu, 1, " ", prMot);
				printf("premier mot: '%s'\n",prMot);

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
					setPixel(matrice, yInt, xInt, couleur);
					strcpy(messageEnvoi," ");
					//afficheMatrice(matrice);

				} else if(strcmp(messageRecu,"/getSize\n")==0){
					strcpy(messageEnvoi,getSize());
				} else if(strcmp(messageRecu,"/getMatrice\n")==0){
					strcpy(messageEnvoi,getMatrice(matrice));
				} else if(strcmp(messageRecu,"/getLimits\n")==0){
					strcpy(messageEnvoi,getLimits(10));
				} else if(strcmp(messageRecu,"/getVersion\n")==0){
					strcpy(messageEnvoi,getVersion());
				} else if(strcmp(messageRecu,"/getWaitTime\n")==0){
					strcpy(messageEnvoi,getWaitTime(60));
				} else{
					printf("Message reçu : %s (%d octets)\n\n", messageRecu, lus);
					strcpy(messageEnvoi,"Bad Command");
				}
				

		}

		// On envoie des données vers le client (cf. protocole)
		ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
		switch(ecrits)
		{
			case-1 :/* une erreur ! */
				perror("write");
				close(socketDialogue);
				exit(-6);
			case 0:/* la socket est fermée */
				fprintf(stderr, "La socket a été fermée par le client !\n\n");
				close(socketDialogue);
				return 0;
			default:/* envoi de n octets */
				printf("%s\n(%d octets)\n\n", messageEnvoi, ecrits);
		}

		// On ferme la socket de dialogue et on se replace en attente ...
		close(socketDialogue);
	}

	// On ferme la ressource avant de quitter
	close(socketEcoute);
	return 0;
}


void initMartice(CASE matrice[L][C]){
	for (int i = 0; i < L; ++i)//parcours des lignes
	{
		for (int j = 0; j < C; ++j)//parcours des colonnes 
		{
			strcpy(matrice[i][j].couleur,"255255255");
		}
	}
}

void setPixel(CASE matrice[L][C], int posL, int posC, char val[10]){
	strcpy(matrice[posL][posC].couleur,val);
}

char* getMatrice(CASE matrice[L][C]) {
    char* matstr = NULL;
    int matstr_size = 0;
    for (int i = 0; i < L; ++i) {
        for (int j = 0; j < C; ++j) {
            char* couleur = matrice[i][j].couleur;
            int couleur_size = strlen(couleur);
			matstr_size += couleur_size;
            matstr = (char*)realloc(matstr, matstr_size + 1);// +1 pour le \0
            strcat(matstr, couleur);   
        }
    }
    return matstr;
}

char *getSize(){//C et L paramettre de serveur
	char *size;
	size = (char*)calloc(20, sizeof(char));
	sprintf(size, "%d", L);
	strcat(size, "x");
	char temp[3];
	sprintf(temp, "%d", C);
	strcat(size,temp);
	return size;
}

char *getLimits(int pixMin){//pixMin paramettre de serveur
	char *pixel;
	pixel = (char*)calloc(3, sizeof(char));
	sprintf(pixel, "%d", pixMin);
	return pixel;
}

char *getVersion(){
	char *version;
	version = (char*)calloc(3, sizeof(char));
	sprintf(version, "%f", VERSION);
	return version;
}

char *getWaitTime(int timer){//A identifier par rapport à l'IP client 
	char *time;
	time = (char*)calloc(3, sizeof(char));
	sprintf(time, "%d", timer);
	return time;
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