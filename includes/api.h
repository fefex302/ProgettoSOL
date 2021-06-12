#if !defined(API)
#define API

//richieste mandabili al server sottoforma di numeri
#define OPN 	1	//open
#define OPNC 	2	//open create mode
#define OPNL 	3	//open lock mode
#define OPNCL 	4	//open create and lock mode
#define RD 		5	
#define RDN 	6
#define APP 	7
#define LO 		8
#define UN 		9
#define CLS 	10
#define RM 		11
#define CLOSE 	12
#define WRT		13

#define O_CREATE 1
#define O_LOCK 2
#define O_LOCK_CREATE 3

int openConnection(const char* sockname, int msec,const struct timespec abstime);

int closeConnection(const char* sockname);

int writeFile(const char* pathname, const char*dirname);

int openFile(const char* pathname, int flags);

int readFile(const char* pathname, void** buf, size_t* size);

int readNFiles(int N, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int lockFile(const char* pathname);

int unlockFile(const char* pathname);

int closeFile(const char* pathname);

int removeFile(const char* pathname);

#endif
