#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */

#define LG_MESSAGE 256
#define PORT IPPORT_USERRESERVED // = 5000

int main(int argc, char *argv[]){

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



	int descripteurSocket;
	struct sockaddr_in serv_addr;
	char messageEnvoi[LG_MESSAGE];/* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE];/* le message de la couche Application ! */
	int ecrits, lus;/* nb d’octets ecrits et lus */
	int retour;

	// Crée un socket de communication
	descripteurSocket = socket(PF_INET, SOCK_STREAM, 0);
	/* 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP */

	// Teste la valeur renvoyée par l’appel système socket()
	if(descripteurSocket < 0)/* échec ? */
	{
		perror("socket");// Affiche le message d’erreur
		exit(-1);// On sort en indiquant un code erreur
	}
	printf("Socket créée avec succès ! (%d)\n", descripteurSocket);


	// les info serveur
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = PF_INET;
	serv_addr.sin_port = htons(port); //port serv
	serv_addr.sin_addr.s_addr = inet_addr(ip); //ip serv
	
	// connexion vers le serveur
	if((connect(descripteurSocket, (struct sockaddr *)&serv_addr,sizeof(serv_addr))) == -1){
		perror("Connexion vers le serveur à échouée");// Affiche le message d’erreur
		close(descripteurSocket);// On ferme la ressource avant de quitter
		exit(-2);// On sort en indiquant un code erreur
	}

	printf("Connexion au serveur réussie avec succès !\n");
	// Initialise à 0 les messages
	memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
	memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));

	// Envoie un message au serveur
	char phrase[LG_MESSAGE*sizeof(char)];
	fgets(phrase, sizeof(phrase), stdin);
	strcpy(messageEnvoi, phrase);
	ecrits = write(descripteurSocket, messageEnvoi, strlen(messageEnvoi));// message àTAILLE variable
	switch(ecrits)
	{
		case-1 :/* une erreur ! */
			perror("write");
			close(descripteurSocket);
			exit(-3);
		case 0 :/* la socket est fermée */
			fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
			close(descripteurSocket);
			return 0;
		default:/* envoi de n octets */
			printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
	}

		/* Reception des données du serveur */
	lus = read(descripteurSocket, messageRecu, LG_MESSAGE*sizeof(char));/* attend un messagede TAILLE fixe */
	switch(lus)
	{
		case-1 :/* une erreur ! */
			perror("read");
			close(descripteurSocket);
			exit(-4);
		case 0 :/* la socket est fermée */
			fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
			close(descripteurSocket);
			return 0;
		default:/* réception de n octets */
			printf("Message reçu du serveur : %s (%d octets)\n\n", messageRecu, lus);
	}

	// On ferme la ressource avant de quitter
	close(descripteurSocket);
	return 0;
}
