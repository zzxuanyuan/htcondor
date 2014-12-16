#/usr/bin/env python


import sys
from urlparse import urlparse

import htcondor

# Capabilities
capabilities = """PluginVersion = "0.1"
PluginType = "FileTransfer"
SupportedMethods = "cached"
"""


# When the starter starts, it queries the plugins for their capabilities
# Handle the query here
if sys.argv[1] == "-classad":
    print capabilities
    sys.exit(0)
    
    
# The arguments should be:
# cached://cached_name/cache_name /path/to/install
URL = sys.argv[1]
parsed_url = urlparse(URL)

install_path = sys.argv[2]

# Step 1: Query the collector for the cached
# Step 2: Connect to the cached
# Step 3: Query the cached for the cache classad
# Step 4: Send the cache classad to the local cached

collector = htcondor.Collector()

cacheds = collector.query("Any", "Name == %s" % parsed_url.netloc)

if len(cacheds) != 1:
    print "Unable to find cached %s" % parsed_url.netloc
    sys.exit(2)

origin_cached = htcondor.Cached(cacheds[0])

caches = origin_cached.listCacheDirs("", "Name == %s" parsed_url.path)

if len(caches) != 1:
    print "Unable to find single cache %s" % parsed_url.path
    sys.exit(3)
    
