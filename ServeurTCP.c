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
void getSize();
void getLimits(int pixMin);
void getVersion();
void getWaitTime(int timer);


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
	setPixel(matrice, 1, 1, "000000000");
	//printf("[%s]", matrice[1][1].couleur); //DEBUG



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

	//--- Début de l’étape n°7 :
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
			close(socketDialogue); //pas ca
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
				if(strcmp(messageRecu,"/getMatrice\n")==0){
					char *repo;
					repo = (char*)calloc(L*C*9+1, sizeof(char));//il y a 9 char par case et L*C cases +1
					strcpy(messageEnvoi,getMatrice(matrice));
					printf("%s", repo);
					//printf("On charge la Matrice...\n\n");
				}else if(strcmp(messageRecu,"/getSize\n")==0){
					getSize();
				}else if(strcmp(messageRecu,"/setPixel\n")==0){
					setPixel(matrice,1,1,"000000000");

				}else if(strcmp(messageRecu,"/getLimits\n")==0){
					getLimits(10);
				}else if(strcmp(messageRecu,"/getVersion\n")==0){
					getVersion();
				}else if(strcmp(messageRecu,"/getWaitTime\n")==0){
					getWaitTime(20);
				}else{
					printf("Message reçu : %s (%d octets)\n\n", messageRecu, lus);	
				}

		}

		// On envoie des données vers le client (cf. protocole)
		//sprintf(messageEnvoi, "ok\n");
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
	//--- Fin de l’étape n°7 !		

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

char *getMatrice(CASE matrice[L][C]){
	char *matstr;
	matstr = (char*)calloc(10, sizeof(char));
	strcpy(matstr,"");
	for (int i = 0; i < L; ++i)
	{
		for (int j = 0; j < C; ++j)
		{
			strcat(matstr, matrice[i][j].couleur);
		}
	}
	return matstr;
}

void getSize(){//C et L paramettre de serveur
	printf("taille de la Matrice: %dx%d\n", C,L);
}

void getLimits(int pixMin){//pixMin paramettre de serveur
	printf("%d pixels/min\n", pixMin);
}

void getVersion(){
	printf("Version: %f\n", VERSION);
}

void getWaitTime(int timer){//A identifier par rapport à l'IP client 
	printf("%d secondes à attendre", timer);
}