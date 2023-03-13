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

void mainClient(int socketEcoute);
int createSocket();
void bindSocket(int socketEcoute, int port, char* ip);
void interpretationMsg(char messageRecu[LG_MESSAGE]);
int espace(char messageRecu[LG_MESSAGE]);
void affichageMatrice(char messageRecu[LG_MESSAGE]);

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
				interpretationMsg(messageRecu);
				//printf("Message reçu du serveur : %s (%d octets)\n\n", messageRecu, lus);
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

void interpretationMsg(char messageRecu[LG_MESSAGE]){
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

		if(espace(messageRecu)==0){
			printf("%s\n", messageRecu);
		}else{
			affichageMatrice(messageRecu);
		}
		
	}
}

int espace(char messageRecu[LG_MESSAGE]){
	int i=0;
	while (messageRecu[i]!='\0')
	{
		if (messageRecu[i]==' ')
		{
			return 0;
		}
		i++;
	}
	return 1;
}

void affichageMatrice(char messageRecu[LG_MESSAGE]){
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
    printf("%s\n\n",messageEnvoi);
	free(messageEnvoi);
}