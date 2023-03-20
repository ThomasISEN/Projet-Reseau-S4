#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */
#include <ncurses.h>

#define LG_MESSAGE 256
#define PORT IPPORT_USERRESERVED // = 5000

void mainClient(int socketEcoute, WINDOW *boite);
int createSocket();
void bindSocket(int socketEcoute, int port, char* ip);
void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c, WINDOW *boite);
//int espace(char messageRecu[LG_MESSAGE]);
void interpretationMatrice(char messageRecu[LG_MESSAGE], int l, int c);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char *mot);
//void afficheMatrice();
char *affichage(WINDOW *boite);
void affichageEntree(int socketEcoute);

int main(int argc, char *argv[]){

	int socketEcoute;

	//Interpretation de la commande du lancement serveur
	int opt;
    int port=0;
	char *ip = NULL;
	// Vérifie que la commande a la forme attendue
    if (argc != 5) {
        fprintf(stderr, "Usage: %s [-i IP] [-p PORT]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    while ((opt = getopt(argc, argv, "i:p:")) != -1) {
        switch (opt) {
            case 'i':
                ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-i IP] [-p PORT]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }


	// Crée un socket de communication
	socketEcoute = createSocket(); 
	//configuration du socket
	bindSocket(socketEcoute, port, ip);
	printf("Connexion au serveur réussie avec succès !\n");


	//boucle principale
	affichageEntree(socketEcoute);
	//mainClient(socketEcoute);
	
	return 0;
}

void mainClient(int socketEcoute, WINDOW *boite){

	int ecrits, lus;/* nb d’octets ecrits et lus */
	char messageEnvoi[LG_MESSAGE];/* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE];/* le message de la couche Application ! */

	// Initialise à 0 les messages
	memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
	memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));

	char* limit="/getSize";
	strcpy(messageEnvoi, limit);
	ecrits = write(socketEcoute, messageEnvoi, strlen(messageEnvoi));
	lus = read(socketEcoute, messageRecu, LG_MESSAGE*sizeof(char));

	char strL[LG_MESSAGE];
	char strC[LG_MESSAGE];
	selectMot(messageRecu, 1, strC);
	selectMot(messageRecu, 2, strL);
	int l=atoi(strL);
	int c=atoi(strC);
	//printf("taille:%dx%d\n", l,c);


	// Envoie un message au serveur
	char phrase[LG_MESSAGE*sizeof(char)];
	while (phrase!="\n")
	{
		//fgets(phrase, sizeof(phrase), stdin);
		strcpy(messageEnvoi, affichage(boite));
		ecrits = write(socketEcoute, messageEnvoi, strlen(messageEnvoi));// message à TAILLE variable
		switch(ecrits)
		{
			case-1 :/* une erreur ! */
				perror("write");
				close(socketEcoute);
				exit(-3);
			case 0 :/* la socket est fermée */
				fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
				close(socketEcoute);
				exit(-1);
			default:/* envoi de n octets */
			printf(" ");
		}

		/* Reception des données du serveur */
		lus = read(socketEcoute, messageRecu, LG_MESSAGE*sizeof(char));/* attend un message de TAILLE fixe */
		switch(lus)
		{
			case-1 :/* une erreur ! */
				perror("read");
				close(socketEcoute);
				exit(-4);
			case 0 :/* la socket est fermée */
				fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
				close(socketEcoute);
				exit(-1);
			default:/* réception de n octets */
				messageRecu[lus]='\0';
				//printf("Message reçu du serveur : %s (%d octets)\n\n", messageRecu, lus);
				interpretationMsg(messageRecu, messageEnvoi, l, c, boite);
				
		}
	}
	close(socketEcoute);
	endwin();
	free(boite);
}

int createSocket() {
    int socketEcoute = socket(PF_INET, SOCK_STREAM, 0);// 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP
    if (socketEcoute == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket créée avec succès ! (%d)\n", socketEcoute);
    return socketEcoute;
}

void bindSocket(int socketEcoute, int port, char* ip){
	// les info serveur
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); //port serv
	serv_addr.sin_addr.s_addr = inet_addr(ip); //ip serv

	// connexion vers le serveur
	if((connect(socketEcoute, (struct sockaddr *)&serv_addr,sizeof(serv_addr))) == -1){
		perror("Connexion vers le serveur à échouée");// Affiche le message d’erreur
		close(socketEcoute);// On ferme la ressource avant de quitter
		exit(-2);// On sort en indiquant un code erreur
	}

}

void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c, WINDOW *boite){
	if(strcmp(messageRecu,"00\0")==0){
        mvprintw(LINES - 1, 0, "OK");
	} else if(strcmp(messageRecu,"10\0")==0){
		mvprintw(LINES - 1, 0, "Bad Command");
	} else if(strcmp(messageRecu,"11\0")==0){
		mvprintw(LINES - 1, 0, "Pixel out of band");
	} else if(strcmp(messageRecu,"12\0")==0){
		mvprintw(LINES - 1, 0, "Bad color");
	} else if(strcmp(messageRecu,"20\0")==0){
		mvprintw(LINES - 1, 0, "Out of quota");
	} else{
		//printf("le message envoyé:'%s'\n",messageEnvoi);

		if (strcmp(messageEnvoi,"/getSize\0")==0){
			mvprintw(LINES - 1, 0, "la taille de la matrice est de: %s", messageRecu);
			refresh();
		}else if (strcmp(messageEnvoi,"/getLimits\0")==0){
			mvprintw(LINES - 1, 0, "%s pixel par minutes", messageRecu);
			refresh();
		}else if (strcmp(messageEnvoi,"/getVersion\0")==0){
			mvprintw(LINES - 1, 0, "Version %s", messageRecu);
			refresh();
		}else if (strcmp(messageEnvoi,"/getWaitTime\0")==0){
			mvprintw(LINES - 1, 0, "%s secondes à attendre avant de pouvoir envoyer un nouveau pixel", messageRecu);
			refresh();
		}else{
			//printf("AFFICHAGE MATRICE\n");
			interpretationMatrice(messageRecu, l, c);
		}
		
	}
}

// int espace(char messageRecu[LG_MESSAGE]){
// 	int i=0;
// 	while (messageRecu[i]!='\0')
// 	{
// 		if (messageRecu[i]==' ')
// 		{
// 			return 0;
// 		}
// 		i++;
// 	}
// 	return 1;
// }

void interpretationMatrice(char messageRecu[LG_MESSAGE], int l, int c){
	static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t message_len = strlen(messageRecu);
    size_t padding = 0;
	
	//BASE64->UTF-8
    // Compter le nombre de caractères de padding
    if (messageRecu[message_len - 1] == '=')
        padding++;
    if (messageRecu[message_len - 2] == '=')
        padding++;

    // Calculer la longueur de la sortie
    size_t output_len = message_len * 3 / 4 - padding;
	char *messageEnvoi = (char*)malloc(output_len);

    int i, j;
    unsigned int bits = 0;
    for (i = 0, j = 0; i < message_len; i++)
    {
        int index = strchr(base64_table, messageRecu[i]) - base64_table;

        if (index >= 0 && index <= 63)
        {
            bits = (bits << 6) | index;
            if (i % 4 == 3)
            {
                (messageEnvoi)[j++] = (bits >> 16) & 0xff;
                (messageEnvoi)[j++] = (bits >> 8) & 0xff;
                (messageEnvoi)[j++] = bits & 0xff;
            }
        }
    }

    if (padding == 1)
        (messageEnvoi)[j++] = (bits >> 10) & 0xff;
    else if (padding == 2)
        (messageEnvoi)[j++] = (bits >> 16) & 0xff;

	//AFFICHAGE
	printf("------Matrice-----\n");
    //printf("%s\n\n",messageEnvoi);
	//printf("l:%d c:%d", l, c);
	int matrice[l][c];
	int k=0;
	for (int i = 0; i < l; i++)
	{
		for (int j = 0; j < c*3; j++)
		{
			char nombre[3];
			nombre[0]=messageEnvoi[(i*c*3)+k];
			nombre[1]=messageEnvoi[((i*c*3)+k)+1];
			nombre[2]=messageEnvoi[((i*c*3)+k)+2];
			matrice[i][j]=atoi(nombre);
			printf("%d",matrice[i][j]);//Affichage a améliorer et stockage aussi si besoin
			k+=3;
			
		}
		printf("\n");
		k=0;
		
	}
	//afficheMatrice();
	
	
	free(messageEnvoi);
}

void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char *mot){
	int i=0, j=0, cpt=1;
	//printf("message recu dans le selectMot: %s", phrase);
	while (phrase[i]!='\n'  && phrase[i]!='\0')
	{
		if(phrase[i]=='x'){
			cpt++;
			i++;
		}
		if (cpt==nombre)//on regarde si on est au mot que l'on veut
		{
			//printf("le chiffre: %c",phrase[i]);
			mot[j]=phrase[i];
			j++;
			i++;
		}else{
			i++;
		}
	}
	mot[j]='\0';
}

void affichageEntree(int socketEcoute){
	WINDOW *boite;
	initscr();
	char *msgA[] = {
		"########  #### ##     ## ######## ##       ##      ##    ###    ########  ",
		"##     ##  ##   ##   ##  ##       ##       ##  ##  ##   ## ##   ##     ## ",
		"########   ##     ###    ######   ##       ##  ##  ## ##     ## ########  ",
		"##         ##    ## ##   ##       ##       ##  ##  ## ######### ##   ##   ",
		"##         ##   ##   ##  ##       ##       ##  ##  ## ##     ## ##    ##  ",
		"##        #### ##     ## ######## ########  ###  ###  ##     ## ##     ## ",
		"appuyer sur 1 pour aller au menu"
	};
	int taille= strlen(msgA[1]);

	clear();    // Efface la fenêtre (donc l'ancien message)
	mvprintw((LINES/2)-2, (COLS / 2) - (taille / 2), msgA[0]);
	mvprintw((LINES/2)-1, (COLS / 2) - (taille / 2), msgA[1]);
	mvprintw((LINES/2), (COLS / 2) - (taille / 2), msgA[2]);
	mvprintw((LINES/2)+1, (COLS / 2) - (taille / 2), msgA[3]);
	mvprintw((LINES/2)+2, (COLS / 2) - (taille / 2), msgA[4]);
	mvprintw((LINES/2)+3, (COLS / 2) - (taille / 2), msgA[5]);
	mvprintw((LINES)-2, (COLS/2) - (strlen(msgA[6])/2), msgA[6]);
	refresh();

	noecho();
    int key = getch(); // attendre l'appui d'une touche
	while (key != '1' && key != '&'){
		key = getch();
	}
	clear();
	mainClient(socketEcoute, boite);
}

char *affichage(WINDOW *boite){
	

	char *msgM[] = {
		"1. Placer un pixel",
		"2. Taille de la matrice",
		"3. Limite de pixel par minute",
		"4. Version du jeu",
		"5. Temps d'attente avant recharge",
		"0. Quitter",
		"Appuyer sur le chiffre correspondant pour sélectionner une option"
	};
	// Afficher les options du menu
	mvprintw(LINES/2 - 3, (COLS/2) - (strlen(msgM[0])/2), msgM[0]);
	mvprintw(LINES/2 - 2, (COLS/2) - (strlen(msgM[1])/2), msgM[1]);
	mvprintw(LINES/2 - 1, (COLS/2) - (strlen(msgM[2])/2), msgM[2]);
	mvprintw(LINES/2, (COLS/2) - (strlen(msgM[3])/2), msgM[3]);
	mvprintw(LINES/2 + 1, (COLS/2) - (strlen(msgM[4])/2), msgM[4]);
	mvprintw(LINES/2 + 2, (COLS/2) - (strlen(msgM[5])/2), msgM[5]);

	// Afficher le message pour sélectionner une option
	mvprintw((LINES)-2, (COLS/2) - (strlen(msgM[6])/2), msgM[6]);
	refresh(); // rafraîchir l'affichage

	move(COLS - 1, 0);
	int key=getch();
	while (key != 'a' && key != 'z' && key != 'e' && key != 'r' && key != 't' && key != 'y')
	{
		key=getch();
	}
	if (key == 'a')
	{
		/* SetPixel */
		echo();
		clear();
		char phrase[50];
		getnstr(phrase, 100);
		char *messageFinal = malloc(100 * sizeof(char)); // allocation de la mémoire
		sprintf(messageFinal, "/setPixel %s", phrase);
		clear();
		noecho();
		return messageFinal;

	}else if (key == 'z')
	{
		/* getSize */
		clear();
		return "/getSize\0";
	}else if (key == 'e')
	{
		/* getLimits */
		clear();
		return "/getLimits\0";
	}else if (key == 'r')
	{
		/* getVersion */
		clear();
		return "/getVersion\0";
	}else if (key == 't')
	{
		/* getWaitTime */
		clear();
		return "/getWaitTime\0";
	}else if (key == 'y')
	{
		endwin();
		free(boite);
		return NULL; 
	}
	
}

void affichageRecu(char messageRecu[LG_MESSAGE]){

}