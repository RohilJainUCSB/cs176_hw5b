//Used help from searches for TCP client creation, connection establishment, and input management
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

int main(int argc, char ** argv)
{
    //The IP and port are passed in. We need to use these to build the socket and make a connection request to it
    const char * ip = argv[1];
    int port = atoi(argv[2]);
    int fileDescriptor = socket(AF_INET, SOCK_STREAM, 0); //TCP socket with SOCK_STREAM
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; //IPv4
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr); //Convert the IP string into proper IP format
    connect(fileDescriptor, (void*) & addr, sizeof(addr));
    //First packet could be "server-overloaded", so we need to handle that overload termination
    unsigned char flag;
    if(read(fileDescriptor, &flag, 1) <= 0) //If the read has the server closed, we can terminate
    {
        close(fileDescriptor);
        return 0;
    }
    if(flag != 0) //Otherwise if the flag we read is nonzero, then that means the server overloaded message was present so we can print that and terminate
    {
        char msg[256];
        read(fileDescriptor, msg, flag);
        msg[flag] = 0;
        printf("%s\n", msg);
        close(fileDescriptor);
        return 0;
    }

    //We need to query user readiness and handle each case
    printf("Ready to start game? (y/n): ");
    fflush(stdout); //This forces the buffer to print because there are occasional issues with this not displaying
    char line[64]; //Went with 64 bit buffer just arbitrarily to be safe
    fgets(line, 64, stdin);
    if(line[0] == 'n' || line[0] == 'N') //If they don't want to play, terminate
    {
        close(fileDescriptor);
        return 0;
    }
    //Otherwise they want to play so we can send our empty "0" message to the server
    unsigned char zero = 0;
    write(fileDescriptor, &zero, 1);

    //Read the initial managerial empty message to get it out of the buffer. This is purely operational and prints no output
    if(read(fileDescriptor, &flag, 1) <= 0) //Error checking to make sure server is still live
    {
        close(fileDescriptor);
        return 0;
    }
    if(flag == 0)
    {
        unsigned char wordLength, numIncorrect;
        read(fileDescriptor, &wordLength, 1);
        read(fileDescriptor, &numIncorrect, 1);
        char pattern[32], incorrect[16];
        read(fileDescriptor, pattern, wordLength);
        pattern[wordLength] = 0; //Null char
        read(fileDescriptor, incorrect, numIncorrect);
        incorrect[numIncorrect] = 0; //Null char
    }

    //Game loop
    while(true)
    {
        //If we ever fail to read, then the server has terminated, so we need to terminate
        if(read(fileDescriptor, &flag, 1) <= 0)
        {
            break;
        }
        //If we read a non-game-state message (flag is nonzero), we need to print it and then continue
        if(flag != 0)
        {
            char msg[256];
            read(fileDescriptor, msg, flag);
            msg[flag] = 0; //Set the null char
            printf("%s\n", msg);
            fflush(stdout);
            //If this was the final message we can exit the loop
            if(strcmp(msg, "Game Over!") == 0)
            {
                break;
            }
            continue; //We can proceed to the next loop
        }
        //By now, we know we have a game state message, so we can read it in format
        unsigned char wordLength, numIncorrect;
        read(fileDescriptor, &wordLength, 1);
        read(fileDescriptor, &numIncorrect, 1);
        char pattern[32], incorrect[16];
        read(fileDescriptor, pattern, wordLength);
        pattern[wordLength] = 0; //Set the null char
        read(fileDescriptor, incorrect, numIncorrect);
        incorrect[numIncorrect] = 0; //Set null char
        
        //Now we can print the game string to guess the word from
        for(int i = 0; i < wordLength; ++i)
        {
            printf("%c ", pattern[i]);
            fflush(stdout);
        }
        printf("\n");
        //Now we can print the incorrect guesses
        printf("Incorrect Guesses: %s\n", incorrect);
        printf("\n");
        fflush(stdout);

        //If a game end condition has been met (No letter left to guess or too many incorrect)
        if(strchr(pattern, '_') == NULL || strlen(incorrect) >= 6)
        {
            //The server will send You Win!/You Lose. and then Game Over!
            while(true)
            {
                // Read the message length
                if(read(fileDescriptor, &flag, 1) <= 0)
                {
                    //If the read fails, we need to clean up
                    goto cleanup;
                }
                if(flag == 0)
                {
                    //Consume the extra game state packets and continue checking.
                    unsigned char wordLength, numIncorrect;
                    if(read(fileDescriptor, &wordLength, 1) <= 0)
                    {
                        goto cleanup;
                    }
                    if(read(fileDescriptor, &numIncorrect, 1) <= 0)
                    {
                        goto cleanup;
                    }
                    char tempPattern[64], tempIncorrect[64];
                    if(wordLength > 0)
                    {
                        read(fileDescriptor, tempPattern, wordLength);
                    }
                    tempPattern[wordLength] = 0;
                    if(numIncorrect > 0)
                    {
                        read(fileDescriptor, tempIncorrect, numIncorrect);
                    }
                    tempIncorrect[numIncorrect] = 0;
                    //Loop again to look for messages
                    continue;
                }
                //Once we reach here, we have a message packet
                char msg[256];
                read(fileDescriptor, msg, flag);
                msg[flag] = 0;
                printf("%s\n", msg);
                fflush(stdout);

                if(strcmp(msg, "Game Over!") == 0)
                {
                    //Once we have the Game Over! we can finally terminate
                    goto cleanup;
                }
                //Otherwise we keep reading the next message to get "You Win!" then "Game Over!"
            }
        }

        //Now we need to do the guess loop to receive, approve, and send a guess
        char guess;
        while(true)
        {
            printf("Letter to guess: "); //Query string
            fgets(line, 64, stdin); //Read the input
            //We need to validate the input. It must be a single character followed by a newline or string terminator
            if(!isalpha(line[0]) || (line[1] != '\n' && line[1] != '\0'))
            {
                printf("Error! Please guess one letter.\n");
                continue;
            }

            //We need to set it to be lowercase since that isn't guarunteed
            guess = tolower(line[0]);
            break;
        }
        //Finally we send the guess to the server
        unsigned char len = 1;
        write(fileDescriptor, &len, 1);
        write(fileDescriptor, &guess,1);
    }
    //At the end, once the server has terminated, we can terminate
    cleanup:
        close(fileDescriptor);
        return 0;
}