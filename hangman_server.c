//Used help from searches for TCP server creation, multiple connection management, and received message management
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

static const int MAX_CLIENTS = 3;
static const int MAX_INCORRECT = 6;

static int activeClients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //This is to manage the threads for multiple different clients

//This is a helper to just package and send a game managerial message to the client
void sendMessage(int fileDescriptor, const char * msg)
{
    //For managerial messages, we simply send the message length and message to the stream
    unsigned char len = strlen(msg);
    write(fileDescriptor, &len, 1);
    write(fileDescriptor, msg, len);
}

//This is a helper to just package and send the game state message to the client
void sendGame(int fileDescriptor, const char * pattern, const char * incorrect)
{
    //Game messages always have a 0 flag
    unsigned char flag = 0;
    //We set all the flag values and then write in the message to the stream so that the client can read it
    unsigned char wlen = strlen(pattern);
    unsigned char ninc = strlen(incorrect);
    write(fileDescriptor, &flag, 1);
    write(fileDescriptor, &wlen, 1);
    write(fileDescriptor, &ninc, 1);
    write(fileDescriptor, pattern, wlen);
    write(fileDescriptor, incorrect, ninc);
}

//This is a helper to randmly choose a word to set as the game's word
char * chooseWord()
{
    //We need to open the words file and read through it
    FILE * f = fopen("hangman_words.txt", "r");
    char * words[15];
    int count = 0;
    char buff[10]; //Words are at most 8 chars, so I chose 8 + null char + 1 for safety for the buffer
    //We can read in all the words by just reading in every line in the file
    while(fgets(buff, 10, f))
    {
        buff[strcspn(buff, "\n")] = 0; //We need to set the newline to the null char since we want the word to terminate, not newline
        words[count++] = strdup(buff); //Store the word
    }
    fclose(f); //We have to close the file after we are done using it

    //We can now randomly choose a word by seeding based on time and return that value by using modulo with the word count
    return words[rand() % count];
}

//This runs the actual game logic in the thread (has to be passed to the thread with the void *)
void * client_thread(void * arg)
{
    //Online, this was an effective way to extract the file descriptor which is essentially a handle to use to access the stream input
    int clientFileDescriptor = *(int*)arg;
    free(arg); //Online, this is supposed to be done so that the temp memory can be deallocated properly and early

    //Now we need to choose the random word to guess and store it
    char * word = chooseWord();
    int wordLength = strlen(word);
    char pattern[32]; //We can set the game text to store 32 characters just as a safe upper bound
    memset(pattern, '_', wordLength); //We want wordLength number of characters to be '_' 
    pattern[wordLength] = 0; //Also need to remember to add the null terminator at the end
    //We need to track incorrect guessed chars and track how many are wrong for game termination
    char incorrect[16] = "";
    int wrong = 0;

    //Wait for start signal by using read to get the first bit of the input to make sure it is empty
    unsigned char msgLength;
    //If the connection terminates or throws an error, the read would return a 0 or less than 0 so the client doesn't want to play
    if(read(clientFileDescriptor, &msgLength, 1) <= 0)
    {
        goto cleanup;
    }
    //If the read was successful, then we know it has to mean that the client wants to start and sent and empty message

    //Once we are ready to start, call the helper to send the pattern and incorrect values to the client stream
    sendGame(clientFileDescriptor, pattern, incorrect);

    while(true)
    {
        unsigned char msgLength;
        //Read the message length field and terminate if it fails (client closed)
        if(read(clientFileDescriptor, &msgLength, 1) <= 0)
        {
            goto cleanup;
        }
        char buf[msgLength + 1]; //The buffer just has to be the message length + space for the null char
        //Read that messageLength bytes to get the guess into the buffer
        read(clientFileDescriptor, buf, msgLength);
        char guess = buf[0]; //First byte of the data is the guess

        //We need to now add in all the correct locations of the guess and track if there were any at all
        int hit = 0;
        for(int i = 0; i < wordLength; ++i)
        {
            if(word[i] == guess)
            {
                pattern[i] = guess;
                hit ++;
            }
        }
        //If nothing hit, then the guess must be wrong
        if(!(hit > 0))
        {
            int len = strlen(incorrect);
            incorrect[len] = guess;
            incorrect[len + 1] = 0; //Update the string end
            wrong ++;
        }

        //If there are no '_' left, then we know the client has won
        if(strchr(pattern, '_') == NULL)
        {
            char word_msg[32];
            //Start the initial prefix of the string
            size_t off = snprintf(word_msg, sizeof(word_msg), "The word was ");
            for(int i = 0; i < wordLength && off + 2 < sizeof(word_msg); ++i)
            {
                //Insert the character and its space (we only want to add a space if it's not the last letter)
                off += snprintf(word_msg + off, sizeof(word_msg) - off, "%c", word[i]);
                if(i + 1 < wordLength)
                {
                    off += snprintf(word_msg + off, sizeof(word_msg) - off, " ");
                }
            }
            //Send the final word, the win message, and the game over message
            sendMessage(clientFileDescriptor, word_msg);
            sendMessage(clientFileDescriptor, "You Win!");
            sendMessage(clientFileDescriptor, "Game Over!");
            //We can terminate the connection now that we have sent the concluding messages
            break;
        }
        //If we have received too many wrong guesses, the client has lost
        if(wrong >= MAX_INCORRECT)
        {
            char word_msg[32];
            //Start the initial prefix of the string
            size_t off = snprintf(word_msg, sizeof(word_msg), "The word was ");
            for(int i = 0; i < wordLength && off + 2 < sizeof(word_msg); ++i)
            {
                //Insert the character and its space (we only want to add a space if it's not the last letter)
                off += snprintf(word_msg + off, sizeof(word_msg) - off, "%c", word[i]);
                if(i + 1 < wordLength)
                {
                    off += snprintf(word_msg + off, sizeof(word_msg) - off, " ");
                }
            }
            //Send the final word, the win message, and the game over message
            sendMessage(clientFileDescriptor, word_msg);
            sendMessage(clientFileDescriptor,"You Lose.");
            sendMessage(clientFileDescriptor, "Game Over!");
            //We can terminate the connection now that we have sent the concluding messages
            break;
        }

        //Now we send the updated game state to the client if it is valid to do so (meaning the game hasn't ended)
        sendGame(clientFileDescriptor, pattern, incorrect);
    }

    //This is similar to a MIPS assembly function so it can get jumped-to with a goto call (like j cleanup)
    cleanup:
        close(clientFileDescriptor);
        pthread_mutex_lock(&lock);
        activeClients--;
        pthread_mutex_unlock(&lock);
        free(word);
        return NULL;
}

int main(int argc, char ** argv)
{
    int port = atoi(argv[2]); //The port is the second argument we get after the IP
    int srv = socket(AF_INET, SOCK_STREAM, 0); //Initialize the socket with TCP (SOCK_STREAM)

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], & addr.sin_addr); //This call converts the string at arv[1] to an IPv4-style address (AF_INET) and stores it in addr.sin_addr
    bind(srv, (struct sockaddr *) & addr, sizeof(addr)); //Bind the socket data so that it has an IP and port
    listen(srv, 5); //Listen for a connection request (the 5 is the queue for requests to pile up before dropping)
    
    while(true)
    {
        //We need to accept a new connection when it arrives and then lock the critical section of the code with a mutex lock to allow for the threading to work
        int clientFileDescriptor = accept(srv, NULL, NULL);
        unsigned char zero = 0;
        write(clientFileDescriptor, &zero, 1); //This is an arbitrary start message so that client stops waiting for a "server-overloaded" messages
        pthread_mutex_lock(&lock);
        //Once we have the connection, we need to see if it is okay to use
        //The obvious check is if the connection we currently have match or exceed the max, we need to drop this one with the overload message
        if(activeClients >= MAX_CLIENTS)
        {
            pthread_mutex_unlock(&lock); //This is no longer critical so we can allow another thread to proceed since we're just dropping
            sendMessage(clientFileDescriptor, "server-overloaded");
            close(clientFileDescriptor);
            continue; //Skip the rest since we dropped the active connection
        }
        sendGame(clientFileDescriptor, "", ""); //This is to initiate the game. Otherwise the server is waiting for a message and the client is waiting, one of them has to send to begin the game.
        activeClients ++;
        pthread_mutex_unlock(&lock); //No longer a critical section of the code so it's okay to have threads switch

        //At this point, we have connected a new client validly, so we need to give them a thread so that the client can play while other active clients exist
        int * clientFileDescriptorCopy = malloc(sizeof(int));
        *clientFileDescriptorCopy = clientFileDescriptor; //We need to store clientFileDescriptor in this dynamically allocated variable because clientFileDescriptor is just a main thread stack value so it can go out of scope
        pthread_t thread;
        pthread_create(& thread, NULL, client_thread, clientFileDescriptorCopy); //We pass in clientFileDescriptor using p so that it is safely passed to stay in scope
        
        //We have to lock the client decrement because it is a crticial operation since the variable is statically shared across all threads
        pthread_mutex_lock(&lock);
        activeClients --;
        pthread_mutex_unlock(&lock);

        pthread_detach(thread); //It will reach this once the thread is done executing, so we can just get rid of that thread since we are done
    }
    return 0;
}
