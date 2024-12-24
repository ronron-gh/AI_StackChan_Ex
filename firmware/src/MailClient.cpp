#include "ESP_Mail_Client.h"
//#include "extras/SDHelper.h"  // Provide the SD card interfaces setting and mounting
#include "MailClient.h"

String authMailAdr = "";    // "XXXXXXXX@gmail.com"
String authAppPass = "";   // "XXXXXXXXXXXXXXXX"
String toMailAdr = "";    // "XXXXXXXX@XXX.com"
IMAPSession imap;

TimerHandle_t xTimerReadMail;
std::deque<String> recvMessages;
int prev_nMail = 0;

// 内部参照関数プロトタイプ
void imapCallback(IMAP_Status status);  /* Callback function to get the Email reading status */
void printAllMailboxesInfo(IMAPSession &imap);  /* Print the list of mailbox folders */
void printSelectedMailboxInfo(SelectedFolderInfo sFolder);  /* Print the selected folder info */
void printMessages(MB_VECTOR<IMAP_MSG_Item> &msgItems, bool headerOnly);  /* Print all messages from the message list */
void printAttacements(MB_VECTOR<IMAP_Attach_Item> &atts); /* Print all attachments info from the message */


void imap_init(void){

  // IMAP設定
  MailClient.networkReconnect(true);
  Serial.println("networkReconnect done.");

#if defined(ESP_MAIL_DEFAULT_SD_FS) // defined in src/ESP_Mail_FS.h
  // Mount SD card.
//  SD_Card_Mounting(); // See src/extras/SDHelper.h
#endif

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  imap.debug(1);

  /* Set the callback function to get the reading results */
  imap.callback(imapCallback);

  /** In case the SD card/adapter was used for the file storagge, the SPI pins can be configure from
   * MailClient.sdBegin function which may be different for ESP32 and ESP8266
   * For ESP32, assign all of SPI pins
   * MailClient.sdBegin(14,2,15,13)
   * Which SCK = 14, MISO = 2, MOSI = 15 and SS = 13
   * And for ESP8266, assign the CS pins of SPI port
   * MailClient.sdBegin(15)
   * Which pin 15 is the CS pin of SD card adapter
   */


}

/* Callback function to get the Email reading status */
void imapCallback(IMAP_Status status)
{
    /* Print the current status */
    Serial.println(status.info());

    /* Show the result when reading finished */
    if (status.success())
    {
        /* Print the result */
        /* Get the message list from the message list data */
        IMAP_MSG_List msgList = imap.data();

        printMessages(msgList.msgItems, imap.headerOnly());

        // 受信したメッセージをキューに格納
        IMAP_MSG_Item msg = msgList.msgItems[0];
        if(!strstr(msg.flags, "Seen")){
            Serial.println("Save unseen mail");
            Serial.printf("content: %s", msg.text.content);

            recvMessages.push_back(msg.text.content);
            //メールが5個を超えたら古いものを消す
            if (recvMessages.size() > 5)
            {
                recvMessages.pop_front();
            }
            
            /** Set \Seen and \Answered to flags for message with UID
             * The seesion will keep open.
             */
            if (MailClient.setFlag(&imap, msg.UID, F("\\Seen"), false))
                Serial.println("\nSetting FLAG success");
            else
                Serial.println("\nError, setting FLAG");
        }

        imap.closeSession();

        /* Clear all stored data in IMAPSession object */
        imap.empty();
    }
}

void printAllMailboxesInfo(IMAPSession &imap)
{
    /* Declare the folder collection class to get the list of mailbox folders */
    FoldersCollection folders;

    /* Get the mailbox folders */
    if (imap.getFolders(folders))
    {
        for (size_t i = 0; i < folders.size(); i++)
        {
            /* Iterate each folder info using the  folder info item data */
            FolderInfo folderInfo = folders.info(i);
            ESP_MAIL_PRINTF("%s%s%s", i == 0 ? "\nAvailable folders: " : ", ", folderInfo.name, i == folders.size() - 1 ? "\n" : "");
        }
    }
}

void printSelectedMailboxInfo(SelectedFolderInfo sFolder)
{
    /* Show the mailbox info */
    ESP_MAIL_PRINTF("\nInfo of the selected folder\nTotal Messages: %d\n", sFolder.msgCount());
    ESP_MAIL_PRINTF("UID Validity: %d\n", sFolder.uidValidity());
    ESP_MAIL_PRINTF("Predicted next UID: %d\n", sFolder.nextUID());
    if (sFolder.unseenIndex() > 0)
        ESP_MAIL_PRINTF("First Unseen Message Number: %d\n", sFolder.unseenIndex());
    else
        ESP_MAIL_PRINTF("Unseen Messages: No\n");

    if (sFolder.modSeqSupported())
        ESP_MAIL_PRINTF("Highest Modification Sequence: %llu\n", sFolder.highestModSeq());
    for (size_t i = 0; i < sFolder.flagCount(); i++)
        ESP_MAIL_PRINTF("%s%s%s", i == 0 ? "Flags: " : ", ", sFolder.flag(i).c_str(), i == sFolder.flagCount() - 1 ? "\n" : "");

    if (sFolder.flagCount(true))
    {
        for (size_t i = 0; i < sFolder.flagCount(true); i++)
            ESP_MAIL_PRINTF("%s%s%s", i == 0 ? "Permanent Flags: " : ", ", sFolder.flag(i, true).c_str(), i == sFolder.flagCount(true) - 1 ? "\n" : "");
    }
}

void printAttacements(MB_VECTOR<IMAP_Attach_Item> &atts)
{
    ESP_MAIL_PRINTF("Attachment: %d file(s)\n****************************\n", atts.size());
    for (size_t j = 0; j < atts.size(); j++)
    {
        IMAP_Attach_Item att = atts[j];
        /** att.type can be
         * esp_mail_att_type_none or 0
         * esp_mail_att_type_attachment or 1
         * esp_mail_att_type_inline or 2
         */
        ESP_MAIL_PRINTF("%d. Filename: %s, Name: %s, Size: %d, MIME: %s, Type: %s, Description: %s, Creation Date: %s\n", j + 1, att.filename, att.name, att.size, att.mime, att.type == esp_mail_att_type_attachment ? "attachment" : "inline", att.description, att.creationDate);
    }
    Serial.println();
}

void printMessages(MB_VECTOR<IMAP_MSG_Item> &msgItems, bool headerOnly)
{

    /** In devices other than ESP8266 and ESP32, if SD card was chosen as filestorage and
     * the standard SD.h library included in ESP_Mail_FS.h, files will be renamed due to long filename
     * (> 13 characters) is not support in the SD.h library.
     * To show how its original file name, use imap.fileList().
     */
    // Serial.println(imap.fileList());

    for (size_t i = 0; i < msgItems.size(); i++)
    {

        /* Iterate to get each message data through the message item data */
        IMAP_MSG_Item msg = msgItems[i];

        Serial.println("****************************");
        ESP_MAIL_PRINTF("Number: %d\n", msg.msgNo);
        ESP_MAIL_PRINTF("UID: %d\n", msg.UID);

        // The attachment status in search may be true in case the "multipart/mixed"
        // content type header was set with no real attachtment included.
        ESP_MAIL_PRINTF("Attachment: %s\n", msg.hasAttachment ? "yes" : "no");

        ESP_MAIL_PRINTF("Messsage-ID: %s\n", msg.ID);

        if (strlen(msg.flags))
            ESP_MAIL_PRINTF("Flags: %s\n", msg.flags);
        if (strlen(msg.acceptLang))
            ESP_MAIL_PRINTF("Accept Language: %s\n", msg.acceptLang);
        if (strlen(msg.contentLang))
            ESP_MAIL_PRINTF("Content Language: %s\n", msg.contentLang);
        if (strlen(msg.from))
            ESP_MAIL_PRINTF("From: %s\n", msg.from);
        if (strlen(msg.sender))
            ESP_MAIL_PRINTF("Sender: %s\n", msg.sender);
        if (strlen(msg.to))
            ESP_MAIL_PRINTF("To: %s\n", msg.to);
        if (strlen(msg.cc))
            ESP_MAIL_PRINTF("CC: %s\n", msg.cc);
        if (strlen(msg.date))
        {
            ESP_MAIL_PRINTF("Date: %s\n", msg.date);
            ESP_MAIL_PRINTF("Timestamp: %d\n", (int)MailClient.Time.getTimestamp(msg.date));
        }
        if (strlen(msg.subject))
            ESP_MAIL_PRINTF("Subject: %s\n", msg.subject);
        if (strlen(msg.reply_to))
            ESP_MAIL_PRINTF("Reply-To: %s\n", msg.reply_to);
        if (strlen(msg.return_path))
            ESP_MAIL_PRINTF("Return-Path: %s\n", msg.return_path);
        if (strlen(msg.in_reply_to))
            ESP_MAIL_PRINTF("In-Reply-To: %s\n", msg.in_reply_to);
        if (strlen(msg.references))
            ESP_MAIL_PRINTF("References: %s\n", msg.references);
        if (strlen(msg.comments))
            ESP_MAIL_PRINTF("Comments: %s\n", msg.comments);
        if (strlen(msg.keywords))
            ESP_MAIL_PRINTF("Keywords: %s\n", msg.keywords);

        /* If the result contains the message info (Fetch mode) */
        if (!headerOnly)
        {
            if (strlen(msg.text.content))
                ESP_MAIL_PRINTF("Text Message: %s\n", msg.text.content);
            if (strlen(msg.text.charSet))
                ESP_MAIL_PRINTF("Text Message Charset: %s\n", msg.text.charSet);
            if (strlen(msg.text.transfer_encoding))
                ESP_MAIL_PRINTF("Text Message Transfer Encoding: %s\n", msg.text.transfer_encoding);
            if (strlen(msg.html.content))
                ESP_MAIL_PRINTF("HTML Message: %s\n", msg.html.content);
            if (strlen(msg.html.charSet))
                ESP_MAIL_PRINTF("HTML Message Charset: %s\n", msg.html.charSet);
            if (strlen(msg.html.transfer_encoding))
                ESP_MAIL_PRINTF("HTML Message Transfer Encoding: %s\n\n", msg.html.transfer_encoding);

            if (msg.rfc822.size() > 0)
            {
                ESP_MAIL_PRINTF("\r\nRFC822 Messages: %d message(s)\n****************************\n", msg.rfc822.size());
                printMessages(msg.rfc822, headerOnly);
            }

            if (msg.attachments.size() > 0)
                printAttacements(msg.attachments);
        }

        Serial.println();
    }
}

void imapReadMail(void){

  /* Declare the Session_Config for user defined session credentials */
  Session_Config config;

  /* Set the session config */
  config.server.host_name = IMAP_HOST;
  config.server.port = IMAP_PORT;
  config.login.email = authMailAdr.c_str();
  config.login.password = authAppPass.c_str();

  /* Define the IMAP_Data object used for user defined IMAP operating options. */
  IMAP_Data imap_data;

  /* Set the storage to save the downloaded files and attachments */
  imap_data.storage.saved_path = F("/email_data");

  /** The file storage type e.g.
   * esp_mail_file_storage_type_none,
   * esp_mail_file_storage_type_flash, and
   * esp_mail_file_storage_type_sd
   */
//  imap_data.storage.type = esp_mail_file_storage_type_sd;
  imap_data.storage.type = esp_mail_file_storage_type_none;

  /** Set to download headers, text and html messaeges,
   * attachments and inline images respectively.
   */
  imap_data.download.header = true;
  imap_data.download.text = true;
  //imap_data.download.html = true;
  imap_data.download.html = false;
  //imap_data.download.attachment = true;
  imap_data.download.attachment = false;
  //imap_data.download.inlineImg = true;
  imap_data.download.inlineImg = false;

  /** Set to enable the results i.e. html and text messaeges
   * which the content stored in the IMAPSession object is limited
   * by the option imap_data.limit.msg_size.
   * The whole message can be download through imap_data.download.text
   * or imap_data.download.html which not depends on these enable options.
   */
  //imap_data.enable.html = true;
  imap_data.enable.html = false;
  imap_data.enable.text = true;

  /* Set to enable the sort the result by message UID in the decending order */
  imap_data.enable.recent_sort = true;

  /* Set to report the download progress via the default serial port */
  imap_data.enable.download_status = true;

  /* Header fields parsing is case insensitive by default to avoid uppercase header in some server e.g. iCloud
  , to allow case sensitive parse, uncomment below line*/
  // imap_data.enable.header_case_sensitive = true;

  /* Set the limit of number of messages in the search results */
  //imap_data.limit.search = 5;
  imap_data.limit.search = 1;      //TODO 本当は3通くらい受信したいが、現状、既読メールを毎回受信してしまうので、1にしておく

  /** Set the maximum size of message stored in
   * IMAPSession object in byte
   */
  //imap_data.limit.msg_size = 512;
  imap_data.limit.msg_size = 256;

  /** Set the maximum attachments and inline images files size
   * that can be downloaded in byte.
   * The file which its size is largger than this limit may be saved
   * as truncated file.
   */
//  imap_data.limit.attachment_size = 1024 * 1024 * 5;

  // If ID extension was supported by IMAP server, assign the client identification
  // name, version, vendor, os, os_version, support_url, address, command, arguments, environment
  // Server ID can be optained from imap.serverID() after calling imap.connect and imap.id.

  // imap_data.identification.name = "User";
  // imap_data.identification.version = "1.0";

  /* Set the TCP response read timeout in seconds */
  // imap.setTCPTimeout(10);

  /* Connect to the server */
  if (!imap.connect(&config, &imap_data)){
    Serial.println("imap.connect() failed.");
    return;
  }

    
  /** Or connect without log in and log in later

    if (!imap.connect(&config, &imap_data, false))
      return;

    if (!imap.loginWithPassword(AUTHOR_EMAIL, AUTHOR_PASSWORD))
      return;
  */

  // Client identification can be sent to server later with
  /**
   * IMAP_Identification iden;
   * iden.name = "user";
   * iden.version = "1.0";
   *
   * if (imap.id(&iden))
   * {
   *    Serial.println("\nSend Identification success");
   *    Serial.println(imap.serverID());
   * }
   * else
   *    ESP_MAIL_PRINTF("nIdentification sending error, Error Code: %d, Reason: %s", imap.errorCode(), imap.errorReason().c_str());
   */

  if (!imap.isLoggedIn())
  {
      Serial.println("\nNot yet logged in.");
  }
  else
  {
      if (imap.isAuthenticated())
          Serial.println("\nSuccessfully logged in.");
      else
          Serial.println("\nConnected with no Auth.");
  }

  /*  {Optional} */
  printAllMailboxesInfo(imap);

  /* Open or select the mailbox folder to read or search the message */
  if (!imap.selectFolder(F("INBOX")))
      return;

  /*  {Optional} */
  printSelectedMailboxInfo(imap.selectedFolder());

  //TODO 未読メールがなければ受信せずにcloseSessionしたい


  /** Message UID to fetch or read e.g. 100.
   * In this case we will get the UID from the max message number (lastest message)
   */
  imap_data.fetch.uid = imap.getUID(imap.selectedFolder().msgCount());

  // if IMAP server supports CONDSTORE extension, the modification sequence can be assign to fetch command
  // Note that modsequence value supports in this library is 32-bit integer
  imap_data.fetch.modsequence = 123;

  // To fetch only header part
  // imap_data.fetch.headerOnly = true;

  // or fetch via the message sequence number
  // imap_data.fetch.number = imap.selectedFolder().msgCount();

  // if both imap_data.fetch.uid and imap_data.fetch.number were set,
  // then total 2 messages will be fetched i.e. one using uid and other using number.

  /* Set seen flag */

  // The message with "Seen" flagged means the message was already read or seen by user.
  // The default value of this option is set to false.
  // If you want to set the message flag as "Seen", set this option to true.
  // If this option is false, the message flag was unchanged.
  // To set or remove flag from message, see Set_Flags.ino example.

  //imap_data.fetch.set_seen = true;

  /* Fetch or read only message header */
  // imap_data.fetch.headerOnly = true;

  /* Read or search the Email and close the session */

  // When message was fetched or read, the /Seen flag will not set or message remained in unseen or unread status,
  // as this is the purpose of library (not UI application), user can set the message status as read by set \Seen flag
  // to message, see the Set_Flags.ino example.

  MailClient.readMail(&imap, false);

  /* Clear all stored data in IMAPSession object */
  imap.empty();

}
