#!/usr/bin/perl

for ($nf = 0; $nf <= $#ARGV; $nf++) {

    @sum[$nf]=();
    $num[$nf] = 0;

    open(DATA, "<$ARGV[$nf]");

    while ( $line = <DATA> ) {
	@field = split '\|', $line;
	@node = split '\t', $field[0];
        #@data = split ' ', $field[0];
        # @data = split ' ', $line;

	#printf "%d\n", $#node;

	for ($i = 0; $i <= $#node; $i++ ) {
	    @data = split ' ', $node[$i];

	    #printf "%d\n", $data[0];
	    $sum[$nf][$i] += $data[0];
	}
	$num[$nf]++;
    }
    close(DATA);

}

for ($i = 1; $i <= $#node ; $i++ ) {
    # printf "%2d", $i;
    printf "%d", $i;
    for ($nf = 0; $nf <= $#ARGV; $nf++) {
	printf " %8.2f", ($sum[$nf][$i]) / $num[$nf];
    }
    printf " 361600\n";
}
