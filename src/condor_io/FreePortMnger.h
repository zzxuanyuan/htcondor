#define FIRST_PORT 1026
#define LAST_PORT 65535
#define MAX_PORT LAST_PORT - FIRST_PORT + 1
#define MAX_SET 30


class PortSet {
	public:
		PortSet ();
		// Find a free port
		// @return:	the free port in network order form.
		//			0 if no more free port remained
		uint16_t freePort ();
		// Find a free port, but caller prefers to use 'port', given in network order
		// @return:	the free port in network order form.
		//			0 if no more free port remained
		uint16_t freePort (uint16_t port);
		// mark the port, given in network order, as being used.
	   	// @return - false if the port is already marked as being used
		bool makeOccupied (uint16_t port);
		// free the port given in network byte order.
	   	// @return - false if the port is already free
		bool makeFree (uint16_t port);
	protected:
		// index to start search from for the next free port
		int _index;
		// arrary of tag indicating the corresponding port is free or not
		bool _freeArr[MAX_PORT];
};

class FreePortMnger {
	public:
		FreePortMnger();
		~FreePortMnger();
		// add the ip address of an interface, which is given in network order,
		// to the list of interfaces
		void addInterface (uint32_t ipAddr);
		// find a free port
		// @return:	true, if a free port found. 'lip' and 'lport' are set with
		//				  the pair of (free ip, free port) both in network order
		//			false, if no free port found for any ip
		bool nextFree (uint32_t *lip, uint16_t *lport);
		// find a free port, but caller prefers to use 'rport', given in network order
		// @return:	true, if a free port found. 'lip' and 'lport' are set with
		//				  the pair of (free ip, free port) both in network order
		//			false, if no free port found for any ip
		bool nextFree (uint16_t rport, uint32_t *lip, uint16_t *lport);
		// mark ('lip', 'lport'), both in network order, as being used
		// @return:	true, if succeed
		//			false, if ('lip', 'lport') is being used by another ...
		bool makeOccupied (uint32_t lip, uint16_t lport);
		// return ('lip', 'lport'), both in network order, to free pool
		// @return:	true, if succeed
		//			false, if failed
		bool makeFree (uint32_t lip, uint16_t lport);
	protected:
		int _noInterfaces;
		int _nextInterface;
		struct {
			uint32_t ip;
			PortSet * portSet;
		} _interfaces[MAX_SET];
};
