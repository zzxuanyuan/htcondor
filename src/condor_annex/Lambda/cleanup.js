var response = require( 'response' );
exports.handler = function( event, context ) {

	console.log( 'Received request:\\n', JSON.stringify( event ) );
	if( event.RequestType != 'Delete' ) {
		response.send( event, context, response.SUCCESS );
		return;
	}

	var stackName = event.ResourceProperties.stackName;
	var s3PoolPassword = event.ResourceProperties.s3PoolPassword;
	if(! stackName) {
		var responseData = { Error : 'stackName not specified' };
		console.log( responseData.Error );
		response.send( event, context, response.FAILED, responseData );
		return;
	}

	var AWS = require( 'aws-sdk' );
	var cf = new AWS.CloudFormation();
	var sns = new AWS.SNS();

	function SendFailedResponse( error, message ) {
		console.log( error, error.stack );
		var responseData = { Error : message };
		response.send( event, context, response.FAILED, responseData );
	};

	var topic;
	var lsbtFunction = function( error, data ) {
		if( error ) {
			SendFailedResponse( error, 'listSubscriptionsByTopic() call failed' );
			return;
		}

		var subs = data.Subscriptions;
		if( subs.length == 0 ) {
			response.send( event, context, response.SUCCESS );
			return;
		}

		var processed = 0;
		for( var i = 0; i < subs.length; ++i ) {
			sns.unsubscribe( { SubscriptionArn : subs[i].SubscriptionArn }, function( error, data ) {
				if( error ) {
					SendFailedResponse( error, 'unsubscribe() call failed' );
					return;
				}

				++processed;
				if( processed == subs.length ) {
					response.send( event, context, response.SUCCESS );
				}
			} );
		}
	};

	var dsrFunction = function( error, data ) {
		if( error ) {
			SendFailedResponse( error, 'describeStackResource() call failed' );
			return;
		}

		topic = data.StackResourceDetail;
		sns.listSubscriptionsByTopic( { TopicArn : topic.PhysicalResourceId }, lsbtFunction );
	};

	cf.describeStackResource( { StackName : stackName, LogicalResourceId : 'Topic' }, dsrFunction );
}
