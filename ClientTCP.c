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

void mainClient(int socketEcoute);
int createSocket();
void bindSocket(int socketEcoute, int port, char* ip);
void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c);
//int espace(char messageRecu[LG_MESSAGE]);
void interpretationMatrice(char messageRecu[LG_MESSAGE], int l, int c);
void selectMot(char phrase[LG_MESSAGE*sizeof(char)], int nombre, char *mot);
//void afficheMatrice();

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
	mainClient(socketEcoute);
	
	return 0;
}

void mainClient(int socketEcoute){

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
		fgets(phrase, sizeof(phrase), stdin);
		strcpy(messageEnvoi, phrase);
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
		lus = read(socketEcoute, messageRecu, LG_MESSAGE*sizeof(char));/* attend un messagede TAILLE fixe */
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
				interpretationMsg(messageRecu, messageEnvoi, l, c);
				
		}
	}
	close(socketEcoute);
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

void interpretationMsg(char messageRecu[LG_MESSAGE],char messageEnvoi[LG_MESSAGE], int l, int c){
	if(strcmp(messageRecu,"00\0")==0){
        printf("OK\n");
	} else if(strcmp(messageRecu,"10\0")==0){
		printf("Bad command\n");
	} else if(strcmp(messageRecu,"11\0")==0){
		printf("Pixel out of band\n");
	} else if(strcmp(messageRecu,"12\0")==0){
		printf("Bad color\n");
	} else if(strcmp(messageRecu,"20\0")==0){
		printf("Out of quota\n");
	} else{
		//printf("le message envoyé:'%s'\n",messageEnvoi);

		if (strcmp(messageEnvoi,"/getSize\n")==0){
			printf("la taille de la matrice est de: %s\n", messageRecu);
		}else if (strcmp(messageEnvoi,"/getLimits\n")==0){
			printf("%s pixel par minutes\n", messageRecu);
		}else if (strcmp(messageEnvoi,"/getVersion\n")==0){
			printf("Version %s\n", messageRecu);
		}else if (strcmp(messageEnvoi,"/getWaitTime\n")==0){
			printf("%s secondes à attendre avant de pouvoir envoyer un nouveau pixel\n", messageRecu);
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

// void afficheMatrice(){
// 	// WINDOW *haut, *bas;
// 	WINDOW *boite;
// 	char *msg= "____\n|_\n|___";
//     int taille= strlen(msg);
    
//     initscr();
// 	while(1) {
// 		clear();
//         // printw("Le terminal actuel comporte %d lignes et %d colonnes\n", LINES, COLS);
//         // refresh();  // Rafraîchit la fenêtre par défaut (stdscr) afin d'afficher le message
// 		clear();    // Efface la fenêtre (donc l'ancien message)
//         mvprintw(LINES/2, (COLS / 2) - (taille / 2), msg);
//         refresh();
// 		// haut= subwin(stdscr, LINES / 2, COLS, 0, 0);        // Créé une fenêtre de 'LINES / 2' lignes et de COLS colonnes en 0, 0
// 		// bas= subwin(stdscr, LINES / 2, COLS, LINES / 2, 0); // Créé la même fenêtre que ci-dessus sauf que les coordonnées changent
		
// 		// box(haut, ACS_VLINE, ACS_HLINE);
// 		// box(bas, ACS_VLINE, ACS_HLINE);
		
// 		// mvwprintw(haut, 1, 1, "Ceci est la fenetre du haut");
// 		// mvwprintw(bas, 1, 1, "Ceci est la fenetre du bas");
		
// 		// wrefresh(haut);
//    		// wrefresh(bas);

//         if(getch() != 410)  // 410 est le code de la touche générée lorsqu'on redimensionne le terminal
//             break;
//     }
    
    
//     //getch();
//     endwin();
    
//     // free(haut);
//     // free(bas);
// 	free(boite);
// }