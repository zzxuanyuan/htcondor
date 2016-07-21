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

cached = htcondor.Cached()
classad = cached.requestLocalCache(parsed_url.netloc, parsed_url.path)

print classad
