/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_GAHP_CLIENT_H
#define CONDOR_GAHP_CLIENT_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "classad_hashtable.h"
#include "globus_utils.h"
#include "proxymanager.h"

		class Gahp_Args {
			public:
				Gahp_Args();
				~Gahp_Args();
				void free_argv();
				char **argv;
				int argc;
		};

		class Gahp_Buf {
			public:
				Gahp_Buf(int size);
				~Gahp_Buf();
				char *buffer;
		};

struct GahpProxyInfo
{
	Proxy *proxy;
	int id;
	int cached_expiration;
};

static const char *GAHPCLIENT_DEFAULT_SERVER_ID = "DEFAULT";
static const char *GAHPCLIENT_DEFAULT_SERVER_PATH = "DEFAULT";

// Additional error values that GAHP calls can return
///
static const int GAHPCLIENT_COMMAND_PENDING = -100;
///
static const int GAHPCLIENT_COMMAND_NOT_SUPPORTED = -101;
///
static const int GAHPCLIENT_COMMAND_NOT_SUBMITTED = -102;
///
static const int GAHPCLIENT_COMMAND_TIMED_OUT = -103;

// Special values for what proxy to use with commands
//   Use default proxy (not any cached proxy)
//static const GahpProxyInfo *GAHPCLIENT_CACHE_DEFAULT_PROXY = ((GahpProxyInfo *)1);
#define GAHPCLIENT_CACHE_DEFAULT_PROXY ((GahpProxyInfo *)1)
//   Use last proxy (use whatever proxy was used for the previous command)
//static const GahpProxyInfo *GAHPCLIENT_CACHE_LAST_PROXY = ((GahpProxyInfo *)2);
#define GAHPCLIENT_CACHE_LAST_PROXY ((GahpProxyInfo *)2)

class GahpClient;

class GahpServer : public Service {
 public:
	static GahpServer *FindOrCreateGahpServer(const char *id, const char *path);
	static HashTable <HashKey, GahpServer *> GahpServersById;

	GahpServer(const char *id, const char *path);
	~GahpServer();

	bool Startup();
	bool Initialize(const char * proxy_path);

	char **read_argv(Gahp_Args &g_args);
	char **read_argv(Gahp_Args *g_args) { return read_argv(*g_args); }
	void write_line(const char *command);
	void write_line(const char *command,int req,const char *args);
	void Reaper(Service*,int pid,int status);
	int pipe_ready();

	int doProxyCheck();
	GahpProxyInfo *RegisterProxy( const char *proxy_path );

	/** Set interval to automatically poll the Gahp Server for results.
		If the Gahp server supports async result notification, then
		the poll interval defaults to zero (disabled).  Otherwise,
		it will default to 5 seconds.  
	 	@param interval Polling interval in seconds, or zero
		to disable polling all together.  
		@return true on success, false otherwise.
		@see getPollInterval
	*/
	void setPollInterval(unsigned int interval);

	/** Retrieve the interval used to auto poll the Gahp Server 
		for results.  Also used to determine if async notification
		is in effect.
	 	@return Polling interval in seconds, or a zero
		to represent auto polling is disabled (likely if
		the Gahp server supports async notification).
		@see setPollInterval
	*/
	unsigned int getPollInterval();

	/** Immediately poll the Gahp Server for results.  Normally,
		this method is invoked automatically either by a timer set
		via setPollInterval or by a Gahp Server async result
		notification.
		@return The number of pending commands which have completed
		since the last poll (note: could easily be zero).	
		@see setPollInterval
	*/
	int poll();

	void poll_real_soon();

	bool cacheProxyFromFile( GahpProxyInfo *new_proxy );
	bool uncacheProxy( GahpProxyInfo *gahp_proxy );
	bool useCachedProxy( GahpProxyInfo *new_proxy, bool force = false );

	bool command_cache_proxy_from_file( GahpProxyInfo *new_proxy );
	bool command_use_cached_proxy( GahpProxyInfo *new_proxy );

		// Methods for private GAHP commands
	bool command_version(bool banner_string = false);
	bool command_initialize_from_file(const char *proxy_path,
									  const char *command=NULL);
	bool command_commands();
	bool command_async_mode_on();
	bool command_response_prefix(const char *prefix);

	unsigned int m_reference_count;
	HashTable<int,GahpClient*> *requestTable;
	Queue<int> waitingToSubmit;

	int m_gahp_pid;
	int m_reaperid;
	int m_gahp_readfd;
	int m_gahp_writefd;
	char m_gahp_version[150];
	StringList * m_commands_supported;
	bool use_prefix;
	unsigned int m_pollInterval;
	int poll_tid;
	bool poll_pending;
	int max_pending_requests;
	int num_pending_requests;
	GahpProxyInfo *current_proxy;
	bool skip_next_r;
	char *binary_path;
	char *my_id;

	char *globus_gass_server_url;
	char *globus_gt2_gram_callback_contact;
	void *globus_gt2_gram_user_callback_arg;
	globus_gram_client_callback_func_t globus_gt2_gram_callback_func;
	int globus_gt2_gram_callback_reqid;

	char *globus_gt3_gram_callback_contact;
	void *globus_gt3_gram_user_callback_arg;
	globus_gram_client_callback_func_t globus_gt3_gram_callback_func;
	int globus_gt3_gram_callback_reqid;

	GahpProxyInfo *master_proxy;
	int proxy_check_tid;
	bool is_initialized;
	HashTable<HashKey,GahpProxyInfo*> *ProxiesByFilename;
}; // end of class GahpServer
	
///
class GahpClient : public Service {
	
	friend GahpServer;
	public:
		
		/** @name Instantiation. 
		 */
		//@{
	
			/// Constructor
		GahpClient(const char *id=GAHPCLIENT_DEFAULT_SERVER_ID,
				   const char *path=GAHPCLIENT_DEFAULT_SERVER_PATH);
			/// Destructor
		~GahpClient();
		
		//@}

		///
		bool Startup();

		///
		bool Initialize(const char * proxy_path);

		///
		void purgePendingRequests() { clear_pending(); }

		///
		void setMaxPendingRequests(int max) { server->max_pending_requests = max; }

		///
		int getMaxPendingRequests() { return server->max_pending_requests; }

		/** @name Mode methods.
		 * Methods to set/get the mode.
		 */
		//@{
	
		/// Enum used by setMode() method
		enum mode {
				/** */ normal,
				/** */ results_only,
				/** */ blocking
		};

		/**
		*/
		void setMode( mode m ) { m_mode = m; }

		/**
		*/
		mode getMode() { return m_mode; }
	
		//@}

		/** @name Timeout methods.
		 * Methods to set/get the timeout on pending async commands.
		 */
		//@{
	
		/** Set the timeout.
			@param t timeout in seconds, or zero to disable timeouts.
			@see getTimeout
		*/
		void setTimeout(int t) { m_timeout = t; }

		/** Get the currently timeout value.
			@return timeout in seconds, or zero if no timeout set.
			@see setTimeout
		*/
		unsigned int getTimeout() { return m_timeout; }
	
		//@}

		/** @name Async results methods.
		   Methods to control how to fetch and/or be notified about
		   pending asynchronous results.
		 */
		//@{
		
		/** Reset the specified timer to go off immediately when a
			pending command has completed.
			@param tid The timer id to reset via DaemonCore's Reset_Timer 
			method, or a -1 to disable.
			@see Reset_Timer
			@see getNotificationTimerId
		*/
		void setNotificationTimerId(int tid) { user_timerid = tid; }

		/** Return the timer id previously set with method 
			setNotificationTimerId.
			@param tid The timer id which will be reset via DaemonCore's 
			Reset_Timer method, or a -1 if deactivated.
			@see setNotificationTimerId
			@see Reset_Timer
		*/
		int getNotificationTimerId() { return user_timerid; }

		//@}

		void setNormalProxy( Proxy *proxy );

		void setDelegProxy( Proxy *proxy );

		//-----------------------------------------------------------
		
		/**@name Globus Methods
		 * These methods have the exact same API as their native Globus
		 * Toolkit counterparts.  
		 */
		//@{

		const char *
			getGlobusGassServerUrl() { return server->globus_gass_server_url; }

		/// cache it from the gahp
		const char *
		globus_gram_client_error_string(int error_code);

		///
		int 
		globus_gram_client_callback_allow(
			globus_gram_client_callback_func_t callback_func,
			void * user_callback_arg,
			char ** callback_contact);

		///
		int 
		globus_gram_client_job_request(const char * resource_manager_contact,
			const char * description,
			const int job_state_mask,
			const char * callback_contact,
			char ** job_contact);

		///
		int 
		globus_gram_client_job_cancel(const char * job_contact);

		///
		int
		globus_gram_client_job_status(const char * job_contact,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_signal(const char * job_contact,
			globus_gram_protocol_job_signal_t signal,
			const char * signal_arg,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_callback_register(const char * job_contact,
			const int job_state_mask,
			const char * callback_contact,
			int * job_status,
			int * failure_code);

		///
		int 
		globus_gram_client_ping(const char * resource_manager_contact);

		///
		int 
		globus_gram_client_job_contact_free(char *job_contact) 
			{ free(job_contact); return 0; }


		///
		int
		globus_gram_client_set_credentials(const char *proxy_path);

		///
		int
		globus_gram_client_job_refresh_credentials(const char *job_contact);

		///
		int
		globus_gass_server_superez_init( char **gass_url, int port );


		///
		int
		gt3_gram_client_callback_allow(
			globus_gram_client_callback_func_t callback_func,
			void * user_callback_arg,
			char ** callback_contact);

		///
		int 
		gt3_gram_client_job_create(const char * resource_manager_contact,
			const char * description,
			const char * callback_contact,
			char ** job_contact);

		///
		int
		gt3_gram_client_job_start(const char *job_contact);

		///
		int 
		gt3_gram_client_job_destroy(const char * job_contact);

		///
		int
		gt3_gram_client_job_status(const char * job_contact,
			int * job_status);

		///
		int
		gt3_gram_client_job_callback_register(const char * job_contact,
			const char * callback_contact);

		///
		int 
		gt3_gram_client_ping(const char * resource_manager_contact);

		///
		int 
		gt3_gram_client_job_contact_free(char *job_contact) 
			{ free(job_contact); return 0; }

		///
		int
		gt3_gram_client_job_refresh_credentials(const char *job_contact);

#ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC
	// Not yet ready for prime time...
	globus_duroc_control_barrier_release();
	globus_duroc_control_init();
	globus_duroc_control_job_cancel();
	globus_duroc_control_job_request();
	globus_duroc_control_subjob_states();
	globus_duroc_error_get_gram_client_error();
	globus_duroc_error_string();
#endif /* ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC */

		//@}

	private:

			// Various Private Methods
		int new_reqid();
		void clear_pending();
		bool is_pending(const char *command, const char *buf);
		void now_pending(const char *command,const char *buf,
						 GahpProxyInfo *proxy = GAHPCLIENT_CACHE_DEFAULT_PROXY);
		Gahp_Args* get_pending_result(const char *,const char *);
		bool check_pending_timeout(const char *,const char *);
		int reset_user_timer(int tid);
		int reset_user_timer_alarm();

			// Private Data Members
		unsigned int m_timeout;
		mode m_mode;
		char pending_command[150];
		char *pending_args;
		int pending_reqid;
		Gahp_Args* pending_result;
		time_t pending_timeout;
		int pending_timeout_tid;
		bool pending_submitted_to_gahp;
		int user_timerid;
		GahpProxyInfo *normal_proxy;
		GahpProxyInfo *deleg_proxy;
		GahpProxyInfo *pending_proxy;

			// These data members all deal with the GAHP
			// server.  Since there is only one instance of the GAHP
			// server, all the below data members are static.
		GahpServer *server;

};	// end of class GahpClient


#endif /* ifndef CONDOR_GAHP_CLIENT_H */
