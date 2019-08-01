#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

//magic numbers and constants
#define DEFAULT_PORT_NO 54321
#define ARRAY_SIZE 10
#define STRING_LENGTH 10
#define BACKLOG 10     /* how many pending connections queue will hold */
#define RETURNED_ERROR -1
#define ARGUMENTS_EXPECTED 2
#define usernameMaxLength 10
#define passwordMaxLength 10
#define wordMaxLength 15

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

//define a structure-linked list to hold word pairs
typedef struct list {
    char firstWord[wordMaxLength];
    char secondWord[wordMaxLength];
    unsigned int wordPairNumber;
    struct list * next;
}
wordPair;

//define a structure-linked list to hold users
typedef struct node {
    char username[usernameMaxLength];
    char password[passwordMaxLength];
    int gamesWon;
    int gamesPlayed;
    int online;
    struct node * next;
}
userNode;

//global structure definition
wordPair * words = NULL;
userNode * users = NULL;

//define a structure containing thread arguments
typedef struct threadArgs {
    userNode * users; //pointer to head of user linked lsit
    wordPair * words; //pointer to head of words linked list
    pthread_t tid; //threadID
    int new_fd; //int for socket number
    int idleThread;
    //int idx;
}
ThreadArgs;

//create array of ThreadArgs structures
ThreadArgs threadArgsArray[BACKLOG];

//create semaphore
sem_t readAccess;
sem_t dbAccess;
int readCount = 0;

//callback definitions for linked list
typedef void( * callbackUser)(userNode * data);
typedef void( * callbackWord)(wordPair * data);

//function declarations
char * ReceiveData(int socket_identifier, int size);
int TestCharacter(char receivedChar);

//catch and handle SIGINT ctrl+c
void SignalHandler(void) {
    //close open sockets
    for (int i = 0; i < BACKLOG; i++) {
        if (threadArgsArray[i].new_fd != 0) {
            close(threadArgsArray[i].new_fd);
        }
    }
    exit(0);
}

//resets argument structure values after clients' quit
void CleanArgs(ThreadArgs * args) {
    args-> idleThread = 1;
    args-> new_fd = 0;
}

//initialise array of ThreadArgs structures (at server start)
void GenerateThreadArgs(userNode * users, wordPair * words) {
    for (int i = 0; i < BACKLOG; i++) {
        threadArgsArray[i].users = users;
        threadArgsArray[i].words = words;
        threadArgsArray[i].new_fd = 0;
        threadArgsArray[i].idleThread = 1;
    }
}

// creates a userNode node
userNode * AddUserNode(userNode * next, char username[], char password[]) {
    // create new userNode to add to list
    userNode * newNode = (userNode * ) malloc(sizeof(userNode));
    if (newNode == NULL) {
        return (0);
    }
    // insert new node
    strcpy(newNode-> username, username);
    strcpy(newNode-> password, password);
    newNode-> gamesWon = 0;
    newNode-> gamesPlayed = 0;
    newNode-> online = 0;
    newNode-> next = next;
    return newNode;
}

// puts user at the top of the list, must be used before appendUser
userNode * prependUser(userNode * head, char usrn[], char pass[]) {
    userNode * new_node = AddUserNode(head, usrn, pass);
    head = new_node;
    return head;
}

// puts user after the last user in the list
userNode * appendUser(userNode * head, char usrn[], char pass[]) {
    if (head == NULL)
        return NULL;
    /* go to the last node */
    userNode * cursor = head;
    while (cursor-> next != NULL)
        cursor = cursor-> next;
    /* create a new node */
    userNode * new_node = AddUserNode(NULL, usrn, pass);
    cursor-> next = new_node;
    return head;
}

// finds and returns user from list
userNode * searchUser(userNode * head, char usrn[]) {
    userNode * cursor = head;
    while (cursor != NULL) {
        if (strcmp(cursor-> username, usrn) == 0) {
            return cursor;
        } else {
            cursor = cursor-> next;
        }
    }
    return NULL;
}

//create a wordPair node
wordPair * AddWordPair(wordPair * next, char firstWord[], char secondWord[], \
    unsigned int wordPairCounter) {
    // create new wordPair to add to list
    wordPair * newPair = (wordPair * ) malloc(sizeof(wordPair));
    if (newPair == NULL) {
        return (0);
    }
    // insert new node
    strcpy(newPair-> firstWord, firstWord);
    strcpy(newPair-> secondWord, secondWord);
    newPair-> wordPairNumber = wordPairCounter;
    newPair-> next = next;
    return newPair;
}

//inserts a wordPair at the start of the list, must be used before appendWord
wordPair * prependWord(wordPair * head, char word1[], char word2[]) {
    wordPair * new_node = AddWordPair(head, word1, word2, 0);
    head = new_node;
    return head;
}

//inserts a wordPair after the last wordPair in the list
wordPair * appendWord(wordPair * head, char word1[], char word2[], unsigned int counter) {
    if (head == NULL)
        return NULL;
    /* go to the last node */
    wordPair * cursor = head;
    while (cursor-> next != NULL)
        cursor = cursor-> next;
    /* create a new node */
    wordPair * new_node = AddWordPair(NULL, word1, word2, counter);
    cursor-> next = new_node;

    return head;
}

/*This function searches the wordPair list for a specified (by integer) pair,
then return a pointer to that wordPair.*/
wordPair * searchWord(wordPair * head, int number) {
    wordPair * cursor = head;
    while (cursor != NULL) {
        if (cursor-> wordPairNumber == number) {
            return cursor;
        } else {
            cursor = cursor-> next;
        }
    }
    return NULL;
}

/*This function counts the number of word pairs in the wordPair list and
returns the total as an integer.*/
int wordCount(wordPair * head) {
    wordPair * cursor = head;
    int c = 0;
    while (cursor != NULL) {
        c++;
        cursor = cursor-> next;
    }
    return c;
}

// compare function used to sort linked list of userNodes
int Compare(const userNode * a,
    const userNode * b) { // compare function used to sort vector of accounts
    if (a-> gamesPlayed == 0) { // prevents divide by zero
        return 0;
    } else if (b-> gamesPlayed == 0) {
        return 1;
    }
    if (a-> gamesWon == b-> gamesWon) {
        if (((double) a-> gamesWon) / a-> gamesPlayed == ((double) b-> gamesWon) / b-> gamesPlayed) { // determines percentage of games won
            if (strcmp(a->username, b->username) > 0){ // determine lexicographic order
                return 1;
            } else {return 0;}
        } else {
            return ((double) a-> gamesWon) / a-> gamesPlayed < ((double) b-> gamesWon) / b-> gamesPlayed;
        }
    } else {
        return a-> gamesWon < b-> gamesWon;
    }
}

/*this function is called to perform sorting functions on the userNode linked list, it returns
the new head of the sorted linked userNode list*/
userNode * Sort(userNode * head) {
    userNode * temp = head;
    userNode * temp1 = temp-> next;
    char usrn[10], pass[10];
    int played, won, online;

    while (temp != NULL) {
        temp1 = temp-> next;
        while (temp1 != NULL) {
            if (Compare(temp, temp1)) { // shuffling linked list depending on compare value
                strcpy(usrn, temp-> username);
                strcpy(pass, temp-> password);
                played = temp-> gamesPlayed;
                won = temp-> gamesWon;
                online = temp-> online;

                strcpy(temp-> username, temp1-> username);
                strcpy(temp-> password, temp1-> password);
                temp-> gamesPlayed = temp1-> gamesPlayed;
                temp-> gamesWon = temp1-> gamesWon;
                temp-> online = temp1-> online;

                strcpy(temp1-> username, usrn);
                strcpy(temp1-> password, pass);
                temp1-> gamesPlayed = played;
                temp1-> gamesWon = won;
                temp1-> online = online;
            }
            temp1 = temp1-> next;
        }
        temp = temp-> next;
    }
    return head;
}

/*This function imports the list of user authentication details from a text file
stored in the same directory as the executable, and stores those deatils in a
linked list of userNode structures. When called, this function returns a pointer
to the head of the linked list.*/
userNode * AuthenticationImport() {
    userNode * head = NULL;
    FILE * authenticationText;
    //authenticationArray holds fget string before parsing
    char authenticationArray[passwordMaxLength + usernameMaxLength];
    //parsed username from authenticationArray
    char usernameArray[usernameMaxLength];
    //parsed password from authenticationArray
    char passwordArray[passwordMaxLength];
    //initialise arrays with zeros
    for (int i = 0; i < usernameMaxLength; i++) {
        usernameArray[i] = 0;
        passwordArray[i] = 0;
        authenticationArray[i] = 0;
        authenticationArray[i + 10] = 0;
    }
    //open Authentication.txt in read mode
    authenticationText = fopen("Authentication.txt", "r");
    if (authenticationText == NULL) {
        perror("Error opening Authentication file:");
        return (-1);
    }
    //copy values in Authentication.txt to usernameArray and passwordArray, remove spaces somehow
    fgets(authenticationArray, (usernameMaxLength + passwordMaxLength), authenticationText);
    //head = prependUser(head, usernameArray, passwordArray);
    int cnt = 0;
    while (fgets(authenticationArray, (usernameMaxLength + passwordMaxLength), \
            authenticationText)) {
        //populate usernameArray using first 10 values (or until a space or indent) of authenticationArray
        for (int i = 0; i < usernameMaxLength; i++) {
            //read the username one char at a time until hitting space or indent
            if ((authenticationArray[i] != ' ') && (authenticationArray[i] != '\t')) {
                usernameArray[i] = authenticationArray[i];
            }
            //terminate string
            else {
                usernameArray[i] = '\0';
                break;
            }
            //turn read chars on line into spaces
            authenticationArray[i] = ' ';
        }
        //populate passwordArray using final values (not spaces or indents) of authenticationArray
        int passwordCounter = 0;
        for (int i = 0; i < (usernameMaxLength + passwordMaxLength); i++) {
            if ((authenticationArray[i] != ' ') && (authenticationArray[i] != '\t')\
            && (authenticationArray[i] != '\r') && (authenticationArray[i] != '\n')) {
                passwordArray[passwordCounter] = authenticationArray[i];
                authenticationArray[i] = ' ';
                passwordCounter++;
            }
        }
        // Prepend the first
        if (!cnt) {
            head = prependUser(head, usernameArray, passwordArray);
            cnt++;
        } // append the rest
        else {
            head = appendUser(head, usernameArray, passwordArray);
        }
    }
    fclose(authenticationText);
    return head;
}

/*This function imports the list of word pairs from a text file stored in the
same directory as the executable, and stores those words in a linked list of
wordPair structures. When called, this function returns a pointer to the head
of the linked list.*/
wordPair * WordListImport() {
    wordPair * head = NULL;
    FILE * hangmanText;
    //awordArray holds fget string before parsing
    char wordArray[2 * wordMaxLength];
    //parsed first word from wordArray
    char firstWordArray[wordMaxLength];
    ////parsed password from authenticationArray
    char secondWordArray[wordMaxLength];
    //initialise arrays with zeros
    for (int i = 0; i < wordMaxLength; i++) {
        firstWordArray[i] = 0;
        secondWordArray[i] = 0;
        wordArray[i] = 0;
        wordArray[i + wordMaxLength] = 0;
    }
    //open Authentication.txt in read mode
    hangmanText = fopen("hangman_text.txt", "r");
    if (hangmanText == NULL) {
        perror("Error opening hangman_text file");
        return (-1);
    }
    //copy values in Authentication.txt to usernameArray and passwordArray, remove spaces somehow
    unsigned int wordPairCounter = 0;
    int cnt = 0;
    while (fgets(wordArray, sizeof(wordArray), hangmanText)) {
        //populate firstWordArray using char values (until a comma) of hangmanText
        for (int i = 0; i < wordMaxLength; i++) {
            //read the username one char at a time until hitting comma
            if (wordArray[i] != ',') {
                firstWordArray[i] = wordArray[i];
            }
            //when reaching the comma, change it to a space, terminate string, and break
            else {
                firstWordArray[i] = '\0';
                wordArray[i] = ' ';
                break;
            }
            //change each read char in wordArray to a space
            wordArray[i] = ' ';
        }
        //populate secondWordArray using final values (not spaces) of wordArray
        int secondWordCounter = 0;
        for (int i = 0; i < 2 * wordMaxLength; i++) {
            if (wordArray[i] != '\r' && wordArray[i] != '\n' && wordArray[i] != ' ') {
                secondWordArray[secondWordCounter] = wordArray[i];
                wordArray[i] = ' ';
                secondWordCounter++;
            }
        }
        wordPairCounter++;
        // prepend the first
        if (!cnt) {
            head = prependWord(head, firstWordArray, secondWordArray);
            cnt++;
        } // append the rest
        else {
            head = appendWord(head, firstWordArray, secondWordArray, wordPairCounter);
        }
    }
    fclose(hangmanText);
    return head;
}

//goes through users list executing a function of choice f
void traverseUsers(userNode * head, callbackUser f) {
    userNode * cursor = head;
    while (cursor != NULL) {
        f(cursor);
        cursor = cursor-> next;
    }
}

//goes through word list executing function of choice f
void traverseWords(wordPair * head, callbackWord f) {
    wordPair * cursor = head;
    while (cursor != NULL) {
        f(cursor);
        cursor = cursor-> next;
    }
}

//displays users from list
void displayUsers(userNode * n) {
    if (n != NULL) {
        printf("%s ", n-> username);
        printf("%s\n", n-> password);
    }
}

//sends userdata for leaderboard displaying only users with at least 1 game played
void sendUser(userNode * n, int new_fd) {
    if (n != NULL && n-> gamesPlayed > 0) { // checks if user has played at least 1 game
        char name[usernameMaxLength];
        strcpy(name, n-> username);
        int played = n-> gamesPlayed;
        int won = n-> gamesWon;
        SendInt(new_fd, 0);
        ReceiveInt(new_fd);
        send(new_fd, name, 10, 0); // sends user name
        ReceiveInt(new_fd);
        SendInt(new_fd, won); // sends games won
        ReceiveInt(new_fd);
        SendInt(new_fd, played); // sends games played
        ReceiveInt(new_fd);
    }
}


//sends leaderboard data for each user
void sendLeaderboard(userNode * head, int new_fd) {
    userNode * users = head;

    /*recursively copies entire users linked list before sorting
    to prevent breaking read process elsewhere*/
    userNode *newHead = malloc(sizeof(userNode));
    strcpy(newHead->username, users->username);
    strcpy(newHead->password, users->password);
    newHead-> gamesPlayed = users-> gamesPlayed;
    newHead-> gamesWon = users-> gamesWon;
    newHead-> online = users-> online;
    userNode *p = newHead;
    users = users->next;
    while(users != NULL) {
        p->next = malloc(sizeof(userNode));
        p=p->next;
        strcpy(p->username, users->username);
        strcpy(p->password, users->password);
        p-> gamesPlayed = users-> gamesPlayed;
        p-> gamesWon = users-> gamesWon;
        p-> online = users-> online;
        users = users->next;
    }
    p->next = NULL;
    //now linked list copy is sorted and sent instead

    Sort(newHead);
    while (newHead != NULL) {
        sendUser(newHead, new_fd);
        newHead = newHead-> next;
    }
    SendInt(new_fd, 1);
}

// displays word pairs in list
void displayWords(wordPair * n) {
    if (n != NULL) {
        printf("%s ", n-> firstWord);
        printf("%s\n", n-> secondWord);
    }
}

//sends a string
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

//receives string data
char * ReceiveData(int socket_identifier, int size) {
    int numberOfBytes = 0;
    unsigned short networkValue;
    char * receivedData = malloc(sizeof(char) * size);
    for (int i = 0; i < size; i++) {
        if ((numberOfBytes = recv(socket_identifier, & networkValue, sizeof(unsigned short), 0)) <= 0) {
            return -1; // return signal that client has disconnected
        }
        receivedData[i] = ntohs(networkValue);
    }
    return receivedData;
}

//sends integer
void SendInt(int new_fd, int integer) {
    unsigned short netval = htons(integer);
    send(new_fd, & netval, sizeof(unsigned short), 0);
}

//receives and returns integer
int ReceiveInt(int socket_id) {
    int integer;
    unsigned short netval;
    if (recv(socket_id, & netval, sizeof(unsigned short), 0) <= 0) {
        return -1; // return signal that client has disconnected
    }
    integer = ntohs(netval);
    return integer;
}

//test to see if a received character is a leter from 'a' to 'z'
int TestCharacter(char receivedChar) {
    if (receivedChar >= 'a' && receivedChar <= 'z') {
        return 1;
    } else {
        return 0;
    }
}

/*This is the container function for Hangman Game, Leaderboard, and quit functions.
Once a client connection is accepted, they are passed to this function. Arguments
are in the form of a pointer to a structure of type ThreadArgs.*/
void * GameLoop(ThreadArgs * argStruct) {
        int new_fd = argStruct-> new_fd;
        SendInt(new_fd, 0);

        //set thread as being in-use
        argStruct-> idleThread = 0;

        //wait for string to be received
        int useracc = 0; // flag for username and password correctness
        int passacc = 0;
        userNode * user = NULL;
        while (!useracc) { // keep looping until valid user is entered
            char * receivedUsername = ReceiveData(new_fd, STRING_LENGTH);
            if (receivedUsername == RETURNED_ERROR) {
                CleanArgs(argStruct);
                pthread_exit(NULL);
            }
            //test the received username, for now use simple string comparison
            user = searchUser(users, receivedUsername);
            //if the received and stored usernames match
            if (user != NULL && !user-> online) {
                //send a message prompting for a password
                printf("Password requested.\n");
                useracc = 1;
                SendInt(new_fd, useracc);
                //if the received and stored usernames DON'T match
            } else {
                SendInt(new_fd, useracc);
            }
        }
        while (!passacc) { // keep looping until password is correct
            //receive a password
            char * receivedPassword = ReceiveData(new_fd, STRING_LENGTH);
            if (receivedPassword == RETURNED_ERROR) {
                CleanArgs(argStruct);
                pthread_exit(NULL);
            }
            //if the received and stored passwords match
            if (strcmp(receivedPassword, user-> password) == 0) {
                printf("Login accepted.\n");
                passacc = 1;
                user-> online = 1;
                SendInt(new_fd, passacc);
            } else {
                SendInt(new_fd, 0);
            }
        }

        //receive integer, 1-3 choosing action
        int end = 0;
        int wins;
        char choice;
        char * raw;
        while (!end) {
            raw = ReceiveData(new_fd, STRING_LENGTH);
            if (raw == RETURNED_ERROR) {
                CleanArgs(argStruct);
                user-> online = 0;
                pthread_exit(NULL);
            }
            choice = raw[0];
            switch (choice) {
            case '1':
            {
                time_t t;
                srand((unsigned) time( & t));
                wordPair * word = searchWord(words, rand() % wordCount(words));
                const int sizes[] = {
                    strlen(word-> firstWord),
                    strlen(word-> secondWord)
                };
                wins = PlayHangman(word-> firstWord, sizes[0], word-> secondWord, \
                    sizes[1], user, new_fd);

                //writing into leaderboard
                sem_wait( & dbAccess); // signal other threads to wait
                user-> gamesPlayed++;
                user-> gamesWon += wins;
                sem_post( & dbAccess); // signal other threads database is free
                //end writing

                break;
            }
            case '2':
                // reading into leaderboard semaphore
                sem_wait( & readAccess); // signal this thread is reading
                readCount++; // increment number of readers
                if (readCount == 1) {
                    sem_wait( & dbAccess);
                    //reading code
                    sendLeaderboard(users, new_fd);
                    //end reading code
                }
                sem_post( & readAccess);
                sem_wait( & readAccess);
                readCount = readCount - 1; // done reading, decrement number of readers
                if (readCount == 0) {
                    sem_post( & dbAccess);
                }
                sem_post( & readAccess); // signal this thread is done reading
                //end reading

                break;
            case '3':
                end = 1;
                //exit gracefully
                CleanArgs(argStruct);
                user-> online = 0;
                pthread_exit(NULL);
                break;
            }
        } //end game while(!end) loop
    } //end GameLoop

/*This function contains the game logic for client-server game interaction.*/
int PlayHangman(char * object[], int obj_size, char * obj_type[], int type_size, \
    userNode * user, int new_fd) {

    int size1 = obj_size;
    int size2 = type_size;
    char name[10], word1[size1], word2[size2];
    if (user == NULL) {
        printf("ERROR");
    }

    userNode * acc = user;
    strcpy(name, acc-> username);

    strcpy(word1, object);
    strcpy(word2, obj_type);
    int obj[size1]; // boolean arrays testing whether a letter was guessed
    int type[size2];
    memset(obj, 0, size1 * sizeof(int)); // initialising bool array to all false
    memset(type, 0, size2 * sizeof(int));

    char progress[50] = { // concatenated string of underscores and guessed letters
        NULL
    };

    //for terminating chars with null terminators
    char str_spc[2], str_letter[2];
    str_spc[1] = '\0'; // space after letter
    str_letter[1] = '\0'; // letter itself
    char spc = ' ';
    str_spc[0] = spc;

    char c; // singled received letter
    char * raw; // raw received letters
    char undr[] = "_ "; // underscore
    char space[] = "  "; // space between underscored words
    int endgame = 0;
    int num_guesses = MIN((size1 + size2 + 10), 26);
    while (!endgame) {
        SendInt(new_fd, num_guesses); // sends number of guesses
        memset(progress, 0, sizeof progress); // clears progress initially

        // concatenates underscores and letters depending on which was guessed correctly
        for (int i = 0; i < size1; i++) {
            if (!obj[i]) {
                strcat(progress, undr);
            } else {
                str_letter[0] = word1[i];
                strcat(progress, str_letter);
                strcat(progress, str_spc);
            }
        }
        strcat(progress, space);
        for (int i = 0; i < size2; i++) {
            if (!type[i]) {
                strcat(progress, undr);
            } else {
                str_letter[0] = word2[i];
                strcat(progress, str_letter);
                strcat(progress, str_spc);
            }
        } // end concatenation
        if (ReceiveInt(new_fd) == RETURNED_ERROR) {
            return 0;
        }
        send(new_fd, progress, 60, 0);
        endgame = 1; // assume game has ended
        for (int i = 0; i < size1; i++) { // checks boolean array and changes endgame status
            if (!obj[i]) {
                endgame = 0;
            }
        }
        for (int i = 0; i < size2; i++) {
            if (!type[i]) {
                endgame = 0;
            }
        }
        if (!num_guesses) {
            endgame = 1;
        }
        ReceiveInt(new_fd);
        SendInt(new_fd, endgame); // sends flag if game has ended or not
        if (num_guesses > 0 && !endgame) { // if game has not ended take letters
            raw = ReceiveData(new_fd, STRING_LENGTH);
            c = raw[0];
        }
        num_guesses -= 1;
        for (int i = 0; i < size1; i++) {
            if (c == word1[i]) {
                // setting boolean array depending if letter was guessed
                obj[i] = 1;
            }
        }
        for (int i = 0; i < size2; i++) {
            if (c == word2[i]) {
                // setting boolean array depending if letter was guessed
                type[i] = 1;
            }
        }
    }
    if (num_guesses > 0) {
        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char * argv[]) {

        //Initialise semaphore
        sem_init(&readAccess,0,1);
        sem_init(&dbAccess,0,1);

        int portNumber;
        int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
        struct sockaddr_in my_addr; /* my address information */
        struct sockaddr_in their_addr; /* connector's address information */
        socklen_t sin_size;

        int userCounter = 0;
        int threadUser = 0;

        //If no port number argument is recieved, show message, use default port
        if (argc != ARGUMENTS_EXPECTED) {
            fprintf(stderr, "Defaulting to port 54321.\n");
            portNumber = DEFAULT_PORT_NO;
        } else {
            portNumber = atoi(argv[1]);
        }

        /* generate the socket */
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == RETURNED_ERROR) {
            perror("socket");
            exit(1);
        }

        /* generate the end point */
        my_addr.sin_family = AF_INET; /* host byte order */
        my_addr.sin_port = htons(portNumber); /* short, network byte order */
        my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

        /* bind the socket to the end point */
        if (bind(sockfd, (struct sockaddr * ) & my_addr, sizeof(struct sockaddr))\
        == RETURNED_ERROR) {
            perror("bind");
            exit(1);
        }

        /* start listnening */
        if (listen(sockfd, BACKLOG) == RETURNED_ERROR) {
            perror("listen");
            exit(1);
        }

        //this message may not be necessary
        printf("server starts listnening on port %d...\n", portNumber);

        //import word list and authentication details
        words = WordListImport();
        users = AuthenticationImport();

        GenerateThreadArgs(users, words);

        /* repeat: accept, send, close the connection */
        /* for every accepted connection, use a separate thread to serve it */
        while (1) { /* main accept() loop */

            //SIGINT handler
            signal(SIGINT, SignalHandler);
            sin_size = sizeof(struct sockaddr_in);
            if ((new_fd = accept(sockfd, (struct sockaddr * ) & their_addr, & sin_size))\
            == RETURNED_ERROR) {
                perror("accept");
                continue;
            }

            printf("Got connection from %s\n", inet_ntoa(their_addr.sin_addr));
            printf("Assigned socket: %d\n", new_fd);

            //if an idle thread is available, create thread
            threadUser = 0;
            userCounter = 0;
            while (userCounter < BACKLOG && !threadUser) {
                if (threadArgsArray[userCounter].idleThread == 1) {
                    threadArgsArray[userCounter].new_fd = new_fd;
                    //pass thread ID and attr to GameLoop, along with STRUCTURE of arguments
                    pthread_create( & threadArgsArray[userCounter].tid, NULL, GameLoop, & threadArgsArray[userCounter]);

                    threadUser = 1;
                }
                userCounter++;
            }
            if (!threadUser) { // else reply stating server is full
                printf("Thread pool full\n");
                SendInt(new_fd, 1);
            }

        } //end accept while loop
        //destroy semaphores
        sem_destroy(&dbAccess);
        sem_destroy(&readAccess);

        pthread_exit(NULL);
        return 0;
    } //end main
