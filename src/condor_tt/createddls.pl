#!/usr/bin/perl -w

use Getopt::Long;

$DEFAULT_DDL_FILE = 'common_ddl';

GetOptions ('f=s' => \@f,   #specify files to use, defaults to $DEFAULT_DDL_FILE
            'p' => \$p,     #p,m,o specify output format (postgres, mysql, and oracle respectively)
                            #   only use one at a time
                            #   defaults to postgres
            'm' => \$m,
            'o' => \$o,
            'c' => \$c,     # generate create ddl
            'd' => \$d);    # generate drop ddl

$PGSQL = 1;
$MYSQL = 2;
$ORACLE = 3;
$format = $PGSQL;
if ($p) { $format = $PGSQL; }
if ($m) { $format = $MYSQL; }
if ($o) { $format = $ORACLE; }


$CREATE = 1;
$DROP = 2;
$mode = $CREATE;
if ($c) { $mode = $CREATE; }
if ($d) { $mode = $DROP; }

#all files are concatenated together in the order specified on the command line
if (! @f) {
    @f = ($DEFAULT_DDL_FILE);
}
@input = ();
foreach $file (@f) {
    handleInclude($file);
}

#if the last line returned from nextLine was a comment it will be set to 1
$lastLineWasComment = 0;
#special globals to control how nextLine/readUntil work
$noParse = 0;
$ignoreEND = 0;
%defines = ();

#parser main loop
#   note that not all lines will pass through this loop, because
#   lines are shift()'ed from @input by other subs
while ($line = nextLine()) {
    if ($line =~ /^\s*TABLE/) {
        if ($mode == $CREATE) {
            handleTable();
        } else {
            handleDropTable();
        }
    } elsif ($line =~ /^\s*INDEX/) {
        if ($mode == $CREATE) {
            handleIndex();
        }
    } elsif ($line =~ /^\s*INSERT/) {
        if ($mode == $CREATE) {
            handleInsert();
        } else {
            $noParse = 1;
            readUntil("END INSERT","END INSERT not found");
            $noParse = 0;
        }
    } elsif ($line =~ /^\s*SEQUENCE/) {
        if ($mode == $CREATE) {
            handleSequence();
        } else {
            handleDropSequence();
        }
    } elsif ($line =~ /^\s*DELETE/) {
        if ($mode == $CREATE) {
            handleDelete();
        }
    } elsif ($line =~ /^\s*#END/) {

    } else {
        error("unrecognized command: $line");
    }
}



#outputs ddl for a table
#   at end of routine top of @input will be the line after END TABLE
sub handleTable {
    if ($line =~ /^\s*TABLE\s+(.+)\s*/) {
        $table_name = $1;

        if ($line =~ /^\s*TABLE\s+(.+)\s+CASCADE\s*/) {
            $table_name = $1;
        }

        print "CREATE TABLE $table_name (\n";
        $found = 0;
        $prevCol = 0;
        $primaryKey = "";
        $prepend = ''; #don't prepend anything if the comment is on the first line
        $ignoreEND = 1;
        while ($line = nextLine($prepend)) {
            if ($line =~ /^\s*END TABLE/) {
                $found = 1;
                print "$primaryKey\n);\n";
                last;
            } elsif ($line =~ /^\s*PRIMARY KEY/i) {
                chomp $line;
                $primaryKey = ",\n$line";
            } else {
                if ($prevCol && !$lastLineWasComment) {
                    print ",\n"
                }
                handleCol();
                $prevCol = 1;
            }
            $prepend = ",\n";
        }
        $ignoreEND = 0;
        if (!$found) {
            error("no END TABLE for $table_name");
        }
    } else {
        error("no name for table specified: $line");
    }
}

sub handleDropTable {
    if ($line =~ /^\s*TABLE\s+(.+)\s*/) {
        $table_name = $1;

        if ($line =~ /^\s*TABLE\s+(.+)\s+CASCADE\s*/) {
            $table_name = $1;
            if ($format == $ORACLE) {
                print "DROP TABLE $table_name CASCADE CONSTRAINTS;\n";
            } elsif ($format == $PGSQL) {
                print "DROP TABLE $table_name CASCADE;\n";
            }
        } else {
            print "DROP TABLE $table_name;\n";
        }
        $noParse = 1;
        readUntil("END TABLE","no END TABLE for $table_name");
        $noParse = 0;
    } else {
        error("no name for table specified: $line");
    }
}

#outputs column definition
#   only reads one line
sub handleCol {
    @fields = split(/\s+/, $line);
    if (scalar(@fields) >= 2) {
        $col_name = shift @fields;
        $col_type = join(' ',@fields);

        if ($format == $ORACLE) {
            if ("$col_type" eq "text") {
                $col_type = 'clob';
            }
        } elsif ($format == $MYSQL) {
            #mysql doesn't support storing time zone with timestamp
            if ("$col_type" =~ /with time zone/i) {
                $col_type = $fields[0];
            } 
        } 
        print "$col_name $col_type";
    } else {
        error("improperly formatted column definition: $line");
    }
}

#outputs INDEX definition
#   only reads one line
sub handleIndex {
    if ($line =~ /^\s*INDEX\s+(.+)\s+ON\s+(.+)\((.+)\)\s*/) {
        print "CREATE INDEX $1 ON $2($3);\n";
    } else {
        error("invalid INDEX statement: $line");
    }
}

#outputs INSERT statement
#   at end of routine top of @input will be line after END INSERT
sub handleInsert {
    if ($line =~ /^\s*INSERT\s+(.+)\s*/) {
        $table_name = $1;
        $col_names = ' ';
        if ($line =~ /^\s*INSERT\s+(.+)\s+(\(.*\))/) { #column names are specified
            $table_name = $1;
            $col_names = " $2 ";
        } 
        $ignoreEND = 1;
        my @lines = readUntil("END INSERT","END INSERT not found for INSERT $table_name");
        $ignoreEND = 0;
        map { chomp; $line = $_; print "INSERT INTO $table_name".$col_names."values ($line);\n"; } @lines;
    } else {
        error("no table name specified for INSERT statement");
    }
}

#outputs SEQUENCE definition
#   only reads one line
sub handleSequence {
    if ($line =~ /^\s*SEQUENCE\s+(.+)\s*/) {
        $seq_name = $1;
        print "CREATE SEQUENCE $seq_name;\n";
    } else {
        chomp $line;
        error("no name for sequence specified: $line");
    }
}

#outputs drop statement for SEQUENCE
#   only reads one line
sub handleDropSequence {
    if ($line =~ /^\s*SEQUENCE\s+(.+)\s*/) {
        $seq_name = $1;
        print "DROP SEQUENCE $seq_name;\n";
    } else {
        error("no name for sequence specified: $line");
    }
}

#output DELETE statement to delete contents of a table
#   only reads one line
sub handleDelete {
    if ($line =~ /^\s*DELETE\s+(.*)\s*/) {
        print "DELETE FROM $1;\n";
    } else {
        error("invalid DELETE statement: $line");
    }
}

#TODO - add support for #ELSE
sub handleIf {
    #handles arbitrary perl expressions

    if ($line =~ /^\s*#IF\s+(.+)\s*/) {
        $conditional = $1;
        $conditional =~ s/MYSQL/($format==$MYSQL)/i;
        $conditional =~ s/PGSQL/($format==$PGSQL)/i;
        $conditional =~ s/ORACLE/($format==$ORACLE)/i;
        $conditional =~ s/CREATE/($mode==$CREATE)/i;
        $conditional =~ s/DROP/($mode==$DROP)/i;

        $skip = not eval $conditional;

        $found = 0;       
        if ($skip) {
            while ($line = shift(@input)) { 
                if ($line =~ /^\s*#END IF/) {
                    $found = 1;
                    last;
                }
            }
            if ($found == 0) {
                error("#END IF not found for #IF $conditional");
            }
        }
    } else {
        error("improperly formatted #IF statement: $line");
    }
}

#include the contents of a file
#   at the end of the routine, @input will be prepended with the contents of the included file
#   optional parameter is name of file to include
sub handleInclude {
    my $file = shift;
    if (! $file) {
        if ($line =~ /^\s*#INCLUDE\s+(.+)\s*/) {
            $file = $1;
        } else {
            error("invalid #INCLUDE statement: $line");
        }
    }
    open(TMP,"$file") or error("unable to open file: $file");
    @tmp = <TMP>;
    close(TMP);
    replaceMacros();
    @input = (@tmp,@input);
}

#add an entry to %defines (define a macro)
sub handleDefine {
    my @fields = split /\s+/, $line;
    if (scalar(@fields) == 3) {
        $defines{$fields[1]} = $fields[2];
        replaceMacros();
    } else {
        error("invalid #DEFINE statement: $line");
    }
}

#replace all occurrences of all macros in @input
sub replaceMacros {
    for $line (@input) {
        for (keys %defines) {
            $line =~ s/$_/$defines{$_}/g;
        }
    }
}

#echo everything exactly as is until #END NOPARSE
sub handleNoParse {
    $noParse = 1;
    my @lines = readUntil("#END NOPARSE", "#END NOPARSE not found for #NOPARSE");
    $noParse = 0;
    print @lines;
}



############################
####### Utility functions
############################


#return next line
#   will not return comments or blank lines to caller
#   if an argument is passed it will prepend it to the first comment to be printed
#   sets global $lastLineWasComment (cheap hack for handleTable formatting)
sub nextLine {
    $prependIfComment = shift || '';
    $eof = 0;
    $continue = 0;
    $lastLineWasComment = 0;

    if ($noParse) { return shift(@input); }

    do {
        $line = shift(@input);
        if ($line) {
            $continue = 1;
            if ($line =~ /^\s*#IF/) {
                handleIf();
            } elsif ($line =~ /^\s*#INCLUDE/) {
                handleInclude();
            } elsif ($line =~ /^\s*#NOPARSE/) {
                handleNoParse();
            } elsif ($line =~ /^\s*#END/) {
                if ($ignoreEND == 1) {
                    $continue = 1;
                } else {
                    $continue = 0; 
                }
            } elsif ($line =~ /^\s*#DEFINE/) {
                handleDefine();
            } elsif ($line =~ /^\s*#/) {
                #this form of comment gets ignored
                #actually ends up handling #END IF if the branch is taken
            } elsif ($line =~ /^\s*--/) {
                if ($mode == $CREATE) {
                    $lastLineWasComment = 1;
                    print "$prependIfComment$line";
                    $prependIfComment = '';
                }
            } elsif ($line =~ /^\s*$/) {
                if ($mode == $CREATE) {
                    print "\n";
                }
            } else {
                $continue = 0;
            }
        } else {
            $eof = 1;
        }
    } while ($continue && !$eof);

    if ($eof) {
        return 0;
    } else {
        return $line;
    }
}

#first parameter is line to search for
#second parameter (optional) is error message to print if not found
sub readUntil {
    $str = shift;
    $errMsg = shift;

    my @lines = ();
    $found = 0;
    while ($line = nextLine()) {
        if ($line =~ /^\s*$str/) {
            $found = 1;
            last;
        } else {
            push @lines, $line;
        }
    }
    if ($found == 0) {
        if ($errMsg) {
            error("$errMsg");
        } else {
            error("$str not found");
        }
    }
    return @lines;
}

#die and output optional error message (first parameter)
sub error {
    #TODO - output file and line number (#INCLUDE makes it a little interesting)
    $err = shift;
    die("error: $err\n");
}

