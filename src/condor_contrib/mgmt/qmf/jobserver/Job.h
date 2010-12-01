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

#ifndef _JOB_H
#define _JOB_H

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"

#include "JobLogReader.h"
#include "HistoryFile.h"

#include "cmpstr.h"

#include <string>
#include <map>
#include <set>
#include <vector>

using std::string;
using std::map;
using std::set;
using std::vector;

class SubmissionObject;

class Attribute
{
    public:
        enum AttributeType
        {
            EXPR_TYPE = 0,
            INTEGER_TYPE = 1,
            FLOAT_TYPE = 2,
            STRING_TYPE = 3
        };
        Attribute ( AttributeType _type, const char *_value );
        ~Attribute();

        AttributeType GetType() const;
        const char * GetValue() const;

    private:
        AttributeType m_type;
        const char * m_value;
};

class Job
{
    public:
        Job ( const char *_key);
        virtual ~Job();

        const char * GetKey() const;
        virtual int GetStatus () const = 0;
		virtual bool GetSummary ( ClassAd& _ad ) const = 0;
        virtual bool GetFullAd ( ClassAd& _ad , std::string& text) const = 0;
        void IncrementSubmission();
        void DecrementSubmission();

	// sigh...
	virtual void Set ( const char *_name, const char *_value );
	virtual void Remove ( const char *_name );

    protected:
        SubmissionObject *m_submission;
        void SetStatus ( const Job* _job);
        void SetSubmission ( const char* _subName, int cluster );

    private:
        const char * m_key;
};

class LiveJob : public Job
{
    public:
        LiveJob ( const char *_key,
                  const LiveJob* _parent );
        ~LiveJob();
        int GetStatus () const;
        void Set ( const char *_name, const char *_value );
        bool Get ( const char *_name, const Attribute *&_attribute ) const;
        void Remove ( const char *_name );
		bool GetSummary ( ClassAd& _classAd ) const;
        bool GetFullAd ( ClassAd& _classAd, std::string& text ) const;

    private:
        ClassAd* m_ad;
};

// Job subclass that encapsulates the archived job history
// and can derive the attributes of its associated class ad
class HistoricalJob : public Job
{
    public:
        HistoricalJob ( const char *_key,
                        const HistoryEntry& _he );
        ~HistoricalJob();
        int GetStatus () const;
		bool GetSummary ( ClassAd& _classAd ) const;
        bool GetFullAd ( ClassAd& _classAd, std::string& text ) const;
    private:
        HistoryEntry m_he;
};

#endif /* _JOB_H */
