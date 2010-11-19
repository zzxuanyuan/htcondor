/***************************************************************
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_parser.h"

#include "stringSpace.h"

#include "Globals.h"
// from src/management
#include "Utils.h"

#include <sstream>

// Any key that begins with the '0' char is either the
// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])

// summary attributes
 const char *ATTRS[] = {ATTR_CLUSTER_ID,
                           ATTR_PROC_ID,
                           ATTR_GLOBAL_JOB_ID,
                           ATTR_Q_DATE,
                           ATTR_ENTERED_CURRENT_STATUS,
                           ATTR_JOB_STATUS,
                           ATTR_JOB_CMD,
                           ATTR_JOB_ARGUMENTS1,
                           ATTR_JOB_ARGUMENTS2,
                           ATTR_RELEASE_REASON,
                           ATTR_HOLD_REASON,
                           NULL
                          };

// TODO: C++ utils which may very well exist elsewhere :-)
template <class T>
bool from_string ( T& t,
                   const std::string& s,
                   std::ios_base& ( *f ) ( std::ios_base& ) )
{
    std::istringstream iss ( s );
    return ! ( iss >> f >> t ).fail();
}

template <class T>
std::string to_string ( T t, std::ios_base & ( *f ) ( std::ios_base& ) )
{
    std::ostringstream oss;
    oss << f << t;
    return oss.str();
}

extern ManagementAgent::Singleton *singleton; // XXX: get rid of this
extern com::redhat::grid::JobServerObject* job_server;

Attribute::Attribute ( AttributeType _type, const char *_value ) :
        m_type ( _type ),
        m_value ( _value )
{ }

Attribute::~Attribute()
{
}

Attribute::AttributeType
Attribute::GetType() const
{
    return m_type;
}

const char *
Attribute::GetValue() const
{
    return m_value;
}

// Job
Job::Job ( const char *_key ) : m_submission( NULL ),  m_key ( _key )
{
}

Job::~Job()
{
    if (m_key) {
	delete m_key;
	m_key = NULL;
    }
}

const char *
Job::GetKey() const
{
    return m_key;
}

void
Job::IncrementSubmission()
{
    m_submission->Increment ( this );
}

void
Job::DecrementSubmission()
{
    m_submission->Decrement ( this );
}

void Job::Set ( const char *_name, const char *_value ) {
	dprintf ( D_FULLDEBUG, "Job::Set key='%s', name='%s', value='%s'\n", GetKey(), _name, _value);
}

void Job::Remove ( const char *_name ) {
	dprintf ( D_FULLDEBUG, "Job::Remove key='%s', name='%s'\n", GetKey(), _name);
}

// LiveJob
LiveJob::LiveJob ( const char *_key,
                   const LiveJob* _parent ) :
        Job ( _key )
{
    m_ad = new ClassAd();
    if ( _parent )
    {
        m_ad->ChainToAd ( _parent->m_ad );
	dprintf ( D_FULLDEBUG, "LiveJob created: key '%s' and parent key '%s'\n",_key,  _parent->GetKey() );
    }
}

LiveJob::~LiveJob()
{

  dprintf ( D_FULLDEBUG, "LiveJob destroyed: key '%s'\n", GetKey());

    // do this before releasing the ad
    this->DecrementSubmission ();

    // TODO: do we have to unchain our parent?
    if (m_ad) {
	m_ad->unchain();
	delete m_ad;
	m_ad = NULL;
    }
}

void
Job::SetSubmission ( const char* _subName, int cluster )
{
    const char* owner = NULL;

    // need to see if someone has left us an owner
    OwnerlessClusterType::const_iterator it = g_ownerless.find ( cluster );
    if ( g_ownerless.end() == it )
    {
        dprintf ( D_FULLDEBUG, "warning: unable to resolve owner for Job key '%s' and cluster '%d'\n", GetKey(), cluster );
    }
    else {
        owner = ( *it ).second ;
    }

    SubmissionCollectionType::const_iterator element = g_submissions.find ( _subName );
    SubmissionObject *submission;
    if ( g_submissions.end() == element )
    {
        submission = new SubmissionObject ( singleton->getInstance(), job_server, _subName, owner );
        g_submissions[strdup ( _subName ) ] = submission;
    }
    else
    {
        submission = ( *element ).second;
    }
    m_submission = submission;

    this->IncrementSubmission ();

	if (owner) {
		// ensure that the submission has an owner
		m_submission->setOwner ( owner );
		g_ownerless.erase ( cluster );
	}

}

// TODO: this code doesn't work as expected
// everything seems to get set to EXPR_TYPE
bool
LiveJob::Get ( const char *_name, const Attribute *&_attribute ) const
{
    // our job ad is chained so lookups will
    // encompass our parent ad as well as the child

    // parse the type
    ExprTree *expr = NULL;
    if ( ! ( expr = m_ad->Lookup ( _name ) ) )
    {
        dprintf ( D_FULLDEBUG,
                  "warning: failed to lookup attribute %s in job '%s'\n", _name, GetKey() );
        return false;
    }
    // decode the type
    switch ( expr->RArg()->MyType() )
    {
        case LX_INTEGER:
        {
            int i;
            if ( !m_ad->LookupInteger ( _name, i ) )
            {
                return false;
            }
            _attribute = new Attribute ( Attribute::INTEGER_TYPE, to_string<int> ( i,std::dec ).c_str() );
            return true;
        }
        case LX_FLOAT:
        {
            float f;
            if ( !m_ad->LookupFloat ( _name, f ) )
            {
                return false;
            }
            _attribute = new Attribute ( Attribute::FLOAT_TYPE, to_string<float> ( f,std::dec ).c_str() );
            return true;
        }
        case LX_STRING:
        {
            MyString str;
            if ( !m_ad->LookupString ( _name, str ) )
            {
                return false;
            }
            _attribute = new Attribute ( Attribute::STRING_TYPE, str.StrDup() );
            return true;
        }
        default:
        {
            ExprTree* tree = NULL;
            if ( ! ( tree = m_ad->Lookup ( _name ) ) )
            {
                return false;
            }
            char* rhs;
            tree->RArg()->PrintToNewStr ( &rhs );
            _attribute = new Attribute ( Attribute::EXPR_TYPE, rhs );
            return true;
        }
    }

    return false;
}

int LiveJob::GetStatus() const
{
    const Attribute* attr;

    if ( !this->Get ( ATTR_JOB_STATUS, attr ) )
    {
		// TODO: assume we might get cluster jobs here also
		return UNEXPANDED;
    }

    return strtol ( attr->GetValue(), ( char ** ) NULL, 10 );
}

void
LiveJob::Set ( const char *_name, const char *_value )
{

    if ( strcasecmp ( _name, ATTR_JOB_SUBMISSION ) == 0 )
    {
        std::string val = TrimQuotes( _value );
        // TODO: grab the cluster from our key
        PROC_ID id = getProcByString(GetKey());
        SetSubmission ( val.c_str(), id.cluster );
    }

    // our status is changing...decrement for old one
    if ( strcasecmp ( _name, ATTR_JOB_STATUS ) == 0 )
    {
        if ( m_submission )
        {
            this->DecrementSubmission ();
        }
    }

    if ( strcasecmp ( _name, ATTR_OWNER ) == 0 )
    {
		// need to leave an owner for this job
		// to be picked up soon
		// if we are in here, we don't have m_submission
		PROC_ID id = getProcByString(GetKey());
		std::string val = TrimQuotes( _value );
		g_ownerless[id.cluster] = strdup( val.c_str() );
    }

    // parse the type
    ExprTree *expr;
    if ( ParseClassAdRvalExpr ( _value, expr ) )
    {
        dprintf ( D_ALWAYS,
                  "error: parsing %s[%s] = %s, skipping\n",
                  GetKey(), _name, _value );
        return;
    }
    // add this value to the classad
    switch ( expr->MyType() )
    {
        case LX_INTEGER:
            int i;
            from_string<int> ( i, std::string ( _value ), std::dec );
            m_ad->Assign ( _name, i );
            break;
        case LX_FLOAT:
            float f;
            from_string<float> ( f, std::string ( _value ), std::dec );
            m_ad->Assign ( _name, f );
            break;
        case LX_STRING:
            m_ad->Assign ( _name, _value );
            break;
        default:
            m_ad->AssignExpr ( _name, _value );
            break;
    }
    delete expr; expr = NULL;

    // our status has changed...increment for new one
    if ( strcasecmp ( _name, ATTR_JOB_STATUS ) == 0 )
    {
        if ( m_submission )
        {
            this->IncrementSubmission ();
        }
    }
}

void
LiveJob::Remove ( const char *_name )
{
	// TODO: seems we implode if we don't unchain first
	ChainedPair cp = m_ad->unchain();
	m_ad->Delete ( _name );
	m_ad->RestoreChain(cp);
}

bool LiveJob::GetSummary ( ClassAd& _classAd ) const
{
	_classAd.ResetExpr();
	int i = 0;
	while (NULL != ATTRS[i]) {
		const Attribute* attr = NULL;
		if (this->Get(ATTRS[i],attr)) {
			switch (attr->GetType()) {
				case Attribute::FLOAT_TYPE:
					_classAd.Assign(ATTRS[i], atof(attr->GetValue()));
					break;
				case Attribute::INTEGER_TYPE:
					_classAd.Assign(ATTRS[i], atol(attr->GetValue()));
					break;
				case Attribute::EXPR_TYPE:
				case Attribute::STRING_TYPE:
				default:
					_classAd.Assign(ATTRS[i], strdup(attr->GetValue()));
			}
		}
		i++;
	}

	return true;
}

bool LiveJob::GetFullAd ( ClassAd& _classAd, std::string& /*text*/ ) const
{
	// TODO: a bool that is always true? :-)
    _classAd = *m_ad;
    return true;
}

// HistoricalJob
HistoricalJob::HistoricalJob ( const char *_key, const HistoryEntry& _he ) :
        Job ( _key ), m_he ( _he )
{
    g_ownerless[_he.cluster] = strdup(_he.owner.c_str());
    this->SetSubmission ( _he.submission.c_str(), _he.cluster );
     dprintf ( D_FULLDEBUG, "HistoricalJob created with key '%s'\n", _key );
}

HistoricalJob::~HistoricalJob ()
{
}

int HistoricalJob::GetStatus() const
{
    return m_he.status;
}

bool HistoricalJob::GetSummary ( ClassAd& _classAd ) const
{
		_classAd.ResetExpr();
		// use HistoryEntry data only
		_classAd.Assign(ATTR_GLOBAL_JOB_ID,m_he.id.c_str());
		_classAd.Assign(ATTR_CLUSTER_ID,m_he.cluster);
		_classAd.Assign(ATTR_PROC_ID,m_he.proc);
		_classAd.Assign(ATTR_Q_DATE,m_he.q_date);
		_classAd.Assign(ATTR_JOB_STATUS,m_he.status);
		_classAd.Assign(ATTR_ENTERED_CURRENT_STATE,m_he.entered_status);
		_classAd.Assign(ATTR_JOB_SUBMISSION,m_he.submission.c_str());
		_classAd.Assign(ATTR_OWNER,m_he.owner.c_str());
		_classAd.Assign(ATTR_JOB_CMD,m_he.cmd.c_str());

		// beyond here these may be empty so don't
		// automatically add to summary
		if (!m_he.args1.empty()) {
			_classAd.Assign(ATTR_JOB_ARGUMENTS1,m_he.args1.c_str());
		}
		if (!m_he.args2.empty()) {
			_classAd.Assign(ATTR_JOB_ARGUMENTS2,m_he.args2.c_str());
		}
		if (!m_he.release_reason.empty()) {
			_classAd.Assign(ATTR_RELEASE_REASON,m_he.release_reason.c_str());
		}
		if (!m_he.hold_reason.empty()) {
			_classAd.Assign(ATTR_HOLD_REASON,m_he.hold_reason.c_str());
		}

	return true;
}

// specialization: this GetFullAd has to retrieve its classad attributes
// from the history file based on index pointers
bool HistoricalJob::GetFullAd ( ClassAd& _classAd, std::string& text ) const
{
    // fseek to he.start
    // ClassAd method to deserialize from a file with "***"

    FILE * hFile;
    int end = 0;
    int error = 0;
    int empty = 0;
	std::ostringstream buf;

    // TODO: move the ClassAd/file deserialize back to HistoryFile???
    const char* fName = m_he.file.c_str();
    if ( ! ( hFile = safe_fopen_wrapper ( fName, "r" ) ) )
    {
		buf <<  "unable to open history file " << m_he.file;
		text = buf.str();
        dprintf ( D_ALWAYS, text.c_str(), "\n");
        return false;
    }
    if ( fseek ( hFile , m_he.start , SEEK_SET ) )
    {
		buf << "bad seek in " << m_he.file << " at " << m_he.start;
		text = buf.str();
        dprintf ( D_ALWAYS, text.c_str(), "\n");
        return false;
    }

    ClassAd myJobAd ( hFile, "***", end, error, empty );
    fclose ( hFile );

	// TODO: debug logging and error to i/f for now
	// we might not have our original history file anymore
    if ( error )
    {
		buf <<  "malformed ad for job '" << GetKey() << "' in " << m_he.file;
		text = buf.str();
        dprintf ( D_FULLDEBUG, "%s\n", text.c_str());
		return false;
    }
    if ( empty )
    {
		buf << "empty ad for job '" << GetKey() << "' in " << m_he.file;
		text = buf.str();
        dprintf ( D_FULLDEBUG,"%s\n", text.c_str());
		return false;
    }

	_classAd = myJobAd;

    return true;
}
