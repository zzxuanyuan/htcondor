#!/bin/env perl

package Common; 

require Exporter;

@ISA = qw(Exporter);      # Take advantage of Exporter's capabilities

use constant DOWN_STATUS   => 'Down';
use constant EXITING_EVENT => 'Exiting';

@EXPORT_OK = qw(DOWN_STATUS EXITING_EVENT);

1;
