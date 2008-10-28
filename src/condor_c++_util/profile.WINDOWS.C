/***************************************************************
*
* Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
* University of Wisconsin-Madison, WI.
* 
* Licensed under the Apache License, Version 2.0 (the "License"); you
* may not use this file except in compliance with the License.  You may
* obtain a copy of the License at
* 
*    http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***************************************************************/

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "perm.h"

#include "profile.WINDOWS.h"
#include "string_conversion.WINDOWS.h"
#include "remote_close.WINDOWS.h"
#include "directory.WINDOWS.h"
#include "security.WINDOWS.h"

#include <userenv.h>    // for LoadUserProfile, etc.
#include <sddl.h>       // for ConvertSidToStringSid
#include <ntsecapi.h>   // USER_INFO_4
#include <lm.h>         // for NetUserGetInfo

/***************************************************************
* {d,c}tor
***************************************************************/

OwnerProfile::OwnerProfile () : 
    load_as_owner_ ( FALSE ),
    profile_loaded_  ( FALSE ),
    user_token_ ( NULL ),
    user_name_ ( NULL ),
    domain_name_ ( NULL ),
    profile_directory_ ( NULL ),
    profile_template_ ( NULL ),
    profile_cache_ ( NULL ),
    profile_backup_ ( NULL ) {

    ZeroMemory ( 
        &user_profile_, 
        sizeof ( PROFILEINFO ) );

}

OwnerProfile::~OwnerProfile () {

    if ( loaded () ) {
        unload ();
    }

    if ( NULL != profile_directory_ ) {
        delete [] profile_directory_;
        profile_directory_ = NULL;
    }
    if ( NULL != profile_backup_ ) {
        delete [] profile_backup_;
        profile_backup_ = NULL;
    }
    
    if ( NULL != profile_template_ ) {
        free ( profile_template_ );
        profile_template_ = NULL;
    }
    if ( NULL != profile_cache_ ) {
        free ( profile_cache_ );
        profile_cache_ = NULL;
    }

}

/***************************************************************
* Attributes
***************************************************************/

/* returns TRUE if a the current user's profile is loaded; 
otherwise, FALSE.*/
BOOL
OwnerProfile::loaded () const {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::loaded()\n" );

    return profile_loaded_;

}

/* returns the type of profile the current user has; otherwise 0.
The non-error values are:
PT_MANDATORY : The user has a mandatory user profile.
PT_ROAMING   : The user has a roaming user profile.
PT_TEMPORARY : The user has a temporary user profile; it will be 
               deleted at log off. */
DWORD 
OwnerProfile::type () const {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::type()\n" );

    DWORD profile_type = 0;

    if ( loaded () ) {
        GetProfileType ( &profile_type );
    }

    return profile_type;

}

/* returns TRUE if the profile exists; otherwise FALSE. */
BOOL
OwnerProfile::exists () const {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::exists()\n" );

    BOOL profile_exists = FALSE;

    if ( loaded () ) {
        profile_exists = ( NULL != profile_directory_ );
    }

    return profile_exists;

}

/***************************************************************
* Methods
***************************************************************/

/* returns TRUE if the internals were correctly initialized;
otherwise, FALSE. This can be called multiple times, in case 
of a reconfig. */
BOOL OwnerProfile::update () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::update()\n" );

    priv_state  priv    = PRIV_UNKNOWN;
    BOOL        ok      = TRUE;

    __try {
        
        /* do this as the user, so we get their information. */
        priv = set_user_priv ();

        ZeroMemory ( 
            &user_profile_, 
            sizeof ( PROFILEINFO ) );

        user_token_  = priv_state_get_handle ();
        user_name_   = get_user_loginname ();
        domain_name_ = get_user_domainname ();

        if ( NULL != profile_template_ ) {
            free ( profile_template_ );
            profile_template_ = NULL;
        }
        if ( NULL != profile_cache_ ) {
            free ( profile_cache_ );
            profile_cache_ = NULL;
        }

        /* we always assume there is are fresh directorys in the 
        configuration file(s) */
        profile_template_ = param ( PARAM_PROFILE_TEMPLATE );
        profile_cache_    = param ( PARAM_PROFILE_CACHE );

    }
    __finally {

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}


/* returns TRUE if a user profile was created; otherwise, FALSE.
NOTE: We do not call load() here as we call create() from there,
which would be a rather silly loop to be caught it. */
BOOL
OwnerProfile::create () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::create()\n" );

    priv_state  priv                = PRIV_UNKNOWN;
    int         length              = 0;
    BOOL        profile_loaded      = FALSE,
                profile_unloaded    = FALSE,
                profile_deleted     = FALSE,
                ok                  = FALSE;    

    __try {

        /* Do the following as condor, since we can't do it as the 
        user, as we are creating the profile for the first time, and 
        we need administrative rights to do this (which, presumably,
        the "owner" of this profile does not have) */
        priv = set_condor_priv ();

        /* Creating a profile is quite straight forward: simply try to 
        load it. Windows will realize that there isn't one stashed away
        for the user, so it will create one for us.  We can then simply
        unload it, since we will be making a copy of the unmodified
        version of it up at a later point, so that jobs will always
        run with a clean profile--thereby eliminating any possible
        cross job security issues (i.e. writting missleading data to 
        well known registry entries, etc.) */

        /* load the user's profile for the first time-- this will 
        effectively create it "for free", using the local "default"
        account as a template (for various versions of "default", 
        each Windows flavour has it's own naming scheme). */
        profile_loaded = loadProfile ();

        if ( !profile_loaded ) {            
            __leave;
        }

        /* now unload the newly created profile. */
        profile_unloaded = unloadProfile ();
      
        if ( !profile_unloaded ) {            
            __leave;
        }

        /* retrieve the profile's directory */
        profile_directory_ = directory ();

        if ( !profile_directory_ ) {            
            __leave;
        }

        /* if we're here, then the profile it ready to be used */
        ok = TRUE;

    }
    __finally {

        if ( !ok && ( profile_loaded && !profile_unloaded ) ) {
            unloadProfile ();
        }

        if ( !ok && profile_directory_ ) {
            delete [] profile_directory_;
            profile_directory_ = NULL;
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/* returns TRUE if a user profile was destroyed; otherwise, FALSE. 
It's private because it does not change the state of the profile 
object, so even though the profile object may think it is loaded,
in reality, it's completely gone. This is directly remedied by 
the code surrounding it, at its invocation. */
BOOL
OwnerProfile::destroy () const {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::destroy()\n" );

    priv_state  priv            = PRIV_UNKNOWN;
    DWORD       last_error      = ERROR_SUCCESS;
    PSID        user_sid        = NULL;
    LPSTR       user_sid_string = NULL;
    BOOL        got_user_sid    = FALSE,
                got_sid_string  = FALSE,
                profile_deleted = FALSE,
                ok              = FALSE;

    __try {

        /* we must do the following as condor */
        priv = set_condor_priv ();

        /* load the user's SID */
        got_user_sid = LoadUserSid ( 
            user_token_, 
            &user_sid );

        dprintf ( 
            D_FULLDEBUG, 
            "UserProfile::destroy: Loading %s's SID "
            "%s. (last-error = %u)\n", 
            user_name_,
            got_user_sid ? "succeeded" : "failed", 
            got_user_sid ? 0 : GetLastError () );

        if ( !got_user_sid ) {
            __leave;
        }

        /* convert the SID to a string */
        got_sid_string = ConvertSidToStringSid (
            user_sid,
            &user_sid_string );

        dprintf ( 
            D_FULLDEBUG, 
            "UserProfile::destroy: Converting SID to a string "
            "%s. (last-error = %u)\n", 
            got_sid_string ? "succeeded" : "failed", 
            got_sid_string ? 0 : GetLastError () );
        
        if ( !got_sid_string ) {
            __leave;
        }

        /* let Windows remove the profile for us */
        profile_deleted = DeleteProfile ( 
            user_sid_string,
            profile_directory_,
            NULL /* local computer */ );

        dprintf ( 
            D_FULLDEBUG, 
            "UserProfile::destroy: Removing %s's profile "
            "directory %s. (last-error = %u)\n", 
            user_name_,
            profile_deleted ? "succeeded" : "failed", 
            profile_deleted ? 0 : GetLastError () );
        
        if ( !profile_deleted ) {
            __leave;
        }

#if 0
        /* just make sure we have the profile's directory */
        if ( NULL == profile_directory_ ) {
            __leave;
        }

        /* if we have have a profile directory, let's blow it away */
        profile_deleted = 
            CondorRemoveDirectory ( profile_directory_ );

        dprintf ( 
            D_FULLDEBUG, 
            "UserProfile::destroy: Removing %s's profile "
            "directory %s. (last-error = %u)\n", 
            user_name_,
            profile_deleted ? "succeeded" : "failed", 
            profile_deleted ? 0 : GetLastError () );

        if ( !profile_deleted ) {
            __leave;
        }
#endif

        /* if we got here, all is well */
        ok = TRUE;
    
    }
    __finally {

        /* return to previous privilege level */
        set_priv ( priv );

        if ( user_sid ) {
            UnloadUserSid ( user_sid );
        }
        if ( user_sid_string ) {
            LocalFree ( user_sid_string );
        }

    }

    return ok;

}


/* returns TRUE if a user profile was loaded; otherwise, FALSE.*/
BOOL
OwnerProfile::load ( BOOL as_owner ) {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::load(%s profile)\n",
        as_owner ? "owner" : "dynamic" );

    BOOL ok = FALSE;
    
    __try {

        /* short-cut if we've already loaded the profile */
        if ( loaded () ) {
            ok = TRUE;
            __leave;
        }

        /* set the state of our current dilemma: are we creating a
        bunk profile and destroying it, or are we trying are best 
        not to damage anything the actual user might hold dear. */
        load_as_owner_ = as_owner;
        
        /* now, decide which way we should load this account */
        if ( load_as_owner_ ) {

            /* load as the owner with great concern for their 
            data and it's consistency and integrity */
            ok = loadRealOwner ();

        } else {

            /* load as a dynamic user, in which case we hold no
            regard for the state of the incomming or resulting 
            data. we'll just be blowwing it away anyway. */
            ok = loadGeneratedOwner ();
        }

    }
    __finally {
        /* no-op */
    }

    return ok;

}

/* returns TRUE if a real user's profile was loaded; otherwise, 
    FALSE.  Here it is important always fail gracefully, such that 
    the user loging-in would never know that we've been here.*/
BOOL 
OwnerProfile::loadRealOwner () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::loadRealOwner()\n" );

    priv_state		priv;
    DWORD           last_error          = ERROR_SUCCESS;
    PCHAR           message             = NULL;
    PWCHAR          w_user_name		    = NULL,
					w_domain_name	    = NULL;
    USER_INFO_4	    user_info		        = NULL;
	NET_API_STATUS	got_info	        = NERR_Success;
    BOOL            disabled            = FALSE,
                    ok                  = FALSE;
    
    __try {

        /* we must do the following as Condor */
        priv = set_condor_priv ();

        /* we need Unicode versions of these strings for 
        NetUserGetInfo */
        w_user_name	  = ProduceWFromA ( user_name_ ),
        w_domain_name = ProduceWFromA ( domain_name_ );
        ASSERT ( w_user_name );
		ASSERT ( w_domain_name );
        
        /* if the user has a roaming profile for this user, then we 
        need to get the remote path, as it may not yet be cached 
        here */
		got_info = NetUserGetInfo ( 
            w_domain_name, 
            w_user_name, 
			4, /* magic for plase fill user_info_4, thanks */
            (PBYTE*) &user_info );

        /* there are a few non-standard errors that may come from 
        the above command, so we just bust out a horrid and somewhat
        superfluous switch statement and let it do the writting for 
        us. */
        switch ( got_info ) {
        case NERR_Success:
            message = "loaded network user information";
            break;
        case ERROR_ACCESS_DENIED:
            message = "The user does not have access to the "
                      "requested information";
            break;
        case NERR_InvalidComputer:
            message = "The computer name is invalid";
            break;
        case NERR_UserNotFound:
            message = "The user name could not be found";
            break;
        default:
            message = "Unknown error %d"
            break;
        }

        if ( NERR_Success != got_info ) {
            
            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::loadRealOwner(): "
                "%s.\n",
                message,
                got_info );

            user_info = NULL;            
            __leave;

        }
        
        /* now--just for kicks--lets make sure the account is actually 
        enabled... just in case, or for safety sake, or well, for 
        the security colour of the month (what colour will it be next?
        Or maybe by the time someone else sees this code the 
        delineation of threat levels will have a fancy new names... 
        live mauve, or something.  I don't get it, am I supposed to
        feel a little orange?  I know how blue feels, but is orange
        like like and almost uncomfortable red, like a little too hot,
        but nothing to whine about?) */
        disabled = user_info->usri4_flags & UF_ACCOUNTDISABLE;

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::loadRealOwner(): "
            "%s@%s account it is disabled\n",
            user_name_, 
            domain_name_,
            account_disabled ? "disabled!?!" : "enabled" );
        
        if ( account_disabled ) {
            __leave;
		}

        /* There will aways a non-NULL home directory entry, but it
        may be of zero length. If it is of zero length, it means this
        is not a roaming profile.  Why is this not a set as a FLAG 
        SOMEWHERE... I don't have a clue. */			
		if ( wcslen ( user_info->usri4_home_dir ) <= 0 ) {

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::loadRealOwner(): "
                "%s@%s account has no home directory defined.\n",
                user_name_, 
                domain_name_ );
            
            __leave;

        }
			
		dprintf ( D_ALWAYS, 
            "OwnerProfile::loadRealOwner(): "
            "This user has a roaming profile. You may experience "
            "a delay in execution, if the user has a large registry
            hive.\n" );
			
			*home_directory = ProduceAFromW ( user_info->usri4_home_dir );
			ASSERT ( *home_directory );

			if ( NULL == *home_directory ) {
				dprintf ( D_MALLOC, "CondorGetNetUserHomeDirectory: failed to allocate "
									"memory for home directory string?!" );
				ok = FALSE;
				__leave;
			}
			
			/* print the listed home directory... */			
			dprintf ( D_FULLDEBUG, "CondorGetNetUserHomeDirectory: network home directory: "
				"'%s'\n", *home_directory );

		

    }
    __finally {
        
        if ( user_info ) {
            NetApiBufferFree ( user_info );
        }
        if ( w_user_name ) {
            delete [] w_user_name;
        }
        if ( w_domain_name ) {
            delete [] w_domain_name;
        }
        
        /* return to previous privilege level */
		set_priv ( priv );

    }

    return ok;

}

/* returns TRUE if a user profile was loaded; otherwise, FALSE.*/
BOOL
OwnerProfile::loadGeneratedOwner () {
    
    dprintf ( D_FULLDEBUG, "In OwnerProfile::loadGeneratedOwner()\n" );

    HANDLE          have_access         = INVALID_HANDLE_VALUE;
    DWORD           last_error          = ERROR_SUCCESS;
                    length              = 0,
                    i                   = 0;
    priv_state      priv                = PRIV_UNKNOWN;
    BOOL            backup_created      = FALSE,
                    profile_loaded      = FALSE,
                    profile_exists      = FALSE,
                    profile_destroyed   = FALSE,
                    ok                  = FALSE;

    __try {

        /* we must do the following as Condor */
        priv = set_condor_priv ();

        /* get the user's local profile directory (if this user 
        has a roaming profile, this is when i' cached locally) */
        profile_directory_ = directory ();

        /* if we have have a profile directory, let's make sure that 
        we also have permissions to it. Sometimes, if the startd were
        to crash, heaven forbid, we may have access to the profile 
        directory, but it may still be locked by the previous login 
        session that was not cleaned up properly (the only resource
        I know of that the system does not clean up immediately on 
        process termination are user login handles and their 
        resources). */
        if ( profile_directory_ ) {

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::load: %s's profile directory: '%s'. "
                "(last-error = %u)\n",
                user_name_,
                profile_directory_, 
                GetLastError () );
            
            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::load: A profile directory is listed "
                "but may not exist.\n" );

            have_access = CreateFile ( 
                profile_directory_, 
                GENERIC_WRITE, 
                0,                          /* magic # for NOT shared */
                NULL, 
                OPEN_EXISTING, 
                FILE_FLAG_BACKUP_SEMANTICS, /* only take a peek */
                NULL );

            if ( INVALID_HANDLE_VALUE == have_access ) {

                last_error = GetLastError ();

                dprintf ( 
                    D_FULLDEBUG, 
                    "OwnerProfile::load: Failed to access '%s'. "
                    "(last-error = %u)\n",
                    profile_directory_,
                    last_error );

                if (   ERROR_ACCESS_DENIED     == last_error 
                    || ERROR_SHARING_VIOLATION == last_error ) {

                    /**************************************************
                    NOTE: For future implementations which allow for
                    any user to load their profile, what follows
                    bellow is known as a BAD IDEA. We'd prefer to keep
                    all the data, or FAIL! :)
                    **************************************************/

                    /* so we don't have access, lets just blow it away
                    and create a new one (see bellow) */
                    profile_destroyed = destroy ();

                    dprintf ( 
                        D_FULLDEBUG, 
                        "OwnerProfile::load: Destruction of %s's "
                        "profile %s. (last-error = %u)\n",
                        user_name_,
                        profile_destroyed ? "succeeded" : "failed", 
                        profile_destroyed ? 0 : GetLastError () );

                    if ( !profile_destroyed ) {
                        __leave;
                    }

                }

            }

            /* if we're here, then we can access the profile */
            profile_exists = TRUE;

        }

        /* explicitly create the profile */
        if ( !profile_exists ) {

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::load: Profile directory does not "
                "exist, so we're going to create one.\n" );

            /* we now create the profile, so we can backup it 
            up directly */
            profile_exists = create ();

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::load: Creation of profile for %s %s. "
                "(last-error = %u)\n",
                user_name_,
                profile_exists ? "succeeded" : "failed", 
                profile_exists ? 0 : GetLastError () );

            /* if the profile still does not exist, then bail */
            if ( !profile_exists ) {
                __leave;
            }

        } 

#if 0
        /* now we transfer the user's profile directory to the cache 
        so that we can revert back to it once the user is done doing 
        their thang. */
        backup_created = backup ();

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::load: Creating a backup of %s's "
            "profile %s.\n",
            user_name_,
            backup_created ? "succeeded" : "failed" );

        /* if we were unable to create the backup, we should bail out
        before we allow the user to make changes to the template 
        profile */
        if ( !backup_created ) {
            __leave;
        }
#endif

        /* finally, load the user's profile */
        profile_loaded = loadProfile ();

        if ( !profile_loaded ) {            
            __leave;
        }
        
        /* make sure to change state with regards to being loaded */
        profile_loaded_ = TRUE;

        /* everything went as expected */        
        ok = TRUE;

    }
    __finally {

        /* free the attributes, if required */
        if ( !ok && profile_directory_ ) {
            delete [] profile_directory_;
            profile_directory_ = NULL;
        }
        
        /* if we loaded the profile, but failed for some other reason,
        then we should make sure to unload the profile */            
        if ( !ok && profile_loaded ) {
            unloadProfile ();            
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;
    
}

/* returns TRUE if a user profile was unloaded; otherwise, FALSE.*/
BOOL
OwnerProfile::unload () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::unload()\n" );

    priv_state  priv                = PRIV_UNKNOWN;
    BOOL        profile_unloaded    = FALSE,
                backup_restored     = FALSE,
                ok                  = FALSE;

    __try {

        /* short-cut if we've already unloaded the profile */
        if ( !loaded () ) {
            ok = TRUE;
            __leave;
        }

        /* we must do the following as Condor */
        priv = set_condor_priv ();

        /* Unload the profile */
        profile_unloaded = unloadProfile ();

        if ( !profile_unloaded ) {            
            __leave;
        }

        /* make sure to change state with regards to being unloaded,
        as we cannot _restore_ the original while the profile is 
        loaded */
        profile_loaded_ = FALSE;

#if 0
        /* Now we have unloaded user's profile we can restore the
        original cached version */
        backup_restored = restore ();

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::unload: Restoration of %s's "
            "profile %s.\n",
            user_name_,
            backup_restored ? "succeeded" : "failed" );

        /* if we were unable to create the backup, we should bail out
        before we allow the user to make changes to the template 
        profile */
        if ( !backup_restored ) {
            __leave;
        }
#endif

        /* if we got here, then everything has been reverted */
        ok = TRUE;

    }   
    __finally {

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/* returns TRUE if a user's environment was loaded; otherwise, FALSE.*/
BOOL
OwnerProfile::environment ( Env &env ) {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::environment()\n" );

    priv_state  priv    = PRIV_UNKNOWN;
    PVOID       penv    = NULL;
    PWSTR       w_penv  = NULL,
                w_tmp   = NULL;
    PSTR        tmp     = NULL;    
    BOOL        created = FALSE,
                ok      = FALSE;

    __try {

        /* we must do the following as the user or Condor */
        priv = set_condor_priv ();

        /* if we are loading the user's profile, then overwrite 
        any existing environment variables with the values in the 
        user's profile (don't inherit anything from the global
        environment, as we will already have that at when we are 
        called) */
        created = CreateEnvironmentBlock ( 
            &penv, 
            user_token_, 
            FALSE ); /* we already have the current process env */
        ASSERT ( penv );

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::environment: Loading %s while retrieving "
            "%s's environment (last-error = %u)\n",
            created ? "succeeded" : "failed", 
            user_name_,
            GetLastError () );

        if ( !created ) {
            __leave;
        }

        /* fill the environment with the user's environment values */
        dprintf ( D_FULLDEBUG, "Environment:\n" );
        w_penv = (PWSTR)penv;
        while ( '\0' != *w_penv ) { /* read: while not "\0\0' */
            tmp = ProduceAFromW ( w_penv );
            if ( tmp && strlen ( tmp ) > 0 ) {
                dprintf ( D_FULLDEBUG, "%s\n", tmp );
                env.SetEnv ( tmp );
                delete [] tmp;
            }
            w_penv += wcslen ( w_penv ) + 1;
        }

        /* if we've arrived here, then all it well */
        ok = TRUE;

    }
    __finally {

        /* rid ourselves of the user's environment information */
        if ( penv ) {
            if ( !DestroyEnvironmentBlock ( penv ) ) {
                dprintf ( 
                    D_ALWAYS, 
                    "OwnerProfile::environment: "
                    "DestroyEnvironmentBlock() failed "
                    "(last-error = %u)\n", 
                    GetLastError () );
            }
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/* if it exits, this function returns the path to the root directory 
of the user's profile; otherwise, NULL. 
NOTE: Remember to delete the return value (using the delete [] form).*/
PSTR
OwnerProfile::directory () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::directory()\n" );

    priv_state  priv    = PRIV_UNKNOWN;
    DWORD       size    = MAX_PATH;
    PSTR        buffer  = NULL;
    BOOL	    ok	    = FALSE;

    __try {

        /* if we've already retrieved the profile's directory, then 
        shortcut this operation by returning the one we have stashed
        away */
        if ( profile_directory_ ) {
            buffer = profile_directory_;
            ok = TRUE;
            __leave;
        }

        /* we must do the following as the user or Condor */
        priv = set_condor_priv ();

        /* if we are here, then either we a first-time visitor, or 
        previous calls--heaven forbid--have failed so we will need
        to try and get the user's profile directory */
        buffer = new CHAR[size]; 
        ASSERT ( buffer );

        if ( !GetUserProfileDirectory ( 
            user_token_, 
            buffer,
            &size ) ) {

            /* since we only allocated MAX_PATH CHARs, we may fail 
            with at least ERROR_INSUFFICIENT_BUFFER, so we catch it 
            and allocate the buffer size we were given by 
            GetUserProfileDirectory() */
            if ( ERROR_INSUFFICIENT_BUFFER == GetLastError () ) {

                delete [] buffer; /* kill the old buffer */
                buffer = new CHAR[size];
                ASSERT ( buffer );

                if ( !GetUserProfileDirectory ( 
                    user_token_, 
                    buffer,
                    &size ) ) {
                        
                        dprintf ( 
                            D_FULLDEBUG, 
                            "OwnerProfile::directory: could not get "
                            "profile directory. (last-error = %u)\n",
                            GetLastError () );
                        
                        __leave;

                }

            } else {

                /* print the fact the user has no home buffer */
                dprintf ( 
                    D_FULLDEBUG, 
                    "OwnerProfile::directory: this user has no "
                    "profile directory.\n" );				
                
                __leave;

            }

        }

        /* if we made it this far, then we're rocking */
        ok = TRUE;

    }
    __finally {

        if ( !ok ) {
            delete [] buffer;
            buffer = NULL;
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return buffer;

}

/* returns TRUE if the user profile template was backup-ed up; 
otherwise, FALSE.*/
BOOL
OwnerProfile::backup () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::backup()\n" );

    priv_state  priv            = PRIV_UNKNOWN;
    int         length          = 0;
    BOOL        backup_created  = FALSE,
                ok              = FALSE;

    __try {

        /* can't backup while in use, we'd get tons of access denied 
        errors, as a number of core files will be locked */
        if ( loaded () ) {

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::backup: Cannot backup the profile "
                "while it is in use.\n");

            __leave;

        }

        /* we can do the following as the Condor because our copy 
        mechanism is designed to preserve the directory's ACLs */
        priv = set_user_priv ();

        /* create a backup directory name based on the profile 
        directory (i.e. profile_cache_), user's login name and 
        the */ 
        length = strlen ( profile_cache_ ) 
            + strlen ( user_name_ ) + 1
            + 20; /* +1 for \ +20 for pid */
        profile_backup_ = new CHAR[length + 1];
        ASSERT ( profile_backup_ );
        
        sprintf ( 
            profile_backup_, 
            "%s\\%s-%d", 
            profile_cache_, 
            user_name_,
            GetCurrentProcessId () );

        /* finally, copy the user's profile to the back-up directory */
        backup_created = CondorCopyDirectory ( 
            profile_directory_, 
            profile_backup_ );

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::backup: Copying '%s' to '%s' %s. "
            "(last-error = %u)\n", 
            profile_directory_,
            profile_backup_,
            backup_created ? "succeeded" : "failed", 
            backup_created ? 0 : GetLastError () );

        if ( !backup_created ) {
            __leave;
        }

        /* if we've arrived here, then all it well */
        ok = TRUE;

    }
    __finally {

        /* return to previous privilege level */
        if ( PRIV_UNKNOWN != priv ) {
            set_priv ( priv );
        }

    }

    return ok;

}

/* returns TRUE if the user profile directory was restored; 
otherwise, FALSE.*/
BOOL
OwnerProfile::restore () {
    
    dprintf ( D_FULLDEBUG, "In OwnerProfile::restore()\n" );

    priv_state  priv            = PRIV_UNKNOWN;
    int         length          = 0;
    HANDLE      directory       = NULL;
    BOOL        profile_deleted = FALSE,
                backup_restored = FALSE,
                backup_deleted  = FALSE,
                ok              = FALSE;

    __try {

        /* can't restore while the profile is loaded */
        if ( loaded () ) {

            dprintf ( 
                D_FULLDEBUG, 
                "OwnerProfile::restore: Cannot restore the profile "
                "while it is in use.\n");

            __leave;

        }

        /* we can do the following as the Condor because our copy 
        mechanism is designed to preserve the directory's ACLs */
        priv = set_user_priv ();

        /* use the directory created by the backup() call to 
        roll-back the changes made during the job execution */        
        profile_deleted = 
            CondorRemoveDirectory ( profile_directory_ );
        
        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::restore: Deleting the "
            "modified profile %s. (last-error = %u)\n", 
            profile_deleted ? "succeeded" : "failed", 
            profile_deleted ? 0 : GetLastError () );

        if ( !profile_deleted ) {
            __leave;
        }

        /* having removed the modified profile directory, 
        restore the back-up we made of the profile template */
        backup_restored = CondorCopyDirectory ( 
            profile_backup_,
            profile_directory_ );

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::restore: Deleting the "
            "profile backup %s. (last-error = %u)\n", 
            backup_restored ? "succeeded" : "failed", 
            backup_restored ? 0 : GetLastError () );

        if ( !backup_restored ) {
            __leave;
        }

        /* finally, remove the back-up directory: this ensures
        that each new job receives a fresh copy of the template */        
        backup_deleted = 
            CondorRemoveDirectory ( profile_backup_ );

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::restore: Deleting the "
            "back-up directory %s. (last-error = %u)\n", 
            backup_deleted ? "succeeded" : "failed", 
            backup_deleted ? 0 : GetLastError () );

        if ( !backup_deleted ) {
            __leave;
        }

        /* if we've arrived here, then all it well */
        ok = TRUE;

    }
    __finally {

        /* return to previous privilege level */
        if ( PRIV_UNKNOWN != priv ) {
            set_priv ( priv );
        }

        /* only if we were successful can we delete the 
        name this session's of profile backup directory */
        if ( ok ) { 
            delete [] profile_backup_;
        }

    }

    return ok;

}

/* returns TRUE if the user's real profile was locked; 
otherwise, FALSE. */    
BOOL 
OwnerProfile::lock () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::lock()\n" );

    priv_state  priv                = PRIV_UNKNOWN;
    int         length              = 0;
    BOOL        profile_locked      = FALSE,
                profile_unloaded    = FALSE,
                profile_deleted     = FALSE,
                ok                  = FALSE;    

    __try {

        /* Do the following as condor, since we can't do it as the 
        user, as we are creating the profile for the *second* time, 
        and we need administrative rights to do this (which, 
        presumably, the "owner" of this profile does not have) */
        priv = set_condor_priv ();

        /* As discused before, creating a profile is quite simple, so
        will leave those details for those curious enough to look 
        into the create() function's body. Our purpose here is 
        different.  We are creating a *second* profile for an existing
        user by locking their current profile, which will force 
        Windows to create a fresh one with a modified name. */

        /* open the user's actual profile directory and disable 
        sharing, so any firther attempts to open it will 
        inextricably fail */
        /* retrieve the profile's directory */
        profile_directory_ = directory ();
        
        profile_locked = CreateFile (
            )
        

    }
    __finally {

        /* return to previous privilege level */
        set_priv ( priv );

    }

}

/* returns TRUE if the user's real profile was unlocked; 
otherwise, FALSE. */
BOOL 
OwnerProfile::unlock () {
    
}

/***************************************************************
* Helper Methods
***************************************************************/

/* returns TRUE if the user profile is loaded; otherwise, 
FALSE.  This is a simple helper that does all the initialization
required to do the loading of a profile. */
BOOL 
OwnerProfile::loadProfile ( CHAR *profile_path ) {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::loadProfile()\n" );

    priv_state  priv;
    BOOL        profile_loaded = FALSE,
                ok             = FALSE;

    __try {

        /* we must do the following as Condor, because, presumably,
        the account we are running as, does not have admin rights*/
        priv = set_condor_priv ();
        
        /* initialize profile information */
        ZeroMemory ( 
            &user_profile_, 
            sizeof ( PROFILEINFO ) );
        user_profile_.dwSize = sizeof ( PROFILEINFO );

        /* The PI_NOUI flag avoids the "I'm going to make a temporary 
        profile for you... blah, blah, blah" dialog from being 
        displayed in the case when a roaming profile is inaccessible 
        because of a network outage, among other reasons. */
        user_profile_.dwFlags       = PI_NOUI;
        user_profile_.lpUserName    = (PCHAR) user_name_;
        user_profile_.lpProfilePath = (PCHAR) profile_path;
        
        /* now, finally, load the profile */
        profile_loaded = LoadUserProfile ( 
            user_token_, 
            &user_profile_ );

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::loadProfile: Loading the %s's "
            "profile %s. (last-error = %u)\n",
            user_profile_.lpUserName,
            profile_loaded ? "succeeded" : "failed", 
            profile_loaded ? 0 : GetLastError () );

        if ( !profile_loaded ) {
            __leave;
        }

        /* if we get here then we've managed to create the profile */
        ok = TRUE;

    }  
    __finally { 
    
        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/* returns TRUE if the user profile was unloaded; otherwise, FALSE.
Also ensures the profile information is cleaned out. */
BOOL
OwnerProfile::unloadProfile () {

    dprintf ( D_FULLDEBUG, "In OwnerProfile::unloadProfile()\n" );

    priv_state  priv;
    BOOL        profile_unloaded = FALSE,
                ok               = FALSE;

    __try {

        /* we must do the following as  */
        priv = set_condor_priv ();
        
        /* unload the current profile */
        profile_unloaded = UnloadUserProfile ( 
            user_token_, 
            user_profile_.hProfile ); 

        dprintf ( 
            D_FULLDEBUG, 
            "OwnerProfile::unloadProfile: Unloading %s's "
            "profile %s. (last-error = %u)\n",
            user_name_,
            profile_unloaded ? "succeeded" : "failed", 
            profile_unloaded ? 0 : GetLastError () );

        if ( !profile_unloaded ) {
            __leave;
        }

        /* if we got here then all is well in the universe */
        ok = TRUE;

    }
    __finally {

        /* we use SecureZeroMemory() here because it has a very 
        desirable property: it will never be optimized away, as its 
        cousin ZeroMemory() might be.  This is of great interest to 
        us, as the state of the profile structure greatly influences
        the behaviour of this class. */
        if ( ok ) {            
            SecureZeroMemory ( 
                &user_profile_, 
                sizeof ( PROFILEINFO ) );
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

