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

	var s3DeleteBucket = event.ResourceProperties.s3DeleteBucket;
	var s3ConfigFile = event.ResourceProperties.s3ConfigFile;

	var AWS = require( 'aws-sdk' );
	var sns = new AWS.SNS();
	// We don't create the password buckets in a specific region.
	var s3 = new AWS.S3( { endpoint : "https://s3.amazonaws.com/" } );

	function SendFailedResponse( error, message ) {
		console.log( message );
		console.log( error, error.stack );
		var responseData = { Error : message };
		response.send( event, context, response.FAILED, responseData, message );
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

			// We assume this isn't particularly sensitive data, so it's
			// OK not to delete when the stack is deleted.
			if( s3ConfigFile ) {
				names = s3ConfigFile.split( '/' );
				console.log( "Deleting config file..." );
				s3.deleteObject( { Bucket : names[0], Key : names[1] }, function( error, data ) {
					if( error ) {
						console.log( "Failed to delete config file" );
						console.log( error, error.stack );
					} else {
						console.log( "... config file deleted." );
					}
				} );
			}

			if( s3DeleteBucket ) {
				console.log( "Deleting bucket " + s3DeleteBucket + "..." );
				s3.deleteBucket( { Bucket : s3DeleteBucket }, function( error, data ) {
					// If there's stuff still in the bucket after we've
					// cleaned it up, we can't delete the bucket... but since
					// it's not our stuff, we shouldn't anyway.
					if( error ) {
						console.log( "Failed to delete bucket" );
						console.log( error, error.stack );
					} else {
						console.log( "... bucket deleted." );
					}
				} );
			}

			console.log( "Looking up subscriptions..." );
			sns.listSubscriptionsByTopic( { TopicArn : topicARN }, lsbtFunction );
		}
	} );

}
