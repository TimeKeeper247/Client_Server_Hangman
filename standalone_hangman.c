#include <stdio.h>
#include <stdlib.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void ClearScreen(){
    //system("cls||clear"); // Windows
    //printf("\033[2J"); // X-Term
    printf("\e[1;1H\e[2J"); // other ANSI terms or POSIX
}

typedef struct node{
    char usrn[10];
    char pass[10];
    int played;
    int won;
    struct node* next;
} node;

typedef void (*callback)(node* data);

node* create(char usrn[], char pass[], node* next)
{
    node* new_node = (node*)malloc(sizeof(node));
    if (new_node == NULL){
        printf("Error\n");
        return(0);
    }
    //new_node->usrn = usrn;
    //new_node->pass = pass;
    strcpy(new_node->usrn, usrn);
    strcpy(new_node->pass, pass);
    new_node->played = 0;
    new_node->won = 0;
    new_node->next = next;

    return new_node;
}

node* prepend(node* head, char usrn[], char pass[]){
    node* new_node = create(usrn, pass, head);
    head = new_node;
    return head;
}

node* append(node* head, char usrn[], char pass[])
{
    if(head == NULL)
        return NULL;
    /* go to the last node */
    node *cursor = head;
    while(cursor->next != NULL)
        cursor = cursor->next;

    /* create a new node */
    node* new_node =  create(usrn, pass, NULL);
    cursor->next = new_node;

    return head;
}

node* search(node* head, char usrn[]){
    node *cursor = head;
    while(cursor != NULL){
        if (strcmp(cursor->usrn, usrn)==0){
            return cursor;
        } else {
            cursor = cursor->next;
        }
    }
    return NULL;
}

int count(node *head)
{
    node *cursor = head;
    int c = 0;
    while(cursor != NULL)
    {
        c++;
        cursor = cursor->next;
    }
    return c;
}

void traverse(node* head, callback f){
    node* cursor = head;
    while(cursor != NULL){
        f(cursor);
        cursor = cursor->next;
    }
}

void display(node* n){
    if (n != NULL && n->played > 0){
        printf("\n=============================================\n\n");
        printf("Player - %s\n", n->usrn);
        //printf("%s ", n->pass);
        printf("Number of games won - %d\n", n->won);
        printf("Number of games played - %d\n", n->played);
        printf("\n=============================================\n\n");
    }
}

void PlayHangman(char *object[], int obj_size, char *obj_type[], int type_size, node *user)
{
    int size1 = obj_size;
    int size2 = type_size;
    char name[10], word1[size1], word2[size2];
    if (user == NULL){
        printf("ERROR");
    }
    node* acc = user;
    strcpy(name, acc->usrn);
    printf("user: %s", name);
    strcpy(word1, object);
    strcpy(word2, obj_type);
    printf("words: %s %s", word1, word2);

    int obj[size1];// boolean array testing whether a letter was guessed
    int type[size2];
    memset(obj, 0, size1*sizeof(int));
    memset(type, 0, size2*sizeof(int));
    //int type[word2_len] = {0};

    char letters[20] = { NULL };
    //memset(letters, 0, 20*sizeof(char));

    char c[2];
    int endgame = 0;
    //int num_guesses = 20;
    ClearScreen();
    int num_guesses = MIN((size1 + size2 + 10),26);
    while (!endgame){
        ClearScreen();
        printf("\nGuessed Letters %s\n", letters);
        printf("Number of guesses left: %d\n", num_guesses);
        printf("Word: ");
        for (int i=0;i<size1;i++){
            if (!obj[i]){
                printf("_ ");
            } else {
                printf("%c ", word1[i]);
            }
        }
        printf("  ");
        for (int i=0;i<size2;i++){
            if (!type[i]){
                printf("_ ");
            } else {
                printf("%c ", word2[i]);
            }
        }
        printf("\n");
        printf("Enter your guess - ");
        //c[0] = getchar();
        endgame = 1;
        for (int i=0;i<size1;i++){
            if (!obj[i]){ // checking all true for boolean arrays
                endgame = 0;
            }
        }
        for (int i=0;i<size2;i++){
            if (!type[i]){
                endgame = 0;
            }
        }
        if (!num_guesses){
            endgame = 1;
        }
        if (num_guesses > 0 && !endgame){
            scanf(" %c", &c[0]);
            //c[0] = getchar();
        }
        //ClearScreen();
        num_guesses -= 1;
        strcat(letters, c);
        for (int i=0;i<size1;i++){
            if (c[0] == word1[i]){
                // setting boolean array depending if letter was guessed
                obj[i] = 1;
            }
        }
        for (int i=0;i<size2;i++){
            if (c[0] == word2[i]){
                // setting boolean array depending if letter was guessed
                type[i] = 1;
            }
        }

    }
    printf("\nGame over\n");
    if(num_guesses > 0){
        printf("Well done %s You won this round of Hangman!\n", name);
    } else {
        printf("Bad luck %s You have run out of guesses. The Hangman got you!\n", name);
    }
    //system("pause");
    printf("Press ENTER to continue.");
    char r[100];
    r[0] = getchar();
    r[0] = getchar();
    //getchar();
}

int Compare(const node *a, const node *b){ // compare function used to sort vector of accounts
    if (a->played == 0){ // prevents divide by zero
        return 0;
    } else if (b->played == 0){
        return 1;
    }
    if (a->won == b->won){
        if (((double)a->won)/a->played == ((double)b->won)/b->played){ // determines percentage of games won
            return a->usrn < b->usrn;
        } else {
            return ((double)a->won)/a->played < ((double)b->won)/b->played;
        }
    } else {
        return a->won < b->won;
    }
}

node* Sort(node* head)
{
    struct node *temp=head;
    struct node *temp1=temp->next;
    char usrn[10], pass[10];
    int played, won;

    while(temp!=NULL)
    {
        temp1=temp->next;
        while(temp1!=NULL)
        {
            if(Compare(temp, temp1))
            {
                strcpy(usrn, temp->usrn);
                strcpy(pass, temp->pass);
                played = temp->played;
                won = temp->won;

                strcpy(temp->usrn, temp1->usrn);
                strcpy(temp->pass, temp1->pass);
                temp->played = temp1->played;
                temp->won = temp1->won;

                strcpy(temp1->usrn, usrn);
                strcpy(temp1->pass, pass);
                temp1->played = played;
                temp1->won = won;
            }
            temp1=temp1->next;
        }
        temp=temp->next;
    }
    return head;
}

int main()
{
    node* head = NULL;
    callback disp = display;

    head = prepend(head, "Player1", "54321");
    head = append(head, "Player2", "12345");
    head = append(head, "Player3", "54321");
    head = append(head, "Player4", "12345");
    head = append(head, "Player5", "12345");
    search(head, "Player1")->played = 10;
    search(head, "Player1")->won = 8;
    search(head, "Player2")->played = 10;
    search(head, "Player2")->won = 8;
    search(head, "Player3")->played = 100;
    search(head, "Player3")->won = 8;
    search(head, "Player4")->played = 20;
    search(head, "Player4")->won = 17;

    Sort(head);

    traverse(head, disp);
    printf("\n");

    const char *words[] = {"snek", "snake"};
    const int sizes[] = {strlen(words[0]), strlen(words[1])};

    char usrn[10], pass[10];
    printf("=============================================\n");
    printf("Welcome to the Online Hangman Gaming System\n");
    printf("=============================================\n\n");
    printf("You are required to logon with your registered Username and Password\n");
    printf("Please enter your username-->");
    scanf("%s", usrn);
    printf("Please enter your password-->");
    scanf("%s", pass);

    ClearScreen();

    while (1){
        ClearScreen();
        char key = 0;
        printf("\nWelcome to the Hangman Gaming System\n\n");
        printf("Please enter a selection\n");
        printf("<1> Play Hangman\n");
        printf("<2> Show Leaderboard\n");
        printf("<3> Quit\n");
        printf("Select option 1-3 ->");
        //scanf(" %c", &key);
        key = getchar();
        switch (key)
        {
        case '1':
            printf("option was 1");// testing to see if this option was chosen
            node* player = search(head, "Player1");
            PlayHangman(words[0], sizes[0], words[1], sizes[1], player);
            break;
        case '2':
            printf("option was 2");
            break;
        case '3':
            printf("option was 3");
            exit(EXIT_SUCCESS);
            break;
        default:
            printf("Invalid option");
        }
    }

    return 0;
}
