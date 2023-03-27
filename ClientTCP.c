#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */
#include <ncurses.h>

#define LG_MESSAGE 1024
#define PORT IPPORT_USERRESERVED // = 5000
#define COLOR_PIX 8

void mainClient(int socketEcoute, WINDOW *boite);
int createSocket();
void bindSocket(int socketEcoute, int port, char* ip);
void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c, WINDOW *boite);
//int espace(char messageRecu[LG_MESSAGE]);
void interpretationMatrice(char messageRecu[LG_MESSAGE], WINDOW *boite, int l, int c);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char *mot);
//void afficheMatrice();
char *affichage(WINDOW *boite);
void affichageEntree(int socketEcoute);
char* base64_encode(const char* rgb);

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
	if(strcmp(messageRecu,"00 OK\0")==0){
        mvprintw(LINES - 1, 0, "Validé");
	} else if(strcmp(messageRecu,"10 Bad Command\0")==0){
		mvprintw(LINES - 1, 0, "Mauvaise commande");
	} else if(strcmp(messageRecu,"11 Out Of Bound\0")==0){
		mvprintw(LINES - 1, 0, "Pixel en dehors de la matrice");
	} else if(strcmp(messageRecu,"12 Bad Color\0")==0){
		mvprintw(LINES - 1, 0, "Mauvaise Couleur");
	} else if(strcmp(messageRecu,"12 Bad Color\0")==0){
		mvprintw(LINES - 1, 0, "Vous n'avez plus de pixel, veuillez attendre");
	}else if(strcmp(messageRecu,"99 Unknown Command\0")==0){
		mvprintw(LINES - 1, 0, "Commande Inconue");
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
			interpretationMatrice(messageRecu, boite, l, c);
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

char* base64_encode(const char* rgb) {
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t input_length = strlen(rgb);
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(output_length + 1);
    if (encoded_data == NULL) return "12 Bad Color\0";
	if (strlen(rgb)%9 != 0) return "12 Bad Color\0";
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


void interpretationMatrice(char messageRecu[LG_MESSAGE], WINDOW *boite, int l, int c){
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	printw("                         %d", strlen(messageRecu));
    size_t input_length = strlen(messageRecu);

    size_t output_length = input_length / 4 * 3;
	
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

    // AFFICHAGE
	//mvprintw(0, (COLS / 2) - (strlen("------Matrice Decodee-----") / 2), "------Matrice Decodee-----");
	
	
	move(1,0);
	printw("'%ld'/lignes: %d/colones: %d", output_length,l,c);
	move(2,0);
	int color_r = 0;
	int color_g = 0;
	int color_b = 0;
	
	//start_color();
	for (int lignes = 1; lignes <= l; lignes++)
	{
		//printw("'%d'",(lignes-1));
		for (int i = 0; i < c*3;)
		{
			//printw("'%d'",tableauRGB[((lignes-1)*c*3)+i+2]* 1000 / 255);
			color_r = tableauRGB[((lignes-1)*c*3)+i] * 1000 / 255;
			color_g = tableauRGB[((lignes-1)*c*3)+i+1] * 1000 / 255;
			color_b = tableauRGB[((lignes-1)*c*3)+i+2] * 1000 / 255;
			init_color(COLOR_PIX, color_r, color_g, color_b);
			init_pair(1, COLOR_PIX, COLOR_RED);
			attron(COLOR_PAIR(1));
			printw("0");
			attroff(COLOR_PAIR(1));
			i+=3;

		}
		move(2+lignes,0);
	}
	
	

	
	refresh();
	getch();
	clear();
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
		"a. Placer un pixel",
		"z. Taille de la matrice",
		"e. Limite de pixel par minute",
		"r. Version du jeu",
		"t. Temps d'attente avant recharge",
		"y. Afficher la matrice",
		"q. Quitter",
		"Appuyer sur le chiffre correspondant pour sélectionner une option"
	};
	// Afficher les options du menu
	mvprintw(LINES/2 - 3, (COLS/2) - (strlen(msgM[0])/2), msgM[0]);
	mvprintw(LINES/2 - 2, (COLS/2) - (strlen(msgM[1])/2), msgM[1]);
	mvprintw(LINES/2 - 1, (COLS/2) - (strlen(msgM[2])/2), msgM[2]);
	mvprintw(LINES/2, (COLS/2) - (strlen(msgM[3])/2), msgM[3]);
	mvprintw(LINES/2 + 1, (COLS/2) - (strlen(msgM[4])/2), msgM[4]);
	mvprintw(LINES/2 + 2, (COLS/2) - (strlen(msgM[5])/2), msgM[5]);
	mvprintw(LINES/2 + 3, (COLS/2) - (strlen(msgM[6])/2), msgM[6]);

	// Afficher le message pour sélectionner une option
	mvprintw((LINES)-2, (COLS/2) - (strlen(msgM[7])/2), msgM[7]);
	refresh(); // rafraîchir l'affichage

	move(COLS - 1, 0);
	int key=getch();
	while (key != 'a' && key != 'z' && key != 'e' && key != 'r' && key != 't' && key != 'y' && key != 'q')
	{
		key=getch();
	}
	if (key == 'a')
	{
		/* SetPixel */
		echo();
		clear();
		char position[50];
		getnstr(position, 50);
		char couleur[50];
		getnstr(couleur, 50);
		strcpy(couleur,base64_encode(couleur));
		
		char *messageFinal = malloc(100 * sizeof(char)); // allocation de la mémoire
		sprintf(messageFinal, "/setPixel %s %s", position, couleur);
		clear();
		printw("le msg: %s", messageFinal);
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
		/* getWaitTime */
		clear();
		return "/getMatrix\0";
	}else if (key == 'q')
	{
		endwin();
		free(boite);
		return NULL; 
	}
	
}

void affichageRecu(char messageRecu[LG_MESSAGE]){

}