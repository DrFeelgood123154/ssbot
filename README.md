# ssbot
This is a MMO game bot made mostly for fun in C++17. The game is "2.5d", the bot's efficiency is at least two times of a good player's.

The bot has three main uses
1. g r i n d i n g much faster than an average player while using multiple accounts, amount of accounts is only limited by RAM capacity
2. exploiting the resources that randomly spawn in the large game universe before any player has a chance to find them, leaving the universe empty of prospectable resources
3. assisting the player by taking control of selected accounts and acting as a party member on each of them

It's separated into controller and worker.
Controller is a server which coordinates multiple machines that run the bot, once a worker connects it will send it's config data to the server and with that server decides what each account that the worker runs has to do
Workers can also sync their data with controller upon connection, or when another worker uploads data.

Worker is what gets the job done, it can run multiple accounts in the game and do stuff. Each account can do something different. It can work without controller.

Both worker and controller can report issues (or anything) to a web server.
