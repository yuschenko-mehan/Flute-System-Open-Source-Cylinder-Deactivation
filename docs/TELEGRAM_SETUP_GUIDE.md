📱 STEP-BY-STEP GUIDE: Setting Up Telegram Alerts for Flute System
This guide will help you set up instant notifications to your smartphone in case of critical system errors (e.g., engine synchronization loss or emergency stop).
The entire process takes no more than 3 minutes and requires no programming skills.
⚠️ IMPORTANT: Why do you need to do this manually?
Telegram's security policy prohibits bots from sending messages first. Therefore, we first create the bot, find your personal "address" (Chat ID), and then must "introduce ourselves" to the bot by pressing the Start button. Without this, messages will not be delivered.
STEP 1: Creating the Bot (2 minutes)
Open the Telegram app on your phone or computer.
In the search bar, type: @BotFather (look for the account with a blue checkmark ✅ next to the name – this guarantees it's the official bot).
Press the Start button (or type /start in the chat and send it).
Type the command: /newbot and send it.
BotFather will ask: "Alright, a new bot. How are we going to call it? Please choose a name for your bot."
👉 Type any name, for example: Flute System Monitor and send it.
Next, it will ask: "Good. Now let's choose a username for your bot. It must end in bot."
👉 Type a unique username that must end with the word bot.
Example: my_car_flute_bot or flute_alerts_2024_bot. Send it.
SUCCESS! BotFather will reply with a welcome message and a long string of characters. This is your HTTP API Token.
👉 COPY IT. It looks something like this:
1234567890:ABCdefGHIjklMNOpqrsTUVwxyz
STEP 2: Getting Your Chat ID (30 seconds)
The system needs to know which specific phone to send alerts to.
In Telegram search, find the bot: @userinfobot (or @getidsbot).
Press Start (or type /start).
The bot will instantly reply with a short message. Find the line that says Id or Chat ID.
👉 COPY THESE NUMBERS. This is a set of digits, for example: 987654321.
STEP 3: CRITICAL STEP – "Introducing Yourself" to the Bot (15 seconds)
🚨 Without this step, alerts WILL NOT WORK! (You will get a "403 Forbidden" error).
In Telegram search, find your new bot by the username you created in Step 1, item 6 (e.g., @my_car_flute_bot).
Open the chat with it.
Press the big START button (or type /start and send it).
The bot will reply something like "Hello! I'm ready to work."
✅ Now the bot has permission to send you messages.
STEP 4: Entering Data into the Flute System (1 minute)
Connect your smartphone or laptop to the Wi-Fi network FluteSystem (default password: flute1234).
Open any web browser and enter in the address bar: 192.168.4.1
Scroll down the web interface to the card 📱 Telegram Alerts Setup.
In the Bot Token field, paste the long string copied in Step 1.
In the Your Chat ID field, paste the numbers copied in Step 2.
Click the button Save & Send Test Message.
STEP 5: Verification
If everything is done correctly, a green message will appear under the button on the web page:
✅ Success! Check your Telegram.
Open Telegram. You will instantly receive a message from your bot:
✅ FLUTE SYSTEM: Telegram alerts successfully configured!
Congratulations! 🎉 The system is now protected. If a critical error occurs (for example, if you accidentally press the red "EMERGENCY STOP" button on the website), you will receive a clear notification with a description of the problem directly to your phone, even if you are far from the vehicle.
❓ Troubleshooting (FAQ)
Question: I clicked "Save," but it said "Invalid data" or the Telegram message didn't arrive.
Answer: Most likely, you skipped Step 3. Find your bot in Telegram and be sure to press the START button. After that, try saving the settings in the web interface again.
Question: Will the bot spam me with messages if the error doesn't go away?
Answer: No. The Flute System has built-in intelligent anti-spam protection. It will send a message about a specific error only once. Subsequent notifications will only be sent when the system reports that the error has been cleared and it occurs again.
