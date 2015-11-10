var response = require( 'response' );
exports.handler = function( event, context ) {

	console.log( "Received request:\n", JSON.stringify( event ) );
	if( event.RequestType != 'Delete' ) {
		console.log( "Ignoring non-delete request." );
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
	if(! s3PoolPassword) {
		var responseData = { Error : 's3PoolPassword not specified' };
		console.log( responseData.Error );
		response.send( event, context, response.FAILED, responseData );
		return;
	}

	var AWS = require( 'aws-sdk' );
	var sns = new AWS.SNS();
	var s3 = new AWS.S3();

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
			console.log( "... got zero subscriptions, nothing to do." );
			response.send( event, context, response.SUCCESS );
			return;
		}

		var processed = 0;
		console.log( "... got subscriptions, removing them..." );
		for( var i = 0; i < subs.length; ++i ) {
			sns.unsubscribe( { SubscriptionArn : subs[i].SubscriptionArn }, function( error, data ) {
				if( error ) {
					SendFailedResponse( error, 'unsubscribe() call failed' );
					return;
				}

				++processed;
				if( processed == subs.length ) {
					console.log( "... removed all subscriptions." );
					response.send( event, context, response.SUCCESS );
				}
			} );
		}
	};


	var names = s3PoolPassword.split( '/' );

	console.log( "Deleting pool password file..." );
	s3.deleteObject( { Bucket : names[0], Key : names[1] }, function( error, data ) {
		if( error ) {
			SendFailedResponse( error, 'delete() call failed' );
			return;
		} else {
			console.log( "... pool password file deleted." );
			console.log( "Looking up subscriptions..." );
			sns.listSubscriptionsByTopic( { TopicArn : topicARN }, lsbtFunction );
		}
	} );

}
