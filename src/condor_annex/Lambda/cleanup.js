var response = require( 'response' );
exports.handler = function( event, context ) {

	// console.log( "Received request:\n", JSON.stringify( event ) );
	if( event.RequestType != 'Delete' ) {
		response.send( event, context, response.SUCCESS );
		return;
	}

	console.log( "Received delete request." );
	var topicARN = event.ResourceProperties.topicARN;
	if(! topicARN) {
		var responseData = { Error : 'topicARN not specified' };
		console.log( responseData.Error );
		response.send( event, context, response.FAILED, responseData );
		return;
	}

	var s3PoolPassword = event.ResourceProperties.s3PoolPassword;
	// ...

	var AWS = require( 'aws-sdk' );
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
			// console.log( "... got zero subscriptions, nothing to do." );
			response.send( event, context, response.SUCCESS );
			return;
		}

		var processed = 0;
		// console.log( "... got subscriptions, removing them..." );
		for( var i = 0; i < subs.length; ++i ) {
			sns.unsubscribe( { SubscriptionArn : subs[i].SubscriptionArn }, function( error, data ) {
				if( error ) {
					SendFailedResponse( error, 'unsubscribe() call failed' );
					return;
				}

				++processed;
				if( processed == subs.length ) {
					// console.log( "... removed all subscriptions." );
					response.send( event, context, response.SUCCESS );
				}
			} );
		}
	};

	// console.log( "Looking up subscriptions..." );
	sns.listSubscriptionsByTopic( { TopicArn : topicARN }, lsbtFunction );
}
