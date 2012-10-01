#include "picojson.h"
#include <fstream>
#include <stdio.h>

using namespace std;

static bool bDebug=false;

///< Each parameter is a name=value pair.
typedef struct tparam
{
	string _name;
	string _value;
}tParam;

/// Gets a string out of a single value if it is a string. 
bool get_string(picojson::value o, string & szVal)
{
    bool bRet = true;
    
    if (!o.is<string>())
    {
	cout<<"ERROR IN FILE FORMAT - NOT A STRING"<<endl;
	bRet = false;
    }
    else
    {
	szVal = o.get<string>();
    }
    
    return bRet;
}

// obtains an array of strings.
bool get_string_list(picojson::value o, vector<string>& szList)
{
    bool bRet = false;
    
    if (o.is<picojson::array>()) 
    {
	const picojson::array& a = o.get<picojson::array>();
	for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i) 
	{   
	    string szVal; 
	    
	    if ( (bRet = get_string (*i, szVal) ) ) 
	    {
		szList.push_back(szVal);
	    }
	    else
	    {
		break;
	    }
	}
    }
    else
    {
	cout<<"ERROR IN FILE FORMAT - NOT AN ARRAY OF STRINGS"<<endl;
    }
	
    
    return bRet;
}

// get the default
bool get_default(picojson::value o, string & szVal)
{
    vector<string> szList;
    bool bRet;
    
    // default order in the list is [ ANYTHING BUT WINDOWS, WINDOWS ]
    if ( ( bRet = get_string_list (o, szList) ) )
    {
#ifdef WIN32 
	if ( szList.size() > 1 )
	{
	    szVal = szList[ szList.size()-1 ];
	    return true;
	}
#endif
	szVal = szList[0];
    }
    
    return bRet;
}

// KEYWORDS IN JSON 
#define NAME 		"name"
#define NSPRINTF 	"nsprintf"
#define VALUE		"value"

bool parse_record( picojson::object o, vector< tParam > & table )
{
    bool bRet = true;
    
    vector<string> nsprintf;
    string name;
    string value;
    
    tParam param;
    
    // loop through the elements in each object switch on keywords.
    for (picojson::object::const_iterator j = o.begin(); j != o.end(); ++j)
    {
	if (bDebug)
	{
	    // extra output to check ones sanity.
	    cout << "(" << j->first <<")-> "<<j->second<<endl;
	}
		    
	if ( 0 == strcmp (j->first.c_str(), NAME) )
	{
	    bRet = get_string(j->second, name);
	}
	else if ( 0 == strcmp (j->first.c_str(), NSPRINTF) )
	{
	    bRet = get_string_list(j->second, nsprintf);
	}
	else if ( 0 == strcmp (j->first.c_str(), VALUE ) )
	{
	    bRet = get_default(j->second, value);
	}
	
	if (!bRet)
	    break;
    }
    
    param._value = value;
    
    if (nsprintf.size())
    {
	for (unsigned int i=0; i<nsprintf.size(); i++)
	{
	    char buff[256]; 
	    sprintf( buff,name.c_str(), nsprintf[i].c_str() );
	    
	    param._name = buff; 
	    table.push_back( param );
	}
    }
    else
    {
	param._name = name;
	table.push_back(param);
    }
    
    
    return (bRet);
}

///< 
typedef struct tdefaults
{
	vector< tParam > _defaults;
	
	// This performs the useful part of loading the object.
	bool load (const picojson::array& a) 
	{
	    bool bRet = true;
	    
	    _defaults.clear();
	    
	    // loop through each object in the array/table
	    for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i)
	    {
		// get the non-named object wrapper.
		picojson::value v = *i;
		picojson::object o = v.get<picojson::object>();
		
		if ( !parse_record(o,_defaults) )
		{
		    return false;
		}
		
	    }
  
	    return true;
	}
	
	// This spews the contents out for the defaults table.
	bool spew (const string szFileName)
	{
	    bool bRet = true;
	    
	    for (unsigned int i=0; i<_defaults.size(); i++)
	    {
		cout<<_defaults[i]._name<<"="<<_defaults[i]._value<<endl;
	    }
	    
	    return bRet;
	}
	
}tDefaultTable;


//////////////////////////////////////////////////////////////////////
// The following are simple parsing functions which will recursively
// walk the input json.  This is very useful when defining the format
// or trying to debug the file for whatever reason. 
//
void print_parse_value (picojson::value obj_v, string szPrePend)
{
    if (obj_v.is<picojson::null>()) {
	cout << "NULL" << endl;
    } else if (obj_v.is<bool>()) {
	cout << (obj_v.get<bool>() ? "true" : "false") << endl;
    } else if (obj_v.is<double>()) {
	cout << obj_v.get<double>() << endl;
    } else if (obj_v.is<string>()) {
	cout << obj_v.get<string>() << endl;
    } else if (obj_v.is<picojson::array>()) {
	cout <<"[ARRAY]"<<endl;
	szPrePend+="\t";
	const picojson::array& a = obj_v.get<picojson::array>();
	for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i) 
	{   
	    cout << szPrePend;
	    print_parse_value (*i, szPrePend );
	}
    }
    else if (obj_v.is<picojson::object>()) {
	cout << endl;
	szPrePend+="\t";
	picojson::object & o = obj_v.get<picojson::object>();
	for (picojson::object::const_iterator i = o.begin(); i != o.end(); ++i)
	{
	    picojson::value obj_v = i->second;
	    cout << szPrePend << "(" << i->first <<")-> ";
	    print_parse_value(obj_v, szPrePend);
	}
    
    }
}
//////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    picojson::value v;
    tDefaultTable default_table;
    fstream fParams_in;
    
    // load the defaults table.
    fParams_in.open ("params.json.in", fstream::in | fstream::app);  
    fParams_in >> v;
    
    // check to see if we load the file. 
    if (fParams_in.fail()) 
    {
	cerr << picojson::get_last_error() << endl;
	return 1;
    }
  
    // used to debug if it is necessary
    if (argc==2 ) 
    {
	if (0 == strcmp (argv[1],"--debug"))
	{    
	    cout<<"------------------------ BEGIN DEBUG PARSE PHASE ------------------------"<<endl;
	    // this will test print out the object format during a parse
	    print_parse_value(v, "");
	    cout<<"------------------------ END DEBUG PARSE PHASE ------------------------"<<endl;
	    bDebug=true;
	}
	
    }
   
    // walk through the object
    picojson::object & o = v.get<picojson::object>();
    picojson::object::const_iterator i = o.begin();
    const picojson::array& a = i->second.get<picojson::array>();

    cout << "---- LOADING DATA STRUCTURE  ----" << endl;
    if ( !default_table.load(a)  )
    {
	cerr << "INVALID FORMAT"<<endl;
    }
	
    cout << "---- WRITING CONSUMABLE GOO ----" << endl;
    if ( !default_table.spew("param_info_init.c")  )
    {
	cerr << "OHH I SUCK!!"<<endl;
    }
    
    return 0;
}
