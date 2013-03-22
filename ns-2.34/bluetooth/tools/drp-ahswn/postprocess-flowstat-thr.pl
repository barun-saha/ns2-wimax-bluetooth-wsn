#!/usr/bin/perl

for ($nf = 0; $nf <= $#ARGV; $nf++) {

    @sum[$nf]=();
    $num[$nf] = 0;

    open(DATA, "<$ARGV[$nf]");

    while ( $line = <DATA> ) {
	@field = split '\|', $line;
        #@data = split ' ', $field[0];
        # @data = split ' ', $line;

	for ($i = 0; $i <= $#field - 1; $i++ ) {
	    @data = split ' ',$field[$i+1];
	    $sum[$nf][$i] += $data[0] * 8;
	}
	$num[$nf]++;
    }
    close(DATA)

}

for ($i = 0; $i <= $#field - 1; $i++ ) {
    # printf "%2d", $i;
    printf "%d", $i;
    for ($nf = 0; $nf <= $#ARGV; $nf++) {
	# printf " %8.2f", ($sum[$nf][$i]+$sum[$nf][$i-1]) / $num[$nf] / 2;
	printf " %8.2f", ($sum[$nf][$i]) / $num[$nf];
    }
    printf " 361600\n";
}
