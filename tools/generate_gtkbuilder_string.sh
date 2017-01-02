#!/bin/bash

#
# This is an unsophisticated script to convert a glade gtkbuilder file into
# a file containing a c string that can be compiled with the rest of the
# code. It would probably be better to use the official method of using the
# glib-compile-resources tool but msys and cmake overcomplicate this.
#

script_dir=$( dirname $0 )

out_file="$script_dir/../src/interface.glade.c"

in_file="$script_dir/../res/interface.glade"


# Bail out if there's no input file
if ! [ -e $in_file ]
then
  echo "$in_file doesn't exist."
  exit 1
fi

# Repeat "-" 73 times
dashes=$( printf '%.0s-' {1..73} )


# Clobber/Overwrite the existing file
printf "" >| $out_file

# Output the header
printf "/*$dashes*\\"                                        >> $out_file
printf "\n  Generated file - DO NOT EDIT!\n"                 >> $out_file
printf "\n"                                                  >> $out_file
printf "    Use ../tools/generate_gtkbuilder_string.sh\n"    >> $out_file
printf "\\*$dashes*/\n"                                      >> $out_file
printf "\n\n"                                                >> $out_file


printf "const char* GLADE_STRING = \"" >> $out_file


#
# Process the input file via the "sed" program
#

# First processing command
#   This uses a substitute command s__match__replace__flag.
#   The delimiter (__ above) is set after the s so we are using |.
#   We match double quotes " and prepend an escape \ to them.

cmd_esc_dbl_quote='s|"|\\"|g'


# Second processing command
#   The command starts with an address specifier ending at !
#   followed by another substitute command.
#
#   The address specifier is negated (with the !) so finds lines matching
#   the regex and then excludes them from the following substitute command.
#
#   The address has the following format: /regex/flag where the default /
#   delimiter seperates the regex from the ! negation flag. The regex
#   matches either a line with only </interface> or a blank line (they are
#   combined with the \| ).
#
#   Finally the substitute command has the same format as the first command
#   but we match the entire contents of the line, and append a c string
#   continuation backslash to the line (the & charactor is a placeholder for
#   the match).

cmd_append_bslash='/^<\/interface>$\|^$/! s|.*|& \\|g'


# Third processing command
#   Again the command starts with an address specifier, this time targeting
#   the line with a </interface> on it. The substitute command appends a
#   double quote and semicolon to end the c string.

cmd_end_string='/^<\/interface>$/ s|.*|&\";|g'


# Run the sed program
sed -e "$cmd_esc_dbl_quote" \
    -e "$cmd_append_bslash" \
    -e "$cmd_end_string"    \
    $in_file >> $out_file

# No point continuing if there's an error
if ! [ $? -eq 0 ]; then
  echo "An error occured. Generation stopped."
  exit 1
fi


# Add the end of file comment
printf "\n/* END */" >> $out_file


echo "Sucessfully generated $out_file"
exit 0