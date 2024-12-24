#ifndef _MAIL_CLIENT_H_
#define _MAIL_CLIENT_H_

#include <deque>

// IMAP
#undef ENABLE_CUSTOM_CLIENT
#define IMAP_HOST "imap.gmail.com"
#define IMAP_PORT 993

extern String authMailAdr;    // "XXXXXXXX@gmail.com"
extern String authAppPass;    // "XXXXXXXXXXXXXXXX"
extern String toMailAdr;      // "XXXXXXXX@XXX.com"

extern std::deque<String> recvMessages;
extern int prev_nMail;

// 関数プロトタイプ
void imap_init(void);
void imapReadMail(void);


#endif //_MAIL_CLIENT_H_