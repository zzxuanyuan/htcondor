import java.sql.*;

import oracle.jdbc.driver.*;

public class OracleJobInterface {
	
	static String SubmitCmd = "submit";
	static String CommitCmd = "commit";
	static String StatusCmd = "status";
	static String RemoveCmd = "remove";

	static
	void doSubmit( Connection connection, String args[] ) {
		System.err.println("doSubmit called");
		if ( args.length != 0 ) {
			System.err.println("Syntax: submit");
			return;
		}

		try {
			CallableStatement cstmt;

			cstmt = connection.prepareCall("{call DBMS_JOB.SUBMIT(?,?,SYSDATE+(10/1440))}");

			cstmt.registerOutParameter( 1, java.sql.Types.INTEGER );
//			cstmt.setString( 2, "update jfrey1 set mykey = mykey;" );
			cstmt.setString( 2, "declare my_job binary_integer; begin select job into my_job from user_jobs; end;" );
			ResultSet rs = cstmt.executeQuery();
			int i = cstmt.getInt(1);
			System.out.println(i);
			cstmt.close();

		} catch (SQLException e) {
			System.err.println("SQLException. " + e.getMessage());
			System.out.println("error");
		}
	}

	static
	void doCommit( Connection connection, String args[] ) {
		System.err.println("doCommit called");
		if ( args.length != 2 ) {
			System.err.println("Syntax: commit <job id> <job text>");
			return;
		}

		try {
			CallableStatement cstmt;

			cstmt = connection.prepareCall("{call DBMS_JOB.CHANGE("+args[0]+",?,SYSDATE,NULL)}");
			cstmt.setString(1, args[1]);
			ResultSet rs = cstmt.executeQuery();
			cstmt.close();

		} catch (SQLException e) {
			System.err.println("SQLException. " + e.getMessage());
			System.out.println("error");
		}
	}

	static
	void doStatus( Connection connection, String args[] ) {
		System.err.println("doStatus called");
		if ( args.length != 1 ) {
			System.err.println("Syntax: status <job id>");
			return;
		}

		try {
			Statement stmt;

			stmt = connection.createStatement();
			String query = "select * from user_jobs where job = " + args[0];
			ResultSet rs = stmt.executeQuery(query);

			if ( rs.next() == false ) {
				// The job isn't in the queue
				System.out.println("COMPLETED");
			} else {
				// The job is in the queue, check its status
				Date run_date = rs.getDate("this_date");
				if ( run_date == null ) {
					System.out.println("IDLE");
				} else {
					System.out.println("RUNNING");
				}
			}

			stmt.close();

		} catch (SQLException e) {
			System.err.println("SQLException. " + e.getMessage());
			System.out.println("error");
		}
	}

	static
	void doRemove( Connection connection, String args[] ) {
		System.err.println("doRemove called");
		if ( args.length != 1 ) {
			System.err.println("Syntax: remove <job id>");
			return;
		}

		try {
			CallableStatement cstmt;

			cstmt = connection.prepareCall("{call DBMS_JOB.REMOVE("+args[0]+")}");
			ResultSet rs = cstmt.executeQuery();
			cstmt.close();

		} catch (SQLException e) {
			System.err.println("SQLException. " + e.getMessage());
			System.out.println("error");
		}
	}

    public static void main( String args[] ) {

		int exit_code = 0;

		if ( args.length < 6 ) {
			System.err.println("Usage: OracleJobInterface serverName serverPort sid username password command [arguments]");
			Runtime curr_runtime = Runtime.getRuntime();
			curr_runtime.exit(1);
		}

		String cmd = new String( args[5] );
		String[] cmd_args = new String[args.length - 6];
		for ( int i = 0; i < args.length - 6; i++ ) {
			cmd_args[i] = args[i + 6];
		}

		Connection connection = null;

		try {
			// Load the JDBC driver
			String driverName = "oracle.jdbc.driver.OracleDriver";
			Class.forName(driverName);
		} catch (ClassNotFoundException e) {
			System.err.println("ClassNotFoundException. " + e.getMessage());
			// Could not find the database driver
			Runtime curr_runtime = Runtime.getRuntime();
			curr_runtime.exit(2);
		}

		try {
			// Create a connection to the database
			String url = "jdbc:oracle:thin:@" + args[0] + ":" + args[1] + ":" + args[2];
			connection = DriverManager.getConnection(url, args[3], args[4]);

			connection.setAutoCommit(false);

			if ( cmd.equalsIgnoreCase( SubmitCmd ) ) {
				doSubmit( connection, cmd_args );
			} else if ( cmd.equalsIgnoreCase( CommitCmd ) ) {
				doCommit( connection, cmd_args );
			} else if ( cmd.equalsIgnoreCase( StatusCmd ) ) {
				doStatus( connection, cmd_args );
			} else if ( cmd.equalsIgnoreCase( RemoveCmd ) ) {
				doRemove( connection, cmd_args );
			} else {
				System.err.println("Unknown command '" + cmd + "'");
				exit_code = 4;
			}

			// call method functions


			connection.close();

		} catch (SQLException e) {
			System.err.println("SQLException. " + e.getMessage());
			// Could not connect to the database
			Runtime curr_runtime = Runtime.getRuntime();
			curr_runtime.exit(2);
		}

		System.err.println("Exitting main!");
	}

}
