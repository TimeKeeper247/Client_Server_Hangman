#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

//magic numbers and constants
#define MAXDATASIZE 100
#define ARRAY_SIZE 10
#define RETURNED_ERROR -1
#define h_addr h_addr_list[0] /* for backward compatibility */

//send a string to hangman_server, upto 10 characters length
void SendString(int socket_id, char * string, int stringLength) {
    //pad out string with zeros as required
    if (stringLength < ARRAY_SIZE) {
        for (int i = stringLength; i < ARRAY_SIZE; i++) {
            string[i] = 0;
        }
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
        unsigned short networkValue = htons(string[i]);
        send(socket_id, & networkValue, sizeof(unsigned short), 0);
    }
    fflush(stdout);
}

//send integer
void SendInt(int sockfd, int integer) {
    unsigned short netval = htons(integer);
    send(sockfd, & netval, sizeof(unsigned short), 0);
}

//receives a string and prints
void ReceiveData(int sockfd) {
    int numberOfBytes;
    char buf[MAXDATASIZE];
    if ((numberOfBytes = recv(sockfd, buf, MAXDATASIZE, 0)) == RETURNED_ERROR) {
        perror("recv");
        exit(1);
    }
    buf[numberOfBytes] = '\0';
    printf("%s", buf);
}

//receives integer, returns integer
int ReceiveInt(int sockfd) {
    int integer;
    unsigned short netval;
    if (recv(sockfd, & netval, sizeof(unsigned short), 0) == RETURNED_ERROR){
        return -1;
    }
    integer = ntohs(netval);
    return integer;
}

//clears screen depending on terminal
void ClearScreen() {
    //system("cls||clear"); // Windows
    //printf("\033[2J"); // X-Term
    printf("\e[1;1H\e[2J"); // other ANSI terms or POSIX
}

//draw the welcome message template
void DisplayWelcomeMessage() {
    printf("\n=============================================\n");
    printf("Welcome to the Online Hangman Gaming System\n");
    printf("=============================================\n\n");
    printf("You are required to logon with your registered Username and Password\n\n");
}

void DisplayMainMenu() {
    printf("\nWelcome to the Hangman Gaming System\n\n");
    printf("Please enter a selection\n");
    printf("<1> Play Hangman\n");
    printf("<2> Show Leaderboard\n");
    printf("<3> Quit\n");
    printf("Select option 1-3 ->");
}

void PlayHangman(char * username[], int sockfd) {
    char letters[20] = {
        NULL
    };

    char c[2];
    int endgame = 0;
    int num_guesses = 0;
    while (!endgame) {
        ClearScreen();
        num_guesses = ReceiveInt(sockfd); // receives guesses left from server
        SendInt(sockfd, 1); // sends confirmation
        printf("\nGuessed Letters %s\n", letters);
        printf("Number of guesses left: %d\n", num_guesses);
        printf("Word: ");
        ReceiveData(sockfd); // receives concatenated underscores and guesses leters
        SendInt(sockfd, 1);
        printf("\n");
        printf("Enter your guess - ");
        endgame = 1; // assumes game has ended
        endgame = ReceiveInt(sockfd); // changes otherwise
        if (!num_guesses) {
            endgame = 1;
        }
        if (num_guesses > 0 && !endgame) { // if games has not ended yet take letters
            scanf(" %c", & c[0]);
            SendString(sockfd, c, 2);
        }
        strcat(letters, c); // concatenate guessed letters
    }
    printf("\nGame over\n");
    if (num_guesses > 0) {
        printf("Well done %s You won this round of Hangman!\n", username);
    } else {
        printf("Bad luck %s You have run out of guesses. The Hangman got you!\n", username);
    }
    printf("Press ENTER to continue.");
    char r[100];
    r[0] = getchar(); // catches stray characters
    r[0] = getchar(); // waits for ENTER key
}

//shows leaderboard
void Leaderboard(int sockfd) {
    int won, played;
    while (!ReceiveInt(sockfd)) { // receives flag if leaderboard has been fully sent
        SendInt(sockfd, 0);
        printf("\n=============================================\n\n");
        printf("Player - ");
        ReceiveData(sockfd); // player name
        printf("\n");
        SendInt(sockfd, 0);
        won = ReceiveInt(sockfd);
        printf("Number of games won - %d\n", won);
        SendInt(sockfd, 0);
        played = ReceiveInt(sockfd);
        printf("Number of games played - %d\n", played);
        printf("\n=============================================\n\n");
        SendInt(sockfd, 0);
    }
    printf("Press ENTER to continue.");
    char r[100];
    r[0] = getchar(); // catches stray characters
    r[0] = getchar(); // waits for ENTER key
}

int main(int argc, char * argv[]) {
        int portNumber;
        int sockfd, numberOfBytes;
        char buf[MAXDATASIZE];
        struct hostent * he;
        struct sockaddr_in their_addr; /* connector's address information */

        //if there aren't three main arguments, throw an error message
        if (argc != 3) {
            fprintf(stderr, "Usage: client_hostname, port number\n");
            exit(1);
        }

        //if there are three or more main arguments, convert argv[2] to port number
        if (argc == 3) {
            portNumber = atoi(argv[2]);
        }

        //if a host name isn't specified
        if ((he = gethostbyname(argv[1])) == NULL) { /* get the host info */
            herror("gethostbyname");
            exit(1);
        }

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == RETURNED_ERROR) {
            perror("socket");
            exit(1);
        }

        their_addr.sin_family = AF_INET; /* host byte order */
        their_addr.sin_port = htons(portNumber); /* short, network byte order */
        their_addr.sin_addr = * ((struct in_addr * ) he-> h_addr);

        if (connect(sockfd, (struct sockaddr * ) & their_addr, \
                sizeof(struct sockaddr)) == RETURNED_ERROR) {
            perror("connect");
            exit(1);
        }
        // check if server is full or down
        if (ReceiveInt(sockfd)){
            fprintf(stderr, "Unable to connect. Server full or offline\n");
            exit(1);
        }
        DisplayWelcomeMessage();
        char c[2];

        //process for checking username and password authenticity
        int useracc = 0; // flag for username and password correctness
        int passacc = 0;
        char username[10] = {
            NULL
        };
        char password[10] = {
            NULL
        };
        while (!useracc) { // keep looping until valid user is entered
            printf("Please enter your username-->");
            memset(username, 0, sizeof username);
            scanf("%9s", username);
            int usernameLength = strlen(username);
            //send username
            SendString(sockfd, username, usernameLength);
            //ReceiveData(sockfd);
            useracc = ReceiveInt(sockfd);
            if (!useracc) {
                printf("User doesn't exist or is already online\n");
            }
        }

        //take password to be sent to server
        while (!passacc) { // keep looping until password is correct
            printf("Please enter your password-->");
            memset(password, 0, sizeof password);
            scanf("%9s", password);
            int passwordLength = strlen(password);
            //send password
            SendString(sockfd, password, passwordLength);
            passacc = ReceiveInt(sockfd);
            if (!passacc) {
                printf("Incorrect password\n");
            }
        }

        //should now be logged in, draw main menu
        ClearScreen();
        //receive welcome message
        while (1) {
            //draw the menu
            DisplayMainMenu();
            //wait for key input 1-3
            scanf(" %c", & c[0]);
            SendString(sockfd, c, 2);
            switch (c[0]) {
            case '1':
                PlayHangman(username, sockfd);
                break;
            case '2':
                Leaderboard(sockfd);
                break;
            case '3':
                exit(EXIT_SUCCESS);
                break;
            }
            ClearScreen();
        } //end while loop
        close(sockfd);

        return 0;
    } //end main
