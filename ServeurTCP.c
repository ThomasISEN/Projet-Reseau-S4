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
char *selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1]);


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
	//setPixel(matrice, 1, 1, "000000000");
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

				char *prMot;//le premier mot de la commande
				prMot = (char*)calloc(LG_MESSAGE, sizeof(char));
				strcpy(prMot,selectMot(messageRecu, 1, " "));
				printf("premier mot: %s\n",prMot);
				if(strcmp(prMot,"/setPixel")==0){
					printf("set le pixel\n");
					
					char *place;// l'emplacement du pixel
					place = (char*)calloc(LG_MESSAGE, sizeof(char));
					strcpy(place,selectMot(messageRecu, 2, " "));
					
					char *x;// x du pixel
					x = (char*)calloc(LG_MESSAGE, sizeof(char));
					strcpy(x,selectMot(place, 1, "x"));

					char *y;// x du pixel
					y = (char*)calloc(LG_MESSAGE, sizeof(char));
					strcpy(y,selectMot(place, 2, "x"));

					printf("\nposition: %sx%s\n", x, y);
				}
				
				else if(strcmp(messageRecu,"/getSize\n")==0){
					strcpy(messageEnvoi,getSize());
				}else if(strcmp(messageRecu,"/getMatrice\n")==0){
					strcpy(messageEnvoi,getMatrice(matrice));
				}else if(strcmp(messageRecu,"/getLimits\n")==0){
					strcpy(messageEnvoi,getLimits(10));
				}else if(strcmp(messageRecu,"/getVersion\n")==0){
					strcpy(messageEnvoi,getVersion());
				}else if(strcmp(messageRecu,"/getWaitTime\n")==0){
					strcpy(messageEnvoi,getWaitTime(60));
				}else{
					printf("Message reçu : %s (%d octets)\n\n", messageRecu, lus);	
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
	matstr = (char*)calloc(10, sizeof(char));// a changer L*C
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
char *selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char separateur[1] ){
	//faire gestion erreur pas de mot / nombre trop grand ou trop petit
	char *prMot;
	prMot = (char*)calloc(LG_MESSAGE, sizeof(char));
	int i,j,cpt;
	i=0;j=0;cpt=1;
	//printf("ouii");
	while (phrase[i]!='\n'  && phrase[i]!='\0')
	{
		//printf("%d", i);
		if (cpt==nombre)//on regarde si on est au mot que l'on veut
		{
			prMot[j]=phrase[i];
			j++;
		}			
		//printf("%d/%d\n",phrase[i],*separateur);
		i++;
		if (phrase[i]==*separateur)// on passe au mot suivant //strcmp(phrase[i],separateur)==0
		{
			i++;
			cpt++;
		}
	}
	return prMot;
}


void stripFunc(char phrase[LG_MESSAGE*sizeof(char)]){
    char newString[10][10]; 
    int i,j,ctr; 
    j=0; ctr=0;

    for(i=0;i<=(strlen(phrase));i++)
    {
        // if space or NULL found, assign NULL into newString[ctr]
        if(phrase[i]==' '||phrase[i]=='\0')
        {
            newString[ctr][j]='\0';
            ctr++;  //for next word
            j=0;    //for next word, init index to 0
        }
        else
        {
            newString[ctr][j]=phrase[i];
            j++;
        }
    }
    printf("\n Strings or words after split by space are :\n");
    for(i=0;i < ctr;i++)
        printf(" %s\n",newString[i]);
} 