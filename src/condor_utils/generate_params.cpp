#include "picojson.h"
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <errno.h>

#ifdef WIN32
  #define strcasecmp _stricmp
#endif

using namespace std;

static bool bDebug=false;

typedef enum dataType
{
    PARAM_TYPE_STRING = 0,
    PARAM_TYPE_INT = 1,
    PARAM_TYPE_BOOL = 2,
    PARAM_TYPE_DOUBLE = 3
}dType;
	
///< Each parameter is a name=value pair.
typedef struct tparam
{
	string _name;
	string _value;
	dType _type;
	string range_low;
	string range_high;
	
}tParam;

/// escape a string for output purposes.
string escape_str (const string & szIn)
{
    string szRet;
    
    for ( unsigned int i=0; i<szIn.length(); i++ )
    {
	// you could add in the other qualifiers if needed.
	if ( szIn[i] == '\\' || szIn[i] == '\"' )
	{
	    szRet += '\\';
	}
	
	szRet += szIn[i];
    }
    
    return szRet;
}

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
#define TYPE            "type"

bool parse_record( picojson::object o, vector< tParam > & table )
{
    bool bRet = true;
    
    vector<string> nsprintf;
    string name;
    string value;
    string type;
    
    tParam param;
    
    // loop through the elements in each object switch on keywords.
    for (picojson::object::const_iterator j = o.begin(); j != o.end(); ++j)
    {
	string key = j->first;
	
	if (bDebug)
	{
	    // extra output to check ones sanity.
	    cout << "(" << key <<")-> "<<j->second<<endl;
	}
		    
	// here is where we check the keys
	if ( 0 == strcasecmp (key.c_str(), NAME) )
	{
	    bRet = get_string(j->second, name);
	}
	else if ( 0 == strcasecmp (key.c_str(), NSPRINTF) )
	{
	    bRet = get_string_list(j->second, nsprintf);
	}
	else if ( 0 == strcasecmp (key.c_str(), VALUE ) )
	{
	    bRet = get_default(j->second, value);
	}
	else if ( 0 == strcasecmp (key.c_str(), TYPE ) )
	{
	    
	    if ( (bRet = get_string(j->second, type) ) )
	    { 
		
		if (0 == strcasecmp (type.c_str(), "int") )
		{
		    param._type = PARAM_TYPE_INT;
		    
		}else if (0 == strcasecmp (type.c_str(), "bool"))
		{
		      param._type = PARAM_TYPE_BOOL;
		      
		}else if (0 == strcasecmp (type.c_str(), "double"))
		{
		     param._type = PARAM_TYPE_DOUBLE;
		     
		}else 
		{
		    param._type = PARAM_TYPE_STRING;
		}
		
	    }
	    
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
	    _defaults.clear();
	    
	    // loop through each object in the array/table
	    for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i)
	    {
		// get the non-named object wrapper.
		picojson::value v = *i;
		picojson::object o = v.get<picojson::object>();
		
		// parse each entry or record.
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
	    
	    FILE * fOut = fopen(szFileName.c_str(), "w+");
	    
	    if (!fOut) 
		return false;
	    
	    // write out the heading goo...
	    fprintf(fOut, "#include \"param_info.h\"\n");
	    fprintf(fOut, "void param_info_init()\n{\n");
	    fprintf(fOut, "\tstatic int done_once = 0;\n\n");
	    fprintf(fOut, "\tif (done_once == 1) {\n\t\treturn;\n\t}\n");
	    fprintf(fOut, "\t\n\tdone_once = 1;\n\n\tparam_info_hash_create(&param_info);\n");
	    fprintf(fOut, "\tparam_info_storage_t tmp;\n\n");
	    
	    cout<<"******DEFAULT PARAMS******"<<endl;
	    for (unsigned int i=0; i<_defaults.size(); i++)
	    {
		const char * name = _defaults[i]._name.c_str();
		const char * value = _defaults[i]._value.c_str();
		string escaped = escape_str(_defaults[i]._value);
		unsigned int str_len = _defaults[i]._value.length();
		bool valid = str_len?true:false;
		istringstream buffer(_defaults[i]._value);
		
		fprintf(fOut, "\ttmp.type_string.hdr.name = \"%s\";\n", name );
		fprintf(fOut, "\ttmp.type_string.hdr.str_val = \"%s\";\n", escaped.c_str());
		
		cout<<name<<"="<<escaped<<" [type=";
		switch ( _defaults[i]._type )
		{
		    case PARAM_TYPE_INT:
			cout<<"int]"<<endl;
			fprintf(fOut, "\ttmp.type_string.hdr.type = PARAM_TYPE_INT;\n"); 
			int numb;
			buffer >> numb;
			
			if (valid && !buffer.fail())
			{
			    fprintf(fOut, "\ttmp.type_int.int_val = %s;\n",value);
			}
			else
			{
			    valid = false;
			}
			    
			break;
		    case PARAM_TYPE_BOOL:
			cout<<"bool]"<<endl;
			fprintf(fOut, "\ttmp.type_string.hdr.type = PARAM_TYPE_BOOL;\n");
			
			if (0 == strcasecmp(value, "true"))
			    fprintf(fOut, "\ttmp.type_int.int_val = 1;\n");
			else
			    fprintf(fOut, "\ttmp.type_int.int_val = 0;\n");
			
			break;
		    case PARAM_TYPE_DOUBLE:
			cout<<"double]"<<endl;
			fprintf(fOut, "\ttmp.type_string.hdr.type = PARAM_TYPE_DOUBLE;\n");
			double d;
			
			buffer >> d;
			if (valid && !buffer.fail())
			{
			    fprintf(fOut, "\ttmp.type_double.dbl_val = %s;\n", value);
			}
			else
			{
			    valid = false;
			}
			
			break;
		    default:
			cout<<"string]"<<endl;
			fprintf(fOut, "\ttmp.type_string.hdr.type = PARAM_TYPE_STRING;\n");
			
		}
		
		fprintf(fOut, "\ttmp.type_string.hdr.default_valid = %d;\n", valid?1:0 );
		fprintf(fOut, "\ttmp.type_string.hdr.range_valid = 0;\n");
		fprintf(fOut, "\tparam_info_hash_insert(param_info, &tmp);\n\n");
		
	    }
	    
	    cout<<"******END DEFAULT PARAMS******"<<endl;
	    fprintf(fOut, "\tparam_info_hash_optimize(param_info);\n}\n\n");
	    fclose(fOut);
	    
	    return true;
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
//
////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    picojson::value v;
    tDefaultTable default_table;

    // load the defaults table.
    if (argc<3 || 0 != strcmp (argv[1],"-i"))
    {
	cerr << "invalid input: -i <filename> -debug"<<endl;
    }

    ifstream fParams_in(argv[2]);
    
    if ( fParams_in.fail() )
    {
       cerr << "Failed to open file:"<<endl;
       return 1;
    }
    
    fParams_in >> v;
    
    // check to see if we load the file. 
    if (fParams_in.fail()) 
    {
	cerr << picojson::get_last_error() << endl;
	return 1;
    }
  
    // used to debug if it is necessary
    if (argc==4) 
    {
	if (0 == strcmp (argv[3],"-debug"))
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
	return 1;
    }
	
    cout << "---- WRITING CONSUMABLE GOO ----" << endl;
    if ( !default_table.spew("param_info_init.c")  )
    {
	cerr << "FAILED TO WRITE"<<endl;
	return 1;
    }
    
    return 0;
}
