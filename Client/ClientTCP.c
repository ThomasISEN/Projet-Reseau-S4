/**
 * \file ClientTCP.c
 * \brief Gestion du client
 * \author Denis Arrahmani Massard
 * \version 1
 * \date 31 mars 2023
 *
 */

#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */
#include <ncurses.h>

/**
 * @defgroup fonct_duClient Fonctions du client 
 * Fonctions qui permettent de faire fonctionner le client
 */


#define LG_MESSAGE 1024


void mainClient(int socketEcoute);
int createSocket();
void bindSocket(int socketEcoute, int port, char* ip);
void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c, int* matrice);
void interpretationMatrice(const char messageRecu[LG_MESSAGE], const int l, const int c, int *matrice);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char *mot);
char *affichage();
void affichageEntree(int socketEcoute);
char* base64_encode(const char* rgb);
void afficherCouleurs(int* tableauRGB, const int l, const int c);

int main(int argc, char *argv[]){

	int socketEcoute;
	//Interpretation de la commande du lancement serveur
	int opt;
    int port=0;
	char *ip = NULL;
	// Vérifie que la commande a la forme attendue
    if (argc != 5 && argc != 3) {
        fprintf(stderr, "Usage: %s <-s IP> [-p PORT]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    while ((opt = getopt(argc, argv, "s:p:")) != -1) {
        switch (opt) {
            case 's':
                ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s <-s IP> [-p PORT]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
	if (port==0)
	{
		port=5000;
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

/**
 * Fonction principale du client. Initialise et gère les communications avec le serveur.
 * @param socketEcoute La socket d'écoute utilisée pour la communication avec le serveur.
 * @return Cette fonction ne retourne rien.
 * @ingroup fonct_duClient
*/
void mainClient(int socketEcoute){
	initscr();
	int ecrits, lus;/* nb d’octets ecrits et lus */
	char messageEnvoi[LG_MESSAGE];/* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE];/* le message de la couche Application ! */

	// Initialise à 0 les messages
	memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
	memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));

	char* limit="/getSize";
	strcpy(messageEnvoi, limit);
	ecrits = write(socketEcoute, messageEnvoi, strlen(messageEnvoi));

	int l=0,c=0;
	lus = read(socketEcoute, messageRecu, LG_MESSAGE*sizeof(char));
	if (lus == -1) {
		perror("read");
		close(socketEcoute);
		exit(-4);
	} else if (lus == 0) {
		fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
		close(socketEcoute);
		exit(-1);
	} else {
		messageRecu[lus]='\0';
		char strL[LG_MESSAGE];
		char strC[LG_MESSAGE];
		selectMot(messageRecu, 1, strC);
		selectMot(messageRecu, 2, strL);
		l=atoi(strL);
		c=atoi(strC);
	}

	int matrice[l*c*3];
	for (int i = 0; i < l*c*3; i++) {
		matrice[i] = 0; // initialise chaque élément à 0
	}
		
	//afficherCouleurs(matrice,l,c);

	// Envoie un message au serveur
	char phrase[LG_MESSAGE*sizeof(char)];
	while (1)
	{
		//fgets(phrase, sizeof(phrase), stdin);
		strcpy(messageEnvoi, affichage());
		if (strcmp(messageEnvoi, "")==0)
		{
			break;
		}
		
		ecrits = write(socketEcoute, messageEnvoi, strlen(messageEnvoi));
		if (ecrits == -1) {
			perror("write");
			close(socketEcoute);
			exit(-3);
		} else if (ecrits == 0) {
			fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
			close(socketEcoute);
			exit(-1);
		}

		lus = read(socketEcoute, messageRecu, LG_MESSAGE-1);
		if (lus == -1) {
			perror("read");
			close(socketEcoute);
			exit(-4);
		} else if (lus == 0) {
			fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
			close(socketEcoute);
			exit(-1);
		} else {
			messageRecu[lus]='\0';
			interpretationMsg(messageRecu, messageEnvoi, l, c, matrice);
		}

	}
	close(socketEcoute);
	endwin();
}

/**
 * Crée une nouvelle socket d'écoute pour le client.
 * @return L'identifiant de la nouvelle socket d'écoute.
 * @note Cette fonction utilise le protocole TCP par défaut.
 * @ingroup fonct_duClient
*/
int createSocket() {
    int socketEcoute = socket(PF_INET, SOCK_STREAM, 0);// 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP
    if (socketEcoute == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket créée avec succès ! (%d)\n", socketEcoute);
    return socketEcoute;
}

/**
 * Établit une connexion vers un serveur distant en utilisant une socket TCP.
 * @param socketEcoute Le descripteur de la socket à utiliser pour la connexion.
 * @param port Le port à utiliser pour la connexion.
 * @param ip L'adresse IP du serveur distant.
 * @return Cette fonction ne retourne rien.
 * @ingroup fonct_duClient
*/
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

/**
 * @brief Interprète les messages reçus du serveur et affiche les messages appropriés ou la matrice de pixels
 * @param messageRecu le message reçu du serveur
 * @param messageEnvoi le message envoyé au serveur
 * @param l la hauteur de la matrice
 * @param c la largeur de la matrice
 * @ingroup fonct_duClient
*/
void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c, int *matrice){
	if(strcmp(messageRecu,"00 OK\0")==0){
        mvprintw(LINES - 1, 0, "Validé");
	} else if(strcmp(messageRecu,"10 Bad Command\0")==0){
		mvprintw(LINES - 1, 0, "Mauvaise commande");
	} else if(strcmp(messageRecu,"11 Out Of Bound\0")==0){
		mvprintw(LINES - 1, 0, "Pixel en dehors de la matrice");
	} else if(strcmp(messageRecu,"12 Bad Color\0")==0){
		mvprintw(LINES - 1, 0, "Mauvaise Couleur");
	} else if(strcmp(messageRecu,"20 Out Of Quota\0")==0){
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
			interpretationMatrice(messageRecu, l, c, matrice);
		}
		
	}
}

/**
 * Encodes un tableau de caractères en base64.
 * @param rgb Le tableau de caractères à encoder.
 * @return Un pointeur vers le tableau de caractères encodé en base64, ou NULL en cas d'erreur.
 * @ingroup fonct_duClient
 */
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

/**
 * @brief Décodage et affichage de la matrice d'image à partir d'une chaîne encodée en base64.
 *
 * @param messageRecu Chaîne encodée en base64 contenant la matrice d'image.
 * @param l Nombre de lignes de la matrice.
 * @param c Nombre de colonnes de la matrice.
 * @ingroup fonct_duClient
 */
void interpretationMatrice(const char messageRecu[LG_MESSAGE], const int l, const int c, int *matrice) {
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Précalculer les index de chaque caractère de la table base64_chars
    int base64_index[256] = {0};
    for (const char* p = base64_chars; *p != '\0'; ++p) {
        base64_index[(unsigned char)*p] = p - base64_chars;
    }

    const size_t input_length = strlen(messageRecu);
    const size_t output_length = input_length / 4 * 3;

    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;

    if (matrice == NULL) {
        fprintf(stderr, "Erreur d'allocation de la mémoire\n");
    }
    int* ptrRGB = matrice; // Utiliser un pointeur pour parcourir le tableau

    for (size_t i = 0, j = 0; i < input_length;) {
        sextet_a = (messageRecu[i] == '=') ? 0 : base64_index[(unsigned char)messageRecu[i]];
        sextet_b = (messageRecu[i+1] == '=') ? 0 : base64_index[(unsigned char)messageRecu[i+1]];
        sextet_c = (messageRecu[i+2] == '=') ? 0 : base64_index[(unsigned char)messageRecu[i+2]];
        sextet_d = (messageRecu[i+3] == '=') ? 0 : base64_index[(unsigned char)messageRecu[i+3]];

        triple = (sextet_a << 3 * 6)
               + (sextet_b << 2 * 6)
               + (sextet_c << 1 * 6)
               + (sextet_d << 0 * 6);

        *ptrRGB++ = (triple >> 2 * 8) & 0xFF;
        *ptrRGB++ = (triple >> 1 * 8) & 0xFF;
        *ptrRGB++ = (triple >> 0 * 8) & 0xFF;

        i += 4;
    }
	//AFFICHAGE
	afficherCouleurs(matrice,l,c);
	printw("BON1");

}

void afficherCouleurs(int* tableauRGB, const int l, const int c) {
	if (!has_colors()) {
        printw("Erreur : Le terminal ne supporte pas les couleurs");
        return;
    }

    // Initialiser les couleurs
    start_color();
    //use_default_colors();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	attron(COLOR_PAIR(1));


    move(1, 0);
    printw("'%d'/lignes: %d/colonne: %d", l * c, l, c);
    move(2, 0);
	attroff(COLOR_PAIR(1));
    for (int lignes = 0; lignes < l; lignes++) {
        for (int colonnes = 0; colonnes < c*3; ) {
            int indice = (lignes * c + colonnes);
            int color_r = tableauRGB[indice] * 1000 / 255;
            int color_g = tableauRGB[indice + 1] * 1000 / 255;
            int color_b = tableauRGB[indice + 2] * 1000 / 255;
			int customColor = (lignes*c)+colonnes+2;
            init_color(customColor, color_r, color_g, color_b);
            init_pair(customColor+2, COLOR_WHITE, customColor);
            attron(COLOR_PAIR(customColor+2));
			printw(" ");
            //printw("(%d,%d,%d)",tableauRGB[indice],tableauRGB[indice+1],tableauRGB[indice+2]);
            attroff(COLOR_PAIR(customColor+2));
			colonnes+=3;
        }
		attron(COLOR_PAIR(1));
        move(3 + lignes, 0);
		attroff(COLOR_PAIR(1));
    }
	attron(COLOR_PAIR(1));
	attroff(COLOR_PAIR(1));
	//use_default_colors();
    refresh();
    getch();
    clear();
}

/**
 * @brief Sélectionne un mot dans une phrase en fonction de sa position.
 *
 * @param phrase La phrase dans laquelle chercher le mot.
 * @param nombre La position du mot à récupérer.
 * @param mot    Un pointeur vers l'endroit où stocker le mot.
 * @ingroup fonct_duClient
 */
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

/**
 * Affiche un message de bienvenue sur la fenêtre du terminal et attend que l'utilisateur appuie sur la touche '1' ou '&' pour continuer.
 * 
 * @param socketEcoute Le socket sur lequel le serveur écoute les connexions entrantes.
 * @ingroup fonct_duClient
 */
void affichageEntree(int socketEcoute) {
    initscr();
    noecho();
    keypad(stdscr, TRUE);

    const char* msgA[] = {
        "########  #### ##     ## ######## ##       ##      ##    ###    ########  ",
        "##     ##  ##   ##   ##  ##       ##       ##  ##  ##   ## ##   ##     ## ",
        "########   ##     ###    ######   ##       ##  ##  ## ##     ## ########  ",
        "##         ##    ## ##   ##       ##       ##  ##  ## ######### ##   ##   ",
        "##         ##   ##   ##  ##       ##       ##  ##  ## ##     ## ##    ##  ",
        "##        #### ##     ## ######## ########  ###  ###  ##     ## ##     ## ",
        "appuyer sur 1 pour aller au menu"
    };

    int taille = strlen(msgA[1]);
	mvprintw((LINES/2)-2, (COLS / 2) - (taille / 2), msgA[0]);
	mvprintw((LINES/2)-1, (COLS / 2) - (taille / 2), msgA[1]);
	mvprintw((LINES/2), (COLS / 2) - (taille / 2), msgA[2]);
	mvprintw((LINES/2)+1, (COLS / 2) - (taille / 2), msgA[3]);
	mvprintw((LINES/2)+2, (COLS / 2) - (taille / 2), msgA[4]);
	mvprintw((LINES/2)+3, (COLS / 2) - (taille / 2), msgA[5]);
	mvprintw((LINES)-2, (COLS/2) - (strlen(msgA[6])/2), msgA[6]);
    refresh();

    int key;
    while ((key = getch()) != '1' && key != '&') {}
	clear();
    endwin();
    mainClient(socketEcoute);
}


/**
 * Cette fonction affiche un menu dans une fenêtre et attend que l'utilisateur sélectionne une option. 
 * 
 * @return un pointeur vers une chaîne de caractères contenant le message à envoyer au serveur en fonction de l'option sélectionnée par l'utilisateur.
 * @ingroup fonct_duClient
 */
char *affichage(){
    initscr();

    // Définition des options du menu
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

    // Calcul des coordonnées du centre de l'écran
    int centerY = LINES / 2;
    int centerX = COLS / 2;

    // Afficher les options du menu
    for(int i=0; i<7; i++){
        mvprintw(centerY-3+i, centerX-(strlen(msgM[i])/2), msgM[i]);
    }

    // Afficher le message pour sélectionner une option
    mvprintw(LINES-2, centerX-(strlen(msgM[7])/2), msgM[7]);

    refresh(); // Rafraîchir l'affichage

    // Boucle de saisie de l'option sélectionnée
    int key;
    while (1){
        key = getch();
        if(key == 'a' || key == 'z' || key == 'e' || key == 'r' || key == 't' || key == 'y' || key == 'q'){
            break;
        }
    }

    char *messageFinal = NULL;
    switch(key){
        case 'a':
            /* SetPixel */
            echo();
            clear();
            char position[50];
            getnstr(position, 50);
            char couleur[50];
            getnstr(couleur, 50);
            strcpy(couleur,base64_encode(couleur));
            messageFinal = malloc(100 * sizeof(char));
            sprintf(messageFinal, "/setPixel %s %s", position, couleur);
            clear();
            printw("le msg: %s", messageFinal);
            noecho();
            break;

        case 'z':
            /* getSize */
            clear();
            messageFinal = "/getSize";
            break;

        case 'e':
            /* getLimits */
            clear();
            messageFinal = "/getLimits";
            break;

        case 'r':
            /* getVersion */
            clear();
            messageFinal = "/getVersion";
            break;

        case 't':
            /* getWaitTime */
            clear();
            messageFinal = "/getWaitTime";
            break;

        case 'y':
            /* getMatrix */
            clear();
            messageFinal = "/getMatrix";
            break;

        case 'q':
            clear();
			messageFinal = "";
            break;
    }

    return messageFinal;
}
