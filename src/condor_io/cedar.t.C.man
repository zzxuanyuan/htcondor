#include "condor_common.h"

#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "condor_debug.h"

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
	SafeSock mySock;
	int op, result;
	char c, rc, *charString, *rcharString;
	int integer, rinteger, len;
	long lint, rlint;
	short sint, rsint;
	float f, rf;
	double d, rd;
	unsigned short port;

	charString = (char *)malloc(100);
	rcharString = (char *)malloc(100);

	result = mySock.set_os_buffers(3000000, false);
	cout << "buffer size set to " << result << endl;

	cout << "(1) Server" << endl;
	cout << "(2) Client" << endl;
	cout << "(9) Exit" << endl;
	cout << "Select: ";
	cin >> op;

	switch(op) {
		case 1: // Server
			cout << "port: ";
			cin >> port;
			result = mySock.bind(port);
			if(result != TRUE) {
				cout << "Bind failed\n";
				exit(-1);
			}
			cout << "Bound to [" << port << "]\n";
			while(true) {
				mySock.decode();
				mySock.code(c);
				mySock.end_of_message();
				cout << "char: " << c << endl;
				mySock.encode();
				mySock.code(c);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(integer);
				mySock.end_of_message();
				cout << "int: " << integer << endl;
				mySock.encode();
				mySock.code(integer);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(lint);
				mySock.end_of_message();
				cout << "long: " << lint << endl;
				mySock.encode();
				mySock.code(lint);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(sint);
				mySock.end_of_message();
				cout << "sint: " << sint << endl;
				mySock.encode();
				mySock.code(sint);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(f);
				mySock.end_of_message();
				cout << "float: " << f << endl;
				mySock.encode();
				mySock.code(f);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(d);
				mySock.end_of_message();
				cout << "double: " << d << endl;
				mySock.encode();
				mySock.code(d);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(charString);
				mySock.end_of_message();
				cout << "str: " << charString << endl;
				mySock.encode();
				mySock.code(charString);
				mySock.end_of_message();

				mySock.decode();
				mySock.code(charString, len);
				mySock.end_of_message();
				cout << "str[" << len << "] " << charString << endl;
				mySock.encode();
				mySock.code(charString);
				mySock.end_of_message();

			}
			break;
		case 2: // Client
			char serverName[30];

			// Connect to the server
			cout << "Server: ";
			cin >> serverName;
			cout << "Server port: ";
			cin >> port;
			result = mySock.connect(serverName, port);
			if(result != TRUE) {
				cout << "Connection failed\n";
				exit(-1);
			}
			cout << "Connected to [" << serverName<< ", " << port << "]\n";
			while(true) {
				mySock.encode();
				cout << "Type char: ";
				cin >> c;
				mySock.code(c);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rc);
				cout << "rcv: " << rc << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type int: ";
				cin >> integer;
				mySock.code(integer);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rinteger);
				cout << "rcv: " << rinteger << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type long: ";
				cin >> lint;
				mySock.code(lint);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rlint);
				cout << "rcv: " << rlint << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type short: ";
				cin >> sint;
				mySock.code(sint);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rsint);
				cout << "rcv: " << rsint << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type float: ";
				cin >> f;
				mySock.code(f);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rf);
				cout << "rcv: " << rf << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type double: ";
				cin >> d;
				mySock.code(d);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rd);
				cout << "rcv: " << rd << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type string: ";
				cin >> charString;
				mySock.code(charString);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rcharString);
				cout << "rcv: " << rcharString << endl;
				mySock.end_of_message();

				mySock.encode();
				cout << "Type string: ";
				cin >> charString;
				mySock.code(charString);
				mySock.end_of_message();
				mySock.decode();
				mySock.code(rcharString);
				cout << "rcv: " << rcharString << endl;
				mySock.end_of_message();
			}

		case 9:
			exit(0);
		default:
			break;
	}
}
