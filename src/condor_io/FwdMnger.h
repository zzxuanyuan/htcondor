#include <map>
#include <algorithm>

#include <linux/ip_masq.h>

/* ip forward specifics */
#define IPPROTO_NONE 65535
#define IP_PORTFW_DEF_PREF 10

#define _pfw _masq.u.portfw_user

#define NAT_MAX_HASH 29


typedef struct fwRule {
	unsigned int lip;
	unsigned short lport;
	unsigned int rip;
	unsigned short rport;
	unsigned short mport;
	struct fwRule * next;
} Rule;

class FwdMnger {
	public:
		/// @args: proto: "tcp" or "udp"
		///		   rawSock: raw socket through which port forwarding rule set up
		FwdMnger (char *proto, int rawSock);

		/// NOTE: Throughout this file, port forwarding rule will be refered as
		///       (lip, lport)->(rip, rport) or (lip, lport)->(rip, rport, mport).
		///       lip and lport refer ip-addr and port # of filewall machine(or
		///       forwarding server), respectively. lip and lport will be refered as
		///       public ip and port, respectively, too. And rip and rport refer ip-addr
		///       and port # of a machine behind the firewall. Hence only lip and lport
		///       will be seen to machines outside of the private network and should
		///       be advertized to CM. mport is a port # of machine behind firewall
		///       through which forwarding server and Cedar of the machine communicate.
		/// NOTE: all ip and port # are in network byte order

		/// Synchronize internal data structure and condor persist file against kernel
		/// forwarding file. This function will be called periodically inside of timeout
		/// handler.
		void sync ();

		/// Add a public ip address of firewall machine.
		/// Note that firewall machine can have multiple public ip addrresses and, in this
		/// case, those ip addrs will be used in round-robin to setup forwarding rules
		/// from a public ip, port pair to private ip and port.
		///
		/// @args: dotNotation: ip addr of the firewall machine in the form of
		///                     xxxx.xxxx.xxxx.xxxx.
		void addInterface (char *dotNotation);

		/// Find a pair of free public (ip, port) and set (lip, lport) with these values
		/// and the add a forwarding rule (lip, lport)->(rip, rport, mport). See addInternal.
		///    - update internal data structure
		///    - append the rule to the persist file
		///    - set the forwarding rule inside kernel
		///
		/// @return: SUCCESS, if success
		///          err code, otherwise
		int addRule(unsigned int *lip,
					unsigned short *lport,
					const unsigned int rip,
					const unsigned short rport,
					unsigned short m_port);


		/// Find the port forwarding rule (lip, lport)->(?ip, ?port, ?mport) and set 
		/// rip and rport with ?ip and ?port respectively.
		///
		/// @return: SUCCESS, if success
		///          err code, otherwise
		int queryRule (	const unsigned int lip,
						const unsigned short lport,
						unsigned int *rip,
						unsigned short *rport);

		/// Delete a port forwarding rule
		/// Find the port forwarding rule (lip, lport)->(?ip, ?port, ?mport) and set 
		/// rip and rport with ?ip and ?port respectively. And then delete the rule.
		///		- delete the rule from the internal representation
		///		- delete the kernel forwarding entry
		///		- delete the rule from condor persist file
		///
		/// @return: SUCCESS, if success
		///			 err code, otherwise
		/// 
		/// Possible enhancement: For performance reason, we could leave the rule in
		/// condor forwarding file NOT deleted. This will results in
		/// the situation where internal representation and kernel table agree but either
		/// of them does not agree with condor persist file. Because condor persist
		/// file is not refered for normal operation, this inconsistency does not cause
		/// any problem. Furthermore sync() will get rid of this discrepancy periodically.
		int deleteRule (const unsigned int lip,
						const unsigned short lport,
						unsigned int * rip,
						unsigned short * rport);

		/// Get the list of forwarding rules which are currently effective.
		///
		/// Note: The list of rules should be free by the caller.
		Rule * getRules ();

	protected:
		// Raw socket through which forwarding rules are setup
		int _rawSock;
		// Name of the local file to store rules
		char * _persistFile;
		// Hashed buckets of forwarding rules
		Rule * _rules[NAT_MAX_HASH];
		// Number of interfaces, known to Internet, of this machine
		int _noInterfaces;
		PortSet * _portSets[NAT_MAX_SET];
		int _nextInterface;
		int _protocol;
		struct ip_masq_ctl _masq;

		/* Method */

		/// Delete a rule (lip, lport) -> (rip, rport, ?) from condor forward file
		///
		/// @return: SUCCESS, if success
		///			 error code, otherwise
		int deletePersist (	const unsigned int lip,
							const unsigned short lport,
							const unsigned int rip,
							const unsigned short rport);

		/// Add port forwarding rule to internal data structure.
		///
		/// When neither of lip and lport is 0, a rule (lip, lport) -> (rip, rport, mport)
		/// will be set, unless the rule has already been set before. When lip or
		/// lport is 0, this function finds a free (ip addr, port #) from a pool of public
		/// ip and port # of firewall machine and set lip and lport with those value found
		/// respectively, and then add forwarding rule (lip, lport) -> ( rip, rport, mport)
		/// to internal data structure.
		///
		/// @return: SUCCESS, if success
		///			 ALREADY_SET, if nonzero lip and lport is given and the rule
		///			              (lip, lport)->(rip, rport, mport) has been set before
		///          err code, otherwise
		///
		int addInternal(unsigned int * lip,
						unsigned short * lport,
						unsigned int rip,
						unsigned short rport,
						unsigned short mport);

		/// Delete a port forwarding rule from the internal representation.
		/// Find the port forwarding rule (lip, lport)->(?ip, ?port, ?mport) and set 
		/// rip and rport with ?ip and ?port respectively. And then delete the rule.
		///
		/// @return: SUCCESS, if success
		///			 error code, otherwise
		int deleteInternal (unsigned int lip,
							unsigned short lport,
							unsigned int * rip,
							unsigned short * rport);
		
};


class FwdServer : public Service {
	public:
		FwdServer (unsigned int ip, FwdMnger * tcpMnger, FwdMnger * udpMnger);
		void probeCedars ();
		int handleCommand (Stream *cmdSock);
	private:
		unsigned int _ipAddr;
		FwdMnger * _tcpPortMnger;
		FwdMnger * _udpPortMnger;
};
