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

#if defined(HAVE_EXT_OPENSSL)

/*
 * report_openssl_errors
 * 
 * Pulls errors from the ERR functions in OpenSSL and puts them into
 * the standard Condor dprintf.
 */
void
report_openssl_errors(const char *func_name);

#endif /* defined(HAVE_EXT_OPENSSL) */
