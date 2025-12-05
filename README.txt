Worked on this solo
The server:
    This works as a standard TCP server, but, to handle multiple connections, it relies on threads!
    The essential logic is that main will create a listening socket on the given port in the args, accepts allowed connections, send a "server-overloaded" message if there are already 3 active clients and close the connection, and spawn a thread to manage each valid connection.
    The thread handles the rest of this with a function made for the thread. By leveraging some hlper functions to send messages to the client and interact with the words file, the client thread function is what runs the actual game logic for this actually.
    It chooses the random word, sends the game state to the client once they are ready, and enters the game loop.
    In the game loop, it takes the guess, checks for its accuracy and updates the word guess, updates the incorrect guesses, and sends the updated game state. When appropriate conditions are met, it will end the game with a win or loss message and close the connection.
The client:
    This works as a standards TCP client, and it has a game loop to maintain persistent data transmissions while the game runs.
    The essential logic is that the main function will connect to the server with the passed in args, prompt the user's readiness, and then handle that response.
    If the user is ready, a "0" is sent since that means the message is of length 0 and thus empty. If the user isn't ready, the connection is gracefully terminated.
    In the game loop, the client must first handle the initial state message from the server. It displays this message to update the game state display.
    The loop is essentially just a prompt, response query, response acceptance or rejection (rejection leads to another query), and a server send.
    Once the game has been won or lost, the connection is gracefully terminated because the server would have terminated the connection so we can detect that.