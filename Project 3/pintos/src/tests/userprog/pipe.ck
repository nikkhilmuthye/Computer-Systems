# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(pipe) begin
(pipe) Hello on the other side of the pipe!
pipe: exit(0)
pipe: exit(0)
EOF
pass;
