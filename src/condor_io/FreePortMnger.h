#define NAT_FIRST_PORT 1026
#define NAT_LAST_PORT 65535
#define NAT_MAX_PORT NAT_LAST_PORT - NAT_FIRST_PORT + 1
#define NAT_MAX_SET 30


class PortSet {
	public:
		// ip should be in network byte order
		PortSet (unsigned int ip);
		// return the next free port. return 0 if no more free port remained
		unsigned int freePort ();
		// free the port given in network byte order.
	   	// @return - false if the port is already free
		bool makeFree (unsigned short port);
		// return the ip address of the interface in network byte order
		unsigned int interface ();
	protected:
		// index to start search from for the next free port
    	int _index;
		// arrary of tag indicating the corresponding port is free or not
		bool _freeArr[NAT_MAX_PORT];
};

class FreePortMnger {
	public:
		void addInterface(unsigned int ipAddr);
		bool nextFree (unsigned int *lip, unsigned short *lport);
		bool makeFree (unsigned int lip, unsigned short lport);
	protected:
		int _noInterfaces;
		int _nextInterface;
		struct {
			unsigned int ip;
			PortSet * portSet;
		} _interfaces[NAT_MAX_SET];
};
