//#define DEBUG_TTY_FEEDBACK /* Comment out to disable debugging messages */

#ifdef DEBUG_TTY_FEEDBACK
	#define DEBUG_PRINTF(args...) printf(args)
#else
	#define DEBUG_PRINTF(args...)
#endif

//From lwip/err.h and lwip/tcpip.h

#define	ERR_OK		0	//No error, everything OK
#define	ERR_CONN	-6	//Not connected
#define	ERR_IF		-11	//Low-level netif error
