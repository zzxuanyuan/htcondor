/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef CABENCH_ADWRAP_BASE_H
#define CABENCH_ADWRAP_BASE_H

class CaBenchAdWrapBase
{
  public:
	CaBenchAdWrapBase( bool dtor_del_ad );
	virtual ~CaBenchAdWrapBase( void );

	void setDtorDelAd( bool v ) { m_dtor_del_ad = v; };
	bool getDtorDelAd( void ) const { return m_dtor_del_ad; };
	virtual void deleteAd( void ) = 0;

  private:
	bool		 m_dtor_del_ad;
};

#endif
