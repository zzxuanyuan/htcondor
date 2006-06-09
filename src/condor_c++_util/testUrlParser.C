#include "UrlParser.h"
#include <stdio.h>

using std::string;	// used _everywhere_ in new ClassAds API

int main(int argc, const char *argv[])
{
	string field, fieldShouldBe;
	bool dir, dirShouldBe;
	const char* url;

	url = "http://www.wisconsin.edu/";
	printf("url: %s\n", url);
	UrlParser parser( url );
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.protocol();
	fieldShouldBe = "http";
	if (field != fieldShouldBe) {
		printf("%d: protocol error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.user();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: user error: was \"%s\" s/b \"%s\"\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	dir = parser.isDirectory();
	dirShouldBe = true;
	if (dir != dirShouldBe) {
		printf("%d: isDirectory error: was \"%d\" s/b \"%d\"\n",
				__LINE__, dir, dirShouldBe );
	}

	url = "file:///dir/file";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.protocol();
	fieldShouldBe = "file";
	if (field != fieldShouldBe) {
		printf("%d: protocol error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.host();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: host error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.port();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: port error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	dir = parser.isDirectory();
	dirShouldBe = false;
	if (dir != dirShouldBe) {
		printf("%d: isDirectory error: was \"%d\" s/b \"%d\"\n",
				__LINE__, dir, dirShouldBe );
	}

	url = "file:/foo/bar";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.protocol();
	fieldShouldBe = "file";
	if (field != fieldShouldBe) {
		printf("%d: protocol error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.host();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: host error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.port();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: port error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	dir = parser.isDirectory();
	dirShouldBe = false;
	if (dir != dirShouldBe) {
		printf("%d: isDirectory error: was \"%d\" s/b \"%d\"\n",
				__LINE__, dir, dirShouldBe );
	}

	url = "ftp://hacker:secret@insecure.gw.com/dir/file";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.protocol();
	fieldShouldBe = "ftp";
	if (field != fieldShouldBe) {
		printf("%d: protocol error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.user();
	fieldShouldBe = "hacker";
	if (field != fieldShouldBe) {
		printf("%d: user error: was \"%s\" s/b \"%s\"\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.password();
	fieldShouldBe = "secret";
	if (field != fieldShouldBe) {
		printf("%d: password error: was \"%s\" s/b \"%s\"\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}

	url = "gsiftp://cracker@moreinsecure.gw.com/dir/file";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.protocol();
	fieldShouldBe = "gsiftp";
	if (field != fieldShouldBe) {
		printf("%d: protocol error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.user();
	fieldShouldBe = "cracker";
	if (field != fieldShouldBe) {
		printf("%d: user error: was \"%s\" s/b \"%s\"\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.password();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: password error: was \"%s\" s/b \"%s\"\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}

	url = "http://www.wisconsin.edu/";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.host();
	fieldShouldBe = "www.wisconsin.edu";
	if (field != fieldShouldBe) {
		printf("%d: host error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.port();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: port error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}

	url = "http://www.colorado.edu:8080/dir/file";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.host();
	fieldShouldBe = "www.colorado.edu";
	if (field != fieldShouldBe) {
		printf("%d: host error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.port();
	fieldShouldBe = "8080";
	if (field != fieldShouldBe) {
		printf("%d: port error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.directory();
	fieldShouldBe = "dir";
	if (field != fieldShouldBe) {
		printf("%d: directory error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.file();
	fieldShouldBe = "file";
	if (field != fieldShouldBe) {
		printf("%d: file error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}

	url = "http://www.wisconsin.edu/mydir/";
	printf("url: %s\n", url);
	parser = url;
	if (! parser.parse() ) {
		printf("%d: parse %s error: %s\n", __LINE__, url, parser.errorMsg() );
	}
	field = parser.directory();
	fieldShouldBe = "mydir";
	if (field != fieldShouldBe) {
		printf("%d: directory error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	field = parser.file();
	fieldShouldBe = "";
	if (field != fieldShouldBe) {
		printf("%d: directory error: was %s s/b %s\n",
				__LINE__, field.c_str(), fieldShouldBe.c_str() );
	}
	dir = parser.isDirectory();
	dirShouldBe = true;
	if (dir != dirShouldBe) {
		printf("%d: isDirectory error: was \"%d\" s/b \"%d\"\n",
				__LINE__, dir, dirShouldBe );
	}

	return 0;
}

