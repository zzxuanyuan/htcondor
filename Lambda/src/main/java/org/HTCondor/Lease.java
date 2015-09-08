package org.HTCondor;

import java.util.Map;
import java.util.List;

import com.amazonaws.services.lambda.runtime.events.SNSEvent;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.SNS;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.SNSRecord;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.MessageAttribute;

import com.amazonaws.services.lambda.runtime.LambdaLogger;
import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.RequestHandler;

import com.amazonaws.util.json.*;

// import com.amazonaws.services.autoscaling.*;
// import com.amazonaws.services.autoscaling.model.*;
import com.amazonaws.services.cloudformation.*;
import com.amazonaws.services.cloudformation.model.*;

public class Lease implements RequestHandler< SNSEvent, String > {
	private void deleteStack( String stackName ) {
		AmazonCloudFormationClient acfc = new AmazonCloudFormationClient();

		DeleteStackRequest dsr = new DeleteStackRequest();
		dsr.setStackName( stackName );

		acfc.deleteStack( dsr );
	}

	private void handleRecord( SNSRecord record, Context context ) {
		LambdaLogger logger = context.getLogger();
		logger.log( "Handling notification from " + record.getEventSource() + "\n" );

		SNS notification = record.getSNS();
		String subject = notification.getSubject();
		String message = notification.getMessage();
		logger.log( "Subject = " + subject + "\n" );
		logger.log( "Message = " + message + "\n" );

		Map< String, MessageAttribute > attributes = notification.getMessageAttributes();
		logger.log( "Found " + attributes.size() + " message attribute(s).\n" );
		for( Map.Entry< String, MessageAttribute > entry : attributes.entrySet() ) {
			logger.log( "[attr = value] " + entry.getKey() + " = " + entry.getValue().getValue() + "\n" );
		}

		//
		// SNS messages are JSON blobs; they don't (yet?) use MessageAttributes.
		// We don't want to have to recompile or reconfigure this lambda
		// function if we don't have to, so look for a way to get the name
		// or ID of the triggering Autoscale Group from the blob.
		//
		// By convention, this will be embedded into the namespace of the
		// metric.
		//
		JSONObject blob;
		try {
			blob = new JSONObject( message );
		} catch( JSONException je ) {
			logger.log( "Failed to deblob message (" + je.toString() + ").\n" );
			return;
		}

		String triggerString;
		try {
			triggerString = blob.getString( "Trigger" );
		} catch( JSONException je ) {
			logger.log( "Message did not contain trigger.\n" );
			return;
		}

		JSONObject trigger;
		try {
			trigger = new JSONObject( triggerString );
		} catch( JSONException je ) {
			logger.log( "Failed to deblob trigger (" + triggerString + ").\n" );
			return;
		}

		String triggerNamespace;
		try {
			triggerNamespace = trigger.getString( "Namespace" );
		} catch( JSONException je ) {
			logger.log( "Trigger did not contain namespace.\n" );
			return;
		}
		logger.log( "Will parse trigger's namespace (" + triggerNamespace + ") to find target.\n" );

		String[] nsEntries = triggerNamespace.split( "/" );
		if( nsEntries.length != 4 ) {
			logger.log( "Did not find the required number of pieces in the namespace.\n" );
			return;
		}
		if(! nsEntries[0].equals( "HTCondor" )) {
			logger.log( "Not an HTCondor metric (" + nsEntries[0] + "), ignoring.\n" );
			return;
		}
		if(! nsEntries[1].equals( "Leases" )) {
			logger.log( "Not a lease (" + nsEntries[1] + "), ignoring.\n" );
			return;
		}

		if( nsEntries[2].equals( "CloudFormation" ) ) {
			logger.log( "Will delete CloudFormation stack '" + nsEntries[3] + "'...\n" );
			deleteStack( nsEntries[3] );
		} else {
			logger.log( "Object of unknown type (" + nsEntries[2] + ") leased, ignoring.\n" );
			return;
		}
	}

	public String handleRequest( SNSEvent event, Context context ) {
		List<SNSRecord> records = event.getRecords();
		LambdaLogger logger = context.getLogger();
		logger.log( "Got " + records.size() + " notification(s).\n" );
		for( int i = 0; i < records.size(); ++i ) {
			handleRecord( records.get( i ), context );
		}
		return "Only for testing.";
	}
}
