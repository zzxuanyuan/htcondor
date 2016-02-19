package org.HTCondor;

import java.util.Map;
import java.util.List;
import java.util.Calendar;
import java.util.Date;
import java.util.Vector;

import com.amazonaws.services.lambda.runtime.events.SNSEvent;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.SNS;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.SNSRecord;
import com.amazonaws.services.lambda.runtime.events.SNSEvent.MessageAttribute;

import com.amazonaws.services.lambda.runtime.LambdaLogger;
import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.RequestHandler;

import com.amazonaws.util.json.*;

import com.amazonaws.services.cloudformation.*;
import com.amazonaws.services.cloudformation.model.*;

import com.amazonaws.services.sns.*;
import com.amazonaws.services.sns.model.*;

import com.amazonaws.services.cloudwatch.*;
import com.amazonaws.services.cloudwatch.model.*;

import com.amazonaws.regions.*;

public class Lease implements RequestHandler< SNSEvent, String > {
	public Region thisRegion = Region.getRegion( Regions.fromName( System.getenv( "AWS_REGION" ) ) );

	private void deleteStack( String stackName ) {
		AmazonCloudFormationClient acfc = new AmazonCloudFormationClient();
		acfc.setRegion( thisRegion );

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
			// Work around an AWS bug: an updated alarm evaluates its new
			// condition before updating its metric data.

			int evaluationPeriods = -1;
			try {
				evaluationPeriods = trigger.getInt( "EvaluationPeriods" );
			} catch( JSONException je ) {
				logger.log( "Trigger did not contain evaluation period count.\n" );
				return;
			}

			if( metricIsLive( triggerNamespace, "Lease", evaluationPeriods, logger ) ) {
				// If the alarm is actually live, just force it back to OK.
				// It shouldn't fire again until the heartbeat has expired,
				// but even if it does, we will just ignore it again, until
				// enough time has passed to expire the heartbeat.
				logger.log( "Resetting bogus alarm.\n" );

				String alarmName;
				try {
					alarmName = blob.getString( "AlarmName" );
				} catch( JSONException je ) {
					logger.log( "Message did not contain alarm name.\n" );
					return;
				}

				setAlarmState( alarmName, "OK" );
				return;
			}

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

	private boolean metricIsLive( String ns, String m, int p,  LambdaLogger l ) {
		AmazonCloudWatchClient acwc = new AmazonCloudWatchClient();
		acwc.setRegion( thisRegion );
		GetMetricStatisticsRequest gmsRequest = new GetMetricStatisticsRequest();
		gmsRequest.setNamespace( ns );
		gmsRequest.setMetricName( m );
		gmsRequest.setPeriod( 60 );
		Vector< String > stats = new Vector< String >();
		stats.add( "Sum" );
		gmsRequest.setStatistics( stats );

		Calendar start = Calendar.getInstance();
		start.add( Calendar.SECOND, -1 * 60 * p );
		start.add( Calendar.SECOND, -30 );
		gmsRequest.setStartTime( start.getTime() );

		Calendar end = Calendar.getInstance();
		end.add( Calendar.SECOND, 30 );
		gmsRequest.setEndTime( end.getTime() );

		GetMetricStatisticsResult gmsResult = acwc.getMetricStatistics( gmsRequest );
		List<Datapoint> datapoints = gmsResult.getDatapoints();
		if( datapoints.size() > 0 ) {
			return true;
		}

		return false;
	}

	private void setAlarmState( String an, String state ) {
		AmazonCloudWatchClient acwc = new AmazonCloudWatchClient();
		acwc.setRegion( thisRegion );
		SetAlarmStateRequest sasr = new SetAlarmStateRequest();
		sasr.setAlarmName( an );
		sasr.setStateValue( state );
		sasr.setStateReason( "Alarm reset to ensure correctness." );
		acwc.setAlarmState( sasr );
	}
}
